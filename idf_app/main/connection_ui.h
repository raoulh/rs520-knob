#ifndef RS520_CONNECTION_UI_H
#define RS520_CONNECTION_UI_H

namespace rs520
{

/// Create connection popup (hidden initially).
/// Must be called inside lvgl_port_lock/unlock.
void connection_ui_create();

/// Show modal connection popup with message.
/// Must be called inside lvgl_port_lock/unlock.
void connection_ui_show(const char* msg);

/// Hide connection popup.
/// Must be called inside lvgl_port_lock/unlock.
void connection_ui_hide();

}  // namespace rs520

#endif // RS520_CONNECTION_UI_H
