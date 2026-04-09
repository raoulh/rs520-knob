# RS520-Knob Agent Guide

## Stack

- **MCU**: ESP32-S3 (Waveshare ESP32-S3-Knob-Touch-LCD-1.8)
- **Framework**: ESP-IDF v6.0
- **Language**: C++20
- **UI**: LVGL 9.x
- **Display**: 360×360 IPS LCD (SH8601 via QSPI)
- **Touch**: CST816 capacitive (I2C)
- **Encoder**: Incremental quadrature (GPIO 7/8)
- **Bridge**: Go (Docker) — WebSocket server ↔ RS520 HTTPS API
- **Target amp**: HiFi Rose RS520 (HTTPS :9283, push notifications :9284, media :8000)

## Agent Skills (Always Active)

| Skill | Purpose |
|-------|---------|
| `caveman full` | Terse communication |
| `cpp-pro` | C++20 idioms, RAII, modern patterns |
| `embedded-systems` | RTOS, bare-metal, firmware |
| `iot-engineer` | ESP-IDF, MQTT, protocols |
| `golang-pro` | Go bridge — concurrency, stdlib HTTP/WS, idiomatic patterns |
| `websocket-engineer` | Knob↔bridge WS transport, reconnect, binary frames |
| `code-reviewer` | Quality + security review |

## Agent Skills (On Demand)

| Skill | When to invoke |
|-------|----------------|
| `debugger` | Root cause analysis, embedded debugging, crash investigation |
| `performance-engineer` | LVGL FPS optimization, latency profiling, memory tuning |
| `build` | CMake/idf.py issues, Docker build, compilation problems |
| `deployment-engineer` | Docker deploy for bridge, CI/CD pipelines, firmware releases |
| `ui-designer` | LVGL UI layout for 360×360 round display, touch interactions |
| `document-writer` | API docs, architecture decisions, hardware reference updates |
| `caveman-commit` | Conventional commit messages |
| `caveman-review` | Terse PR review comments |
| `network-engineer` | WiFi config, mDNS discovery, network troubleshooting |
| `devops-engineer` | CI/CD automation, Docker Compose, monitoring |

## Essential Commands

```bash
# Build
cd idf_app && idf.py build

# Flash + Monitor
idf.py flash -p /dev/ttyUSB0 && idf.py monitor -p /dev/ttyUSB0

# Menuconfig
idf.py menuconfig

# Set target (do this once)
idf.py set-target esp32s3

# Clean rebuild (when sdkconfig.defaults change)
rm sdkconfig && idf.py build

# Full clean
idf.py fullclean && idf.py build
```

## sdkconfig Pitfall

`sdkconfig` (generated) overrides `sdkconfig.defaults`. After editing defaults:
```bash
rm sdkconfig
idf.py build
```
`idf.py reconfigure` and `idf.py fullclean` do NOT regenerate from defaults.

## Git Workflow (MANDATORY)

**NEVER push directly to main/master.** Always feature branches + PRs.

```bash
git checkout -b feature/short-description
# ... work ...
git add <files> && git commit -m "feat: description"
git push -u origin feature/short-description
gh pr create --fill
```

### Branch Naming

- `fix/` — Bug fixes
- `feature/` — New features
- `docs/` — Documentation only

### Merging & Releasing

**ALWAYS test before shipping.** Flash firmware, user verifies, then merge.

**ASK user before:** merging PRs, tagging releases, pushing tags.

## Safety Rules (Embedded)

1. **No `new`/`malloc` in runtime** — Pre-allocate at init, use pools/arenas
2. **No exceptions** — Compile with `-fno-exceptions`
3. **No RTTI** — Compile with `-fno-rtti`
4. **Stack overflow** — Size stacks conservatively, use `uxTaskGetStackHighWaterMark()`
5. **ISR safety** — Only ISR-safe FreeRTOS calls from ISR context (`FromISR` variants)
6. **Mutex all shared state** — FreeRTOS mutexes for cross-task access
7. **DMA buffers** — Must be in internal RAM (`MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL`)
8. **Watchdog** — Never block main task indefinitely

## Documentation Map

| Area | Path | When |
|------|------|------|
| **Meta** | `docs/meta/` | Project aims, roadmap, lessons |
| **Dev** | `docs/dev/` | Build, code style, hardware overview, RS520 API ref |
| **ESP** | `docs/esp/` | Hardware subsystems: display, touch, encoder, battery, fonts |
| **ESP hw-ref** | `docs/esp/hw-reference/` | Component datasheets: pin map, CST816, DRV2605, board |
| **LVGL** | `docs/lvgl/` | UI guidelines, memory, perf |

**When working on:**
- **UI changes** → `docs/esp/DISPLAY.md`, `docs/esp/FONTS.md`, `docs/lvgl/LVGL_GUIDELINES.md`
- **Input handling** → `docs/esp/ROTARY_ENCODER.md`, `docs/esp/TOUCH_INPUT.md`, `docs/esp/SWIPE_GESTURES.md`
- **Haptic feedback** → `docs/esp/hw-reference/drv2605.md`
- **Battery/power** → `docs/esp/BATTERY_MONITORING.md`, `docs/esp/hw-reference/battery.md`
- **WiFi/networking** → `docs/esp/NETWORK_IDENTITY.md`
- **Artwork rendering** → `docs/esp/hw-reference/image_render.md`, `docs/esp/hw-reference/COLORTEST_HELLOWORLD.md`
- **Pin assignments** → `docs/esp/hw-reference/HARDWARE_PINS.md`
- **Dual-chip** → `docs/esp/DUAL_CHIP_ARCHITECTURE.md`
- **Build/config** → `docs/dev/DEVELOPMENT.md`, `docs/dev/CODE_STYLE.md`
- **RS520 API** → `docs/dev/RS520_FULL_API_REF.md`

**Keeping docs current:** When you learn something new about the hardware (pin mappings, component behavior, timing), update the relevant file in `docs/esp/hw-reference/`.

## Architecture

```
ESP32-S3 Knob ──WebSocket──→ Go Bridge (Docker) ──HTTPS :9283──→ Rose RS520
                              ├─ notification listener :9284 ←── RS520 push
                              ├─ artwork proxy (resize 360×360)
                              └─ state cache (push diffs via WS)
```

- **Knob talks WebSocket only** — no TLS, no HTTP server, minimal overhead
- **Bridge handles RS520 complexity** — self-signed TLS, push notifications, artwork resize
- **RS520 API ref**: `docs/dev/RS520_FULL_API_REF.md` (113 endpoints, reverse-engineered)

## Task Management

GitHub Issues is source of truth. No Markdown TODOs.

```bash
gh issue list
gh issue create --title "Description"
gh issue close <number> -c "Done in #<PR>"
```

## Principles

- GitHub Issues for all tasks
- Test before merge, always
- **Always build after code changes** — run `idf.py build` and fix all errors before considering work done
- Keep docs current when learning hardware behavior
- RAII everywhere, zero runtime allocation
- Never assume anything, always ask when in doubt
