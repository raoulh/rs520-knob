#include "display_driver.h"
#include "touch_driver.h"
#include "backlight.h"
#include "encoder.h"
#include "haptic.h"
#include "progress_ui.h"
#include "status_bar.h"
#include "battery.h"
#include "battery_ui.h"
#include "wifi_manager.h"
#include "wifi_provision.h"
#include "wifi_status_ui.h"
#include "bridge_discovery.h"
#include "connection_ui.h"
#include "artwork_ui.h"
#include "metadata_ui.h"
#include "transport_ui.h"

#include "i2c_bsp.h"
#include "lcd_touch_bsp.h"

#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "lvgl.h"

#include <algorithm>
#include <cstdint>

namespace
{

constexpr const char* kTag = "main";
constexpr int kEncoderTaskStack = 3072;
constexpr int kEncoderTaskPrio  = 5;
constexpr int kNetTaskStack     = 4096;
constexpr int kNetTaskPrio      = 4;

/// Sync gate: net_task waits for UI init before touching LVGL widgets
constexpr int kBitUiReady = BIT0;
static EventGroupHandle_t s_boot_events = nullptr;

/// Backlight brightness for each battery state
constexpr uint8_t kBrightnessNormal   = 255;
constexpr uint8_t kBrightnessLow      = 128;  // 50%
constexpr uint8_t kBrightnessCritical = 64;   // 25%

/// Task: reads encoder queue, updates tick + sends volume to bridge.
/// Drains all pending events per wake-up for low-latency haptic.
void encoder_task(void* /*arg*/)
{
    auto queue = rs520::encoder_queue();
    rs520::EncoderDir dir{};

    for (;;)
    {
        // Block until at least one event
        if (xQueueReceive(queue, &dir, portMAX_DELAY) != pdTRUE)
            continue;

        int delta = static_cast<int>(dir);

        // Drain remaining queued events — accumulate total delta
        while (xQueueReceive(queue, &dir, 0) == pdTRUE)
        {
            delta += static_cast<int>(dir);
        }

        if (delta == 0) continue;

        // Clamp to bounds
        int prev = rs520::progress_ui_get_target();
        int next = std::clamp(prev + delta, 0, 100);
        delta = next - prev;
        if (delta == 0) continue;

        // Single UI update for accumulated delta
        lvgl_port_lock(0);
        int actual = rs520::progress_ui_adjust(delta);
        lvgl_port_unlock();

        // Single haptic click for the batch
        rs520::haptic_click();

        // Send to bridge (throttled, non-blocking)
        rs520::bridge_send_volume(actual);
    }
}

/// Bridge connection state callback — drives connection UI popup.
/// Called from WS event task — uses lv_async_call for thread safety.
struct BridgeUiMsg { rs520::BridgeState state; };

void bridge_state_cb(rs520::BridgeState state, void* /*ctx*/)
{
    // Allocate on heap for lv_async_call (freed in callback)
    auto* msg = new BridgeUiMsg{state};
    lv_async_call([](void* d) {
        auto* m = static_cast<BridgeUiMsg*>(d);
        switch (m->state)
        {
        case rs520::BridgeState::kSearching:
            rs520::connection_ui_show("Searching for\nRS520 Bridge...");
            break;
        case rs520::BridgeState::kConnecting:
            rs520::connection_ui_show("Connecting to\nbridge...");
            break;
        case rs520::BridgeState::kConnected:
            rs520::connection_ui_hide();
            break;
        case rs520::BridgeState::kDisconnected:
            rs520::connection_ui_show("Bridge disconnected\nReconnecting...");
            break;
        }
        delete m;
    }, msg);
}

/// Battery state change callback — called from battery monitor task.
/// Uses lv_async_call for UI, direct call for backlight (no LVGL).
void battery_state_cb(rs520::BatteryState state, int percentage, void* /*ctx*/)
{
    // Update battery UI (thread-safe via lv_async_call internally)
    rs520::battery_ui_update(state, percentage);

    // Adjust backlight based on battery state
    switch (state)
    {
    case rs520::BatteryState::kCritical:
        rs520::backlight_set(kBrightnessCritical);
        // Show warning overlay (once per transition)
        lv_async_call([](void* /*d*/) {
            rs520::battery_ui_show_warning();
        }, nullptr);
        break;

    case rs520::BatteryState::kLow:
        rs520::backlight_set(kBrightnessLow);
        lv_async_call([](void* /*d*/) {
            rs520::battery_ui_show_warning();
        }, nullptr);
        break;

    case rs520::BatteryState::kCharging:
    case rs520::BatteryState::kNormal:
        rs520::backlight_set(kBrightnessNormal);
        break;
    }
}

/// Task: WiFi connect + provisioning + bridge discovery.
/// Runs on a background task so HW init can proceed in parallel.
void net_task(void* /*arg*/)
{
    esp_err_t wifi_ret = rs520::wifi_connect();
    if (wifi_ret == ESP_ERR_NVS_NOT_FOUND || wifi_ret == ESP_FAIL)
    {
        ESP_LOGW(kTag, "No WiFi credentials or connection failed — starting provisioning");

        // provision_start() is WiFi-only (AP + HTTP), no LVGL needed
        ESP_ERROR_CHECK(rs520::provision_start());

        // Wait for UI widgets to be created before updating them
        xEventGroupWaitBits(s_boot_events, kBitUiReady, pdFALSE, pdTRUE, portMAX_DELAY);

        lvgl_port_lock(0);
        rs520::wifi_status_ui_show_provision(rs520::provision_ssid());
        lvgl_port_unlock();

        // Wait for WiFi to actually connect via provisioning before starting bridge
        ESP_LOGI(kTag, "Waiting for WiFi provisioning...");
        rs520::wifi_wait_connected(120000);  // 2 min timeout for user to provision
    }

    // Start bridge mDNS discovery + WebSocket client (needs WiFi)
    ESP_ERROR_CHECK(rs520::bridge_discovery_init());

    ESP_LOGI(kTag, "Network init complete");
    vTaskDelete(nullptr);
}

}  // namespace

extern "C" void app_main()
{
    ESP_LOGI(kTag, "rs520-knob v0.1.0 starting");

    // NVS (needed for WiFi later)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Boot sync event group
    s_boot_events = xEventGroupCreate();

    // WiFi: init driver early so association runs in parallel with HW init
    ESP_ERROR_CHECK(rs520::wifi_init());

    // Bridge state callback (connection popup) — register before net_task spawns
    rs520::bridge_on_state_change(bridge_state_cb, nullptr);

    // Spawn network task: wifi_connect + provisioning + bridge discovery
    // Runs in parallel with display/I2C/UI init below
    xTaskCreate(net_task, "net_init", kNetTaskStack, nullptr,
                kNetTaskPrio, nullptr);

    // Initialize esp_lvgl_port (creates LVGL task, tick timer, mutex)
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    // I2C bus (shared: touch @ 0x15, haptic @ 0x5A)
    ESP_LOGI(kTag, "Initializing I2C bus");
    i2c_master_Init();

    // Touch controller
    ESP_LOGI(kTag, "Initializing CST816 touch");
    lcd_touch_init();

    // Haptic motor driver (DRV2605)
    ESP_ERROR_CHECK(rs520::haptic_init());

    // Rotary encoder (GPIO 7/8, 3ms poll timer)
    ESP_ERROR_CHECK(rs520::encoder_init());

    // Backlight PWM (starts off)
    ESP_ERROR_CHECK(rs520::backlight_init());

    // Display (SH8601 QSPI + registered with esp_lvgl_port)
    ESP_ERROR_CHECK(rs520::display_init());

    // Touch input + Progress bar UI + Status bar + WiFi/Battery icons + Connection popup
    lvgl_port_lock(0);
    ESP_ERROR_CHECK(rs520::touch_init());
    rs520::artwork_ui_create();
    rs520::progress_ui_create();
    rs520::metadata_ui_create();
    rs520::transport_ui_create();
    rs520::status_bar_create();
    rs520::wifi_status_ui_create();
    rs520::battery_ui_create();
    rs520::connection_ui_create();
    lvgl_port_unlock();

    // Signal net_task that UI widgets are ready
    xEventGroupSetBits(s_boot_events, kBitUiReady);

    // Smooth backlight fade-in (screen content already rendered)
    rs520::backlight_fade_in(255, 800);

    // Encoder → UI + haptic task
    xTaskCreate(encoder_task, "encoder", kEncoderTaskStack, nullptr,
                kEncoderTaskPrio, nullptr);

    // Battery monitor (ADC1_CH0 on GPIO 1, samples every 30s)
    rs520::battery_on_state_change(battery_state_cb, nullptr);
    ESP_ERROR_CHECK(rs520::battery_init());

    ESP_LOGI(kTag, "Boot complete");
}
