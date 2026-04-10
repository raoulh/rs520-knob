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

### ESP-IDF v6.0 — DNS Server Header Missing Include
**Symptom**: `'esp_ip4_addr_t' does not name a type` when including `dns_server.h` from C++
**Root Cause**: The ESP-IDF example `dns_server.h` relies on transitive includes via `esp_netif.h` in the `.c` file but doesn't include the IP addr type header itself. When included from C++ with strict includes, the type is unknown.
**Fix**: Add `#include "esp_netif_ip_addr.h"` to `dns_server.h`
**Prevention**: When copying ESP-IDF example components, check that headers are self-contained — add missing includes for all types used in the header.

### ESP-IDF v6.0 — DNS_SERVER_CONFIG_SINGLE Macro Triggers -Werror in C++
**Symptom**: `DNS_SERVER_CONFIG_SINGLE("*", "WIFI_AP_DEF")` fails with `-Werror=missing-field-initializers`
**Root Cause**: The macro uses C designated initializer syntax that doesn't zero-initialize all fields. ESP-IDF v6.0's `-Werror` treats this as an error in C++.
**Fix**: Don't use the macro — zero-init the struct with `= {}` then set fields individually.
**Prevention**: Same pattern as other ESP-IDF struct init: always `struct_t x = {};` then field assignment.

### ESP-IDF set-target Required After sdkconfig Delete
**Symptom**: Build targets wrong chip (esp32 instead of esp32s3) after `rm sdkconfig`
**Root Cause**: `rm sdkconfig` also loses the target setting. `idf.py build` defaults to esp32.
**Fix**: `rm sdkconfig && idf.py set-target esp32s3 && idf.py build`
**Prevention**: Always re-run `idf.py set-target esp32s3` after deleting sdkconfig. Consider a build script.

### WiFi Driver RAM Usage
**Symptom**: Need to plan memory budget when adding WiFi to an LVGL project
**Root Cause**: WiFi driver allocates ~70KB internal RAM at `esp_wifi_init()`
**Fix**: Reduce `STATIC_RX_BUFFER_NUM` from 10→6 and `DYNAMIC_RX_BUFFER_NUM` from 32→12 in sdkconfig.defaults. ESP32-S3 has 512KB internal — draw buffers (50KB) + WiFi (70KB) leaves ~390KB free.
**Prevention**: Always check `heap_caps_get_free_size(MALLOC_CAP_INTERNAL)` after WiFi init before adding more features.

### mDNS in ESP-IDF v6.0 is a Managed Component
**Symptom**: `#include "mdns.h"` not found with default ESP-IDF components
**Root Cause**: In IDF v6.0, mDNS was moved out of core into `espressif/mdns` managed component
**Fix**: Add `espressif/mdns: version: "*"` to `idf_component.yml`
**Prevention**: Check component availability in managed registry when porting from older IDF versions.

### ESP-IDF v6.0 — cJSON Is an External Managed Component
**Symptom**: `cJSON.h: No such file or directory` when building with IDF v6.0
**Root Cause**: cJSON was removed from ESP-IDF core in v6.0 and moved to `espressif/cjson` managed component
**Fix**: Add `espressif/cjson: "^1.7.18"` to `idf_component.yml` + `espressif__cjson` to `PRIV_REQUIRES` in CMakeLists.txt
**Prevention**: Never parse JSON manually — always use cJSON. When migrating to IDF v6.0, check all formerly-core components (mDNS, cJSON, etc.) and add as managed dependencies.

<!-- Add new lessons above -->
