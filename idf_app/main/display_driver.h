#ifndef RS520_DISPLAY_DRIVER_H
#define RS520_DISPLAY_DRIVER_H

#include "esp_err.h"
#include "lvgl.h"

namespace rs520
{

/// Initialize SH8601 QSPI display hardware and register with esp_lvgl_port.
/// Must call lvgl_port_init() before this.
[[nodiscard]] esp_err_t display_init();

/// Get LVGL display handle (nullptr if not initialized).
lv_display_t* display_get();

}  // namespace rs520

#endif // RS520_DISPLAY_DRIVER_H
