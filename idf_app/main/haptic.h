#ifndef RS520_HAPTIC_H
#define RS520_HAPTIC_H

#include "esp_err.h"

namespace rs520
{

/// Initialize DRV2605 haptic driver (I2C 0x5A, internal trigger, ERM library).
/// Must call i2c_master_Init() before this.
[[nodiscard]] esp_err_t haptic_init();

/// Fire a short "Strong Click" haptic pulse. Non-blocking.
/// Skips if previous effect still playing (GO=1).
void haptic_click();

}  // namespace rs520

#endif // RS520_HAPTIC_H
