#ifndef RS520_ARTWORK_UI_H
#define RS520_ARTWORK_UI_H

#include <cstddef>
#include <cstdint>

namespace rs520
{

/// Create full-screen artwork image widget (behind all other widgets).
/// Must be called inside LVGL lock, before other UI create calls.
void artwork_ui_create();

/// Update artwork with RGB565 little-endian pixel data (360×360).
/// Must be called inside LVGL lock.
void artwork_ui_set(const uint8_t* rgb565_le, size_t len);

/// Hide artwork (show black background).
/// Must be called inside LVGL lock.
void artwork_ui_clear();

}  // namespace rs520

#endif // RS520_ARTWORK_UI_H
