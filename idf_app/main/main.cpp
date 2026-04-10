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

#include "i2c_bsp.h"
#include "lcd_touch_bsp.h"

#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#include <cstdint>

namespace
{

constexpr const char* kTag = "main";
constexpr int kEncoderTaskStack = 3072;
constexpr int kEncoderTaskPrio  = 5;

/// Backlight brightness for each battery state
constexpr uint8_t kBrightnessNormal   = 255;
constexpr uint8_t kBrightnessLow      = 128;  // 50%
constexpr uint8_t kBrightnessCritical = 64;   // 25%

/// Task: reads encoder queue, updates ghost arc + sends volume to bridge.
/// Runs forever. Blocks on queue (no busy-wait).
void encoder_task(void* /*arg*/)
{
    auto queue = rs520::encoder_queue();
    rs520::EncoderDir dir{};

    for (;;)
    {
        if (xQueueReceive(queue, &dir, portMAX_DELAY) == pdTRUE)
        {
            int delta = static_cast<int>(dir);  // +1 or -1

            // Get current target to check bounds before haptic
            int prev = rs520::progress_ui_get_target();
            int next = prev + delta;

            // Only act if within bounds (no haptic at limits)
            if (next < 0 || next > 100)
            {
                continue;
            }

            // Update ghost arc + show popup (needs LVGL lock)
            lvgl_port_lock(0);
            int actual = rs520::progress_ui_adjust(delta);
            lvgl_port_unlock();

            // Haptic click for feedback
            rs520::haptic_click();

            // Send to bridge (throttled, non-blocking)
            rs520::bridge_send_volume(actual);
        }
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
    rs520::status_bar_create();
    rs520::wifi_status_ui_create();
    rs520::battery_ui_create();
    rs520::connection_ui_create();
    lvgl_port_unlock();

    // Smooth backlight fade-in (screen content already rendered)
    rs520::backlight_fade_in(255, 800);

    // Encoder → UI + haptic task
    xTaskCreate(encoder_task, "encoder", kEncoderTaskStack, nullptr,
                kEncoderTaskPrio, nullptr);

    // Battery monitor (ADC1_CH0 on GPIO 1, samples every 30s)
    rs520::battery_on_state_change(battery_state_cb, nullptr);
    ESP_ERROR_CHECK(rs520::battery_init());

    // WiFi: init driver, then connect or provision
    ESP_ERROR_CHECK(rs520::wifi_init());

    // Bridge state callback (connection popup)
    rs520::bridge_on_state_change(bridge_state_cb, nullptr);

    esp_err_t wifi_ret = rs520::wifi_connect();
    if (wifi_ret == ESP_ERR_NVS_NOT_FOUND || wifi_ret == ESP_FAIL)
    {
        ESP_LOGW(kTag, "No WiFi credentials or connection failed — starting provisioning");
        ESP_ERROR_CHECK(rs520::provision_start());
        lvgl_port_lock(0);
        rs520::wifi_status_ui_show_provision(rs520::provision_ssid());
        lvgl_port_unlock();
    }

    // Start bridge mDNS discovery + WebSocket client
    // Works even before WiFi is fully connected — discovery task
    // waits for mDNS results which require network connectivity.
    ESP_ERROR_CHECK(rs520::bridge_discovery_init());

    ESP_LOGI(kTag, "Boot complete");
}
