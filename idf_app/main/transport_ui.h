#ifndef RS520_TRANSPORT_UI_H
#define RS520_TRANSPORT_UI_H

namespace rs520
{

/// Create transport controls: Shazam button above metadata, prev/play-pause/next below.
/// Must be called inside LVGL lock, after metadata_ui_create().
void transport_ui_create();

}  // namespace rs520

#endif // RS520_TRANSPORT_UI_H
