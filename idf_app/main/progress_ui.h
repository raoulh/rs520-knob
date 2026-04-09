#ifndef RS520_PROGRESS_UI_H
#define RS520_PROGRESS_UI_H

#include "lvgl.h"
#include <cstdint>

namespace rs520
{

/// Create round progress bar UI on active screen.
/// Must be called inside lvgl_port_lock/unlock.
void progress_ui_create();

/// Set progress value (clamped 0–100).
/// Must be called inside lvgl_port_lock/unlock.
/// Returns actual value after clamping.
int progress_ui_set(int value);

/// Get current progress value (0–100).
int progress_ui_get();

/// Increment/decrement by delta. Clamps to [0, 100].
/// Returns actual value after clamping.
int progress_ui_adjust(int delta);

}  // namespace rs520

#endif // RS520_PROGRESS_UI_H
