#ifndef RS520_PROGRESS_UI_H
#define RS520_PROGRESS_UI_H

#include "lvgl.h"
#include <cstdint>

namespace rs520
{

/// Create round progress bar UI on active screen.
/// Must be called inside lvgl_port_lock/unlock.
void progress_ui_create();

/// Set confirmed volume from bridge. Deferred while encoder is active —
/// arc animates to the last received value after encoder settles.
/// Must be called inside lvgl_port_lock/unlock.
void progress_ui_set_confirmed(int value);

/// Hard-snap both confirmed and target to value (no tick, no defer).
/// For full state sync from bridge.
/// Must be called inside lvgl_port_lock/unlock.
/// Returns actual value after clamping.
int progress_ui_set_target(int value);

/// Get current target value (0–100). Used by encoder task for bounds.
int progress_ui_get_target();

/// Get current confirmed value (0–100).
int progress_ui_get_confirmed();

/// Increment/decrement target by delta. Clamps to [0, 100].
/// Shows tick on arc + popup. Defers incoming confirmed updates.
/// Returns actual value after clamping.
int progress_ui_adjust(int delta);

/// Snap both values (legacy alias for set_target).
/// Must be called inside lvgl_port_lock/unlock.
int progress_ui_set(int value);

/// Get current target value (legacy alias).
int progress_ui_get();

}  // namespace rs520

#endif // RS520_PROGRESS_UI_H
