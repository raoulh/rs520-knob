# Swipe Gesture Recognition

Software gesture detection on top of raw CST816 touch coordinates.

## Overview

The touch controller (CST816) reports X/Y positions; the firmware tracks start/end points and timing to detect swipe gestures for navigation (e.g., switching views, entering art mode).

## Gesture Parameters

```cpp
constexpr int kSwipeMinDistance = 60;     // Minimum pixels traveled
constexpr int kSwipeMaxTimeMs = 500;      // Maximum gesture duration
constexpr int kDoubleTapMaxMs = 400;      // Max time between taps
constexpr int kDoubleTapMaxDist = 40;     // Max distance between taps
```

A valid swipe must:

- Travel at least 60 pixels from start to end
- Complete within 500ms
- Move more in the primary direction than perpendicular

## State Machine

```
Touch Down (tracking=false)
    │
    ▼
Record start position and time
Set tracking=true
    │
    ▼
Touch Move (tracking=true)
    │ (nothing — check only on release)
    ▼
Touch Up (tracking=true)
    │
    ├── elapsed > kSwipeMaxTimeMs?  → Too slow, not a swipe
    │
    ├── distance < kSwipeMinDistance? → Too short, not a swipe
    │
    └── Check direction:
         ├── |dy| > |dx| and dy < 0? → Swipe Up
         ├── |dy| > |dx| and dy > 0? → Swipe Down
         ├── |dx| > |dy| and dx < 0? → Swipe Left
         └── |dx| > |dy| and dx > 0? → Swipe Right
```

## Implementation

Detection in the LVGL touch read callback:

```cpp
static void touch_read_cb(lv_indev_t* indev, lv_indev_data_t* data) {
    uint16_t x, y;

    if (read_coordinates(x, y)) {
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;

        if (!tracking_) {
            start_x_ = x;
            start_y_ = y;
            start_time_ = esp_timer_get_time() / 1000;  // µs → ms
            tracking_ = true;
        }
    } else {
        data->state = LV_INDEV_STATE_RELEASED;

        if (tracking_) {
            const int64_t elapsed = (esp_timer_get_time() / 1000) - start_time_;

            if (elapsed < kSwipeMaxTimeMs) {
                const int16_t dx = data->point.x - start_x_;
                const int16_t dy = data->point.y - start_y_;

                if (dy < -kSwipeMinDistance && abs(dy) > abs(dx)) {
                    pending_swipe_up_ = true;
                } else if (dy > kSwipeMinDistance && abs(dy) > abs(dx)) {
                    pending_swipe_down_ = true;
                }
            }
            tracking_ = false;
        }
    }
}
```

## Deferred Processing

Gestures set flags rather than acting immediately — the touch callback runs from LVGL's internal context, where calling display state functions directly could cause threading issues:

```cpp
void process_pending_gestures() {
    if (pending_swipe_up_) {
        pending_swipe_up_ = false;
        enter_art_mode();
    }
    if (pending_swipe_down_) {
        pending_swipe_down_ = false;
        exit_art_mode();
    }
}
```

## Supported Gestures

| Gesture | Action | Condition |
|---------|--------|-----------|
| Swipe Up | Enter art mode (fullscreen artwork) | dy < -60px, time < 500ms |
| Swipe Down | Exit art mode | dy > +60px, time < 500ms |
| Swipe Left | Next track (optional) | dx < -60px, time < 500ms |
| Swipe Right | Previous track (optional) | dx > +60px, time < 500ms |
| Double-tap | Enter art mode (alt) | 2 taps within 400ms, < 40px apart |
| Any tap | Exit art mode | (when in art mode) |

## Related Docs

- [Touch Input](TOUCH_INPUT.md) — touch hardware and LVGL integration
- [CST816 Reference](hw-reference/cst816d.md) — controller details
