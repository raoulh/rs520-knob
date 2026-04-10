#ifndef RS520_WIFI_STATUS_UI_H
#define RS520_WIFI_STATUS_UI_H

#include "wifi_manager.h"
#include <cstdint>

namespace rs520
{

/// Create WiFi status icon on active screen (top-right area).
/// Must be called inside lvgl_port_lock/unlock.
void wifi_status_ui_create();

/// Update WiFi status display. Safe to call from any context —
/// internally uses lv_async_call() for thread safety.
void wifi_status_ui_update(WifiState state, int8_t rssi);

/// Show provisioning overlay (SSID info + instructions).
/// Must be called inside lvgl_port_lock/unlock.
void wifi_status_ui_show_provision(const char* ap_ssid);

/// Hide provisioning overlay.
/// Must be called inside lvgl_port_lock/unlock.
void wifi_status_ui_hide_provision();

}  // namespace rs520

#endif // RS520_WIFI_STATUS_UI_H
