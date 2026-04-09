#include "backlight.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"

#include <cstdint>

namespace
{

constexpr const char* kTag = "backlight";
constexpr gpio_num_t kPinBkLight = GPIO_NUM_47;

}  // namespace

namespace rs520
{

esp_err_t backlight_init()
{
    ESP_LOGI(kTag, "Initializing backlight PWM (GPIO %d)", kPinBkLight);

    ledc_timer_config_t timer_cfg = {};
    timer_cfg.speed_mode      = LEDC_LOW_SPEED_MODE;
    timer_cfg.duty_resolution = LEDC_TIMER_8_BIT;
    timer_cfg.timer_num       = LEDC_TIMER_0;
    timer_cfg.freq_hz         = 5000;
    timer_cfg.clk_cfg         = LEDC_AUTO_CLK;

    esp_err_t ret = ledc_timer_config(&timer_cfg);
    if (ret != ESP_OK)
    {
        ESP_LOGE(kTag, "LEDC timer config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ledc_channel_config_t chan_cfg = {};
    chan_cfg.gpio_num   = kPinBkLight;
    chan_cfg.speed_mode = LEDC_LOW_SPEED_MODE;
    chan_cfg.channel    = LEDC_CHANNEL_0;
    chan_cfg.timer_sel  = LEDC_TIMER_0;
    chan_cfg.duty       = 0;
    chan_cfg.hpoint     = 0;

    ret = ledc_channel_config(&chan_cfg);
    if (ret != ESP_OK)
    {
        ESP_LOGE(kTag, "LEDC channel config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Install LEDC fade service (0 = no ISR alloc flags)
    ret = ledc_fade_func_install(0);
    if (ret != ESP_OK)
    {
        ESP_LOGE(kTag, "LEDC fade install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(kTag, "Backlight ready (8-bit PWM, 5kHz, fade enabled)");
    return ESP_OK;
}

void backlight_set(uint8_t brightness)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, brightness);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

void backlight_fade_in(uint8_t target, uint32_t duration_ms)
{
    ESP_LOGI(kTag, "Fade in to %u over %lu ms", target, static_cast<unsigned long>(duration_ms));
    ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, target, static_cast<int>(duration_ms));
    ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_WAIT_DONE);
}

}  // namespace rs520
