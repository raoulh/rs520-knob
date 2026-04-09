#include "touch_driver.h"

#include "lcd_touch_bsp.h"
#include "esp_log.h"
#include "lvgl.h"

#include <cstdint>

namespace
{

constexpr const char* kTag = "touch";

static lv_indev_t* s_indev = nullptr;

// LVGL input device read callback
static void read_cb(lv_indev_t* indev, lv_indev_data_t* data)
{
    (void)indev;
    uint16_t x = 0;
    uint16_t y = 0;

    if (tpGetCoordinates(&x, &y))
    {
        data->point.x = static_cast<int32_t>(x);
        data->point.y = static_cast<int32_t>(y);
        data->state = LV_INDEV_STATE_PRESSED;
    }
    else
    {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

}  // namespace

namespace rs520
{

esp_err_t touch_init()
{
    ESP_LOGI(kTag, "Registering LVGL touch input device");

    s_indev = lv_indev_create();
    if (!s_indev)
    {
        ESP_LOGE(kTag, "lv_indev_create failed");
        return ESP_FAIL;
    }

    lv_indev_set_type(s_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(s_indev, read_cb);

    ESP_LOGI(kTag, "Touch input registered (CST816 @ I2C 0x15)");
    return ESP_OK;
}

lv_indev_t* touch_indev_get()
{
    return s_indev;
}

}  // namespace rs520
