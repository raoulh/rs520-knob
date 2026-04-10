#ifndef RS520_BATTERY_UI_H
#define RS520_BATTERY_UI_H

#include "battery.h"

namespace rs520
{

/// Create battery icon + percentage label as children of status bar.
/// Must be called inside lvgl_port_lock/unlock, after status_bar_create().
void battery_ui_create();

/// Update battery display. Safe to call from any task —
/// internally uses lv_async_call() for thread safety.
void battery_ui_update(BatteryState state, int percentage);

/// Show low-battery warning overlay (full screen).
/// Must be called inside lvgl_port_lock/unlock.
void battery_ui_show_warning();

/// Hide low-battery warning overlay.
/// Must be called inside lvgl_port_lock/unlock.
void battery_ui_hide_warning();

}  // namespace rs520

#endif // RS520_BATTERY_UI_H
