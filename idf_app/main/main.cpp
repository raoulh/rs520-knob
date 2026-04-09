#include "display_driver.h"
#include "touch_driver.h"
#include "backlight.h"
#include "encoder.h"
#include "haptic.h"
#include "progress_ui.h"

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

/// Task: reads encoder queue, updates progress UI + fires haptic click.
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

            // Get current value to check bounds before haptic
            int prev = rs520::progress_ui_get();
            int next = prev + delta;

            // Only act if within bounds (no haptic at limits)
            if (next < 0 || next > 100)
            {
                continue;
            }

            // Update UI (needs LVGL lock)
            lvgl_port_lock(0);
            rs520::progress_ui_adjust(delta);
            lvgl_port_unlock();

            // Haptic click for feedback
            rs520::haptic_click();
        }
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

    // Touch input + Progress bar UI
    lvgl_port_lock(0);
    ESP_ERROR_CHECK(rs520::touch_init());
    rs520::progress_ui_create();
    lvgl_port_unlock();

    // Smooth backlight fade-in (screen content already rendered)
    rs520::backlight_fade_in(255, 800);

    // Encoder → UI + haptic task
    xTaskCreate(encoder_task, "encoder", kEncoderTaskStack, nullptr,
                kEncoderTaskPrio, nullptr);

    ESP_LOGI(kTag, "Boot complete — progress bar + encoder active");
}
