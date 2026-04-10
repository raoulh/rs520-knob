#ifndef RS520_BATTERY_H
#define RS520_BATTERY_H

#include "esp_err.h"
#include <cstdint>

namespace rs520
{

/// Battery charge state
enum class BatteryState : uint8_t
{
    kNormal,     ///< Battery OK (> 10%)
    kLow,        ///< Battery low (<= 10%)
    kCritical,   ///< Battery critical (<= 5%)
    kCharging,   ///< USB connected, charging
};

/// Callback for battery state transitions.
/// Called from battery monitor task — use lv_async_call() for LVGL updates.
using BatteryStateCallback = void (*)(BatteryState state, int percentage, void* ctx);

/// Initialize ADC1_CH0 (GPIO 1) for battery voltage monitoring.
/// Starts a background task that samples every 30 seconds.
[[nodiscard]] esp_err_t battery_init();

/// Read battery voltage in volts (3.0–4.2 on battery, ~4.9 on USB).
/// Returns 0.0f on error. Thread-safe.
float battery_voltage();

/// Battery percentage (0–100) interpolated from LiPo discharge curve.
/// Thread-safe.
int battery_percentage();

/// Current battery state. Thread-safe.
BatteryState battery_state();

/// True if USB/charging detected (voltage > 4.15V).
bool battery_is_charging();

/// Register callback for state changes. Only one callback supported.
void battery_on_state_change(BatteryStateCallback cb, void* ctx);

}  // namespace rs520

#endif // RS520_BATTERY_H
