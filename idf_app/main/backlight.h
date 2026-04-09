#ifndef RS520_BACKLIGHT_H
#define RS520_BACKLIGHT_H

#include "esp_err.h"
#include <cstdint>

namespace rs520
{

/// Initialize LEDC PWM backlight on GPIO 47. Starts at 0 (off).
[[nodiscard]] esp_err_t backlight_init();

/// Set backlight brightness (0 = off, 255 = full).
void backlight_set(uint8_t brightness);

/// Smooth fade from 0 to target brightness over duration_ms.
void backlight_fade_in(uint8_t target = 255, uint32_t duration_ms = 800);

}  // namespace rs520

#endif // RS520_BACKLIGHT_H
