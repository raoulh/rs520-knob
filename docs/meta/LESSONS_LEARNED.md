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

### Boot Time Optimization — Parallel WiFi + NVS Bridge Cache
**Symptom**: Device took ~10s from boot to usable UI (artwork displayed). WiFi blocked main thread (~1.2s), mDNS discovery added ~5s every boot, all init was sequential.
**Root Cause**: `wifi_connect()` blocked `app_main()` — display/I2C/UI init waited for WiFi. `bridge_discovery_init()` ran mDNS query every boot (5s timeout) even though bridge IP rarely changes.
**Fix**: Three changes reduced boot-to-usable from 10.6s → 3.6s (66% faster):
1. **Parallel WiFi** — `wifi_init()` moved before LVGL/I2C init, `wifi_connect()` runs in `net_task` (background FreeRTOS task). WiFi association overlaps with display init.
2. **NVS bridge cache** — Store bridge IP:port in NVS (`bridge` namespace, keys `host`/`port`) after successful mDNS discovery. On boot, try cached address first (2s WS connect timeout). If fails, clear cache and fall back to mDNS.
3. **mDNS timeout** — Reduced from 5s to 3s for fallback-only queries.
**Prevention**: Always profile boot timeline with log timestamps. Look for sequential blocking calls that can be parallelized. Cache network discovery results in NVS.

### Parallel Init Race — LVGL Widgets Not Yet Created
**Symptom**: When `net_task` runs WiFi + provisioning in parallel with main thread HW init, provisioning path calls `wifi_status_ui_show_provision()` before LVGL widgets exist → crash.
**Root Cause**: `wifi_connect()` with no NVS creds returns instantly (`ESP_ERR_NVS_NOT_FOUND`), faster than main thread can create UI widgets.
**Fix**: Event group sync gate (`s_boot_events` with `kBitUiReady`). Main thread sets bit after `lvgl_port_unlock()`. `net_task` waits for it before touching LVGL.
**Prevention**: When moving blocking init to background tasks, identify all LVGL calls in the new task and gate them on UI readiness. Use event groups or semaphores, not `vTaskDelay()` hacks.

<!-- Add new lessons above -->
