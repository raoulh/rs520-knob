#ifndef RS520_PROGRESS_UI_H
#define RS520_PROGRESS_UI_H

#include "lvgl.h"
#include <cstdint>

namespace rs520
{

/// Create round progress bar UI on active screen.
/// Must be called inside lvgl_port_lock/unlock.
void progress_ui_create();

/// Set confirmed volume (solid arc) — animates to position.
/// Driven by bridge WS events. Hides ghost arc when values match.
/// Must be called inside lvgl_port_lock/unlock.
void progress_ui_set_confirmed(int value);

/// Set target volume (ghost arc) — moves instantly, shows popup.
/// Driven by encoder. Shows volume popup with auto-hide timer.
/// Must be called inside lvgl_port_lock/unlock.
/// Returns actual value after clamping.
int progress_ui_set_target(int value);

/// Get current target value (0–100). Used by encoder task for bounds.
int progress_ui_get_target();

/// Get current confirmed value (0–100).
int progress_ui_get_confirmed();

/// Increment/decrement target by delta. Clamps to [0, 100].
/// Shows popup. Returns actual value after clamping.
int progress_ui_adjust(int delta);

/// Set progress value (legacy — sets both arcs, no popup).
/// Must be called inside lvgl_port_lock/unlock.
/// Returns actual value after clamping.
int progress_ui_set(int value);

/// Get current progress value (legacy — returns target).
int progress_ui_get();

}  // namespace rs520

#endif // RS520_PROGRESS_UI_H
