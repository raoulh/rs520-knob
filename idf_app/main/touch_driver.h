#ifndef RS520_TOUCH_DRIVER_H
#define RS520_TOUCH_DRIVER_H

#include "esp_err.h"
#include "lvgl.h"

namespace rs520
{

/// Initialize LVGL touch input device using CST816 via I2C BSP.
/// Registered manually since CST816 BSP doesn't use esp_lcd_touch interface.
/// Must call lvgl_port_init(), i2c_master_Init(), and lcd_touch_init() before this.
/// Must be called inside lvgl_port_lock/unlock.
[[nodiscard]] esp_err_t touch_init();

/// Get LVGL input device handle (nullptr if not initialized).
lv_indev_t* touch_indev_get();

}  // namespace rs520

#endif // RS520_TOUCH_DRIVER_H
