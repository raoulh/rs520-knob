#ifndef RS520_METADATA_UI_H
#define RS520_METADATA_UI_H

namespace rs520
{

/// Create metadata display (title, artist, trackInfo, progress bar, time).
/// Must be called inside LVGL lock.
void metadata_ui_create();

/// Update track info labels. Pass nullptr or "" to hide a field.
/// Must be called inside LVGL lock.
void metadata_ui_set_track(const char* title, const char* artist, const char* track_info);

/// Update playback position. Both values in milliseconds.
/// Must be called inside LVGL lock.
void metadata_ui_set_position(int cur_ms, int dur_ms);

/// Show metadata container (call after first state event).
/// Must be called inside LVGL lock.
void metadata_ui_show();

/// Hide metadata container.
/// Must be called inside LVGL lock.
void metadata_ui_hide();

}  // namespace rs520

#endif // RS520_METADATA_UI_H
