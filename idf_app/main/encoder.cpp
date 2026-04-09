#include "encoder.h"

#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"

#include <cstdint>

namespace
{

constexpr const char* kTag = "encoder";

// Hardware pins — quadrature encoder
constexpr gpio_num_t kGpioA = GPIO_NUM_8;
constexpr gpio_num_t kGpioB = GPIO_NUM_7;

// Polling / debounce
constexpr int kPollIntervalUs = 3000;  // 3ms
constexpr uint8_t kDebounceThreshold = 2;

// Queue depth — enough for fast rotation bursts
constexpr int kQueueDepth = 16;

struct EncoderState
{
    uint8_t debounce_a{};
    uint8_t debounce_b{};
    uint8_t level_a{1};  // Pull-up default = high
    uint8_t level_b{1};
    uint8_t prev_a{1};
    uint8_t prev_b{1};
};

static EncoderState s_state;
static QueueHandle_t s_queue = nullptr;
static esp_timer_handle_t s_timer = nullptr;

static void poll_cb(void* /*arg*/)
{
    // Read raw GPIO levels
    uint8_t raw_a = static_cast<uint8_t>(gpio_get_level(kGpioA));
    uint8_t raw_b = static_cast<uint8_t>(gpio_get_level(kGpioB));

    // Debounce channel A
    if (raw_a == s_state.level_a)
    {
        s_state.debounce_a = 0;
    }
    else
    {
        s_state.debounce_a++;
        if (s_state.debounce_a >= kDebounceThreshold)
        {
            s_state.level_a = raw_a;
            s_state.debounce_a = 0;
        }
    }

    // Debounce channel B
    if (raw_b == s_state.level_b)
    {
        s_state.debounce_b = 0;
    }
    else
    {
        s_state.debounce_b++;
        if (s_state.debounce_b >= kDebounceThreshold)
        {
            s_state.level_b = raw_b;
            s_state.debounce_b = 0;
        }
    }

    // Detect falling edges — this encoder moves one channel per detent:
    //   CW click:  only A falls (B stays 1)
    //   CCW click: only B falls (A stays 1)
    if (s_state.prev_a == 1 && s_state.level_a == 0)
    {
        rs520::EncoderDir dir = (s_state.level_b == 1)
            ? rs520::EncoderDir::kCW
            : rs520::EncoderDir::kCCW;

        xQueueSend(s_queue, &dir, 0);
    }
    else if (s_state.prev_b == 1 && s_state.level_b == 0)
    {
        rs520::EncoderDir dir = (s_state.level_a == 1)
            ? rs520::EncoderDir::kCCW
            : rs520::EncoderDir::kCW;

        xQueueSend(s_queue, &dir, 0);
    }

    s_state.prev_a = s_state.level_a;
    s_state.prev_b = s_state.level_b;
}

}  // namespace

namespace rs520
{

esp_err_t encoder_init()
{
    ESP_LOGI(kTag, "Initializing rotary encoder (A=GPIO%d, B=GPIO%d)", kGpioA, kGpioB);

    // Configure GPIO inputs with pull-up
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << kGpioA) | (1ULL << kGpioB);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK)
    {
        ESP_LOGE(kTag, "GPIO config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Create event queue (pre-allocated, static-sized)
    s_queue = xQueueCreate(kQueueDepth, sizeof(EncoderDir));
    if (!s_queue)
    {
        ESP_LOGE(kTag, "Queue create failed");
        return ESP_ERR_NO_MEM;
    }

    // Read initial levels
    s_state.level_a = static_cast<uint8_t>(gpio_get_level(kGpioA));
    s_state.level_b = static_cast<uint8_t>(gpio_get_level(kGpioB));
    s_state.prev_a = s_state.level_a;
    s_state.prev_b = s_state.level_b;

    // Start periodic polling timer
    esp_timer_create_args_t timer_args = {};
    timer_args.callback = poll_cb;
    timer_args.name = "enc_poll";

    ret = esp_timer_create(&timer_args, &s_timer);
    if (ret != ESP_OK)
    {
        ESP_LOGE(kTag, "Timer create failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_timer_start_periodic(s_timer, kPollIntervalUs);
    if (ret != ESP_OK)
    {
        ESP_LOGE(kTag, "Timer start failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(kTag, "Encoder ready (3ms poll, debounce=%d)", kDebounceThreshold);
    return ESP_OK;
}

QueueHandle_t encoder_queue()
{
    return s_queue;
}

}  // namespace rs520
