#ifndef RS520_STATUS_BAR_H
#define RS520_STATUS_BAR_H

#include "lvgl.h"

namespace rs520
{

/// Create shared status bar container at bottom-center of active screen.
/// Must be called inside lvgl_port_lock/unlock, before wifi/battery UI create.
void status_bar_create();

/// Get status bar container (for adding child widgets).
/// Returns nullptr if not yet created.
lv_obj_t* status_bar_container();

}  // namespace rs520

#endif // RS520_STATUS_BAR_H
