# Lessons Learned

Capture learnings after each bug, gotcha, or non-obvious discovery.

## Format

```
### [Date] — [Short Title]
**Symptom**: What happened
**Root Cause**: Why it happened
**Fix**: What fixed it
**Prevention**: How to avoid next time
```

## Lessons

### ESP-IDF sdkconfig Regeneration
**Symptom**: New `sdkconfig.defaults` options ignored after build
**Root Cause**: `sdkconfig` (generated) has higher priority than `sdkconfig.defaults`
**Fix**: `rm sdkconfig && idf.py build`
**Prevention**: Always delete `sdkconfig` after editing defaults

### DMA Buffer Allocation
**Symptom**: Display corruption or crash during rendering
**Root Cause**: Draw buffers allocated in PSRAM instead of DMA-capable internal RAM
**Fix**: Use `MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL` for buffer allocation
**Prevention**: Always specify `MALLOC_CAP_DMA` for SPI/display buffers

### ESP-IDF v6.0 — C++ Designated Initializers Break with `-Werror=missing-field-initializers`
**Symptom**: Build fails with `-Werror=missing-field-initializers` and designator order errors for ESP-IDF/LVGL structs
**Root Cause**: ESP-IDF v6.0 enables `-Werror=missing-field-initializers` globally. C++20 designated initializers that skip fields trigger this. Additionally, struct layouts changed between IDF versions — field ordering must match the declaration exactly.
**Fix**: Use zero-init (`struct_t cfg = {};`) then set fields individually instead of designated initializers. This avoids both missing-field and field-order issues.
**Prevention**: Always prefer `= {}` + field assignment for ESP-IDF/third-party structs in C++. Only use designated initializers for our own simple structs.

### ESP-IDF v6.0 — Transitive Includes Removed
**Symptom**: `'gpio_num_t' does not name a type` in backlight.cpp, despite working before
**Root Cause**: ESP-IDF v6.0 tightened header includes. `driver/ledc.h` no longer transitively includes `driver/gpio.h`, so `gpio_num_t` is unavailable unless explicitly included.
**Fix**: Add `#include "driver/gpio.h"` explicitly
**Prevention**: Always include headers for every type you use. Never rely on transitive includes.

### ESP-IDF v6.0 — Third-party Macros Not C++20-Safe
**Symptom**: `SH8601_PANEL_IO_QSPI_CONFIG` macro causes `-fpermissive` error (int `-1` → `gpio_num_t` enum)
**Root Cause**: C macros from managed components use C idioms (`-1` as GPIO) that are invalid in strict C++20
**Fix**: Don't use the macro — replicate the config manually with proper casts (`static_cast<gpio_num_t>(-1)`)
**Prevention**: Inspect third-party C macros before using them in C++ code. Prefer manual struct init.

<!-- Add new lessons above -->
