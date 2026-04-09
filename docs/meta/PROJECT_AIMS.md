# Project Aims — rs520-knob

## Vision

Battery-powered WiFi remote control for **HiFi Rose RS520** amplifier. Rotary knob for volume, round touch display for transport controls and now-playing artwork. Instant-on WiFi, ultra-responsive UI, haptic feedback. A premium physical controller that feels better than a phone app.

## Goals

1. **Volume control via rotary encoder** — smooth, latency-free, haptic click per detent (DRV2605)
2. **Instant WiFi connection** — stored credentials, connect in <2s from wake
3. **Ultra-reactive UI** — LVGL ≥30 FPS, no visible lag on touch/encoder input
4. **Now-playing display** — album art (JPEG decode to 360×360), track info overlay
5. **Touch transport controls** — play/pause, next, previous, power on/off via on-screen buttons
6. **Haptic feedback** — DRV2605 buzzes on encoder detent + button press
7. **Battery management** — LiPo charge monitoring, low-battery warning, graceful shutdown
8. **Power saving** — deep sleep on idle timeout, light sleep between interactions, display off after N seconds
9. **RS520 control via Go bridge** — knob ↔ WebSocket ↔ Go bridge ↔ HTTPS/HTTP RS520 API (volume, transport, power, metadata, artwork)
10. **OTA firmware updates** — dual OTA partitions, update via WiFi

## Non-Goals

- **No Bluetooth audio** — this is a remote control, not an audio sink
- **No multi-room** — controls one RS520 only
- **No streaming** — no audio data passes through this device
- **No web UI** — all interaction is on-device (encoder + touch display)
- **No cloud dependency** — all communication is local network only
- **No direct RS520 HTTPS from ESP32** — bridge handles TLS complexity, cert pinning, notification server

## Communication Architecture

```
┌─────────────────┐  WebSocket   ┌────────────────────┐  HTTPS :9283   ┌──────────────┐
│  ESP32-S3 Knob  │─────────────→│  Go Bridge (Docker) │──────────────→│  Rose RS520  │
│  (LVGL + WiFi)  │←─────────────│                    │←──────────────│              │
│                 │  JSON msgs   │  - WS server       │  JSON POST   │  113 endpoints│
│                 │              │  - RS520 HTTPS cli  │               │              │
│                 │              │  - Notification srv │  HTTP :9284   │              │
│                 │              │    (receives push)  │←──────────────│  Push notifs │
│                 │              │  - State cache      │               │              │
│                 │              │  - Artwork proxy    │  HTTP :8000   │              │
│                 │              │    (resize+cache)   │──────────────→│  Media lib   │
└─────────────────┘              └────────────────────┘               └──────────────┘
```

### Why a Go Bridge?

| Problem | Bridge Solution |
|---------|-----------------|
| RS520 uses self-signed TLS (RSA-4096) | Bridge handles TLS + cert pinning — ESP32 avoids heavy TLS overhead |
| RS520 has no WebSocket support | Bridge exposes WebSocket to knob — persistent, low-latency, bidirectional |
| RS520 push notifications need HTTP server on client | Bridge runs notification listener on :9284, forwards via WS |
| Album art is full-size JPEG | Bridge resizes to 360×360 + caches — saves ESP32 PSRAM + bandwidth |
| Polling for state is wasteful | Bridge caches state, pushes diffs via WS — knob reacts instantly |
| ESP32 HTTP client is slow for HTTPS | Plain WebSocket from ESP32 = minimal overhead, <50 ms round-trip |

### Bridge Responsibilities

1. **WebSocket server** — accepts knob connections, JSON message protocol
2. **RS520 HTTPS client** — handles TLS 1.2 + self-signed cert (`rootCA.crt`)
3. **Notification receiver** — HTTP server on :9284, receives RS520 push events
4. **State cache** — polls `/get_current_state` + `/get_control_info`, pushes diffs to knob
5. **Artwork proxy** — fetches from `:8000/v1/albumarts/{id}`, resizes to 360×360 JPEG, caches
6. **mDNS discovery** — finds RS520 via `_http._tcp.local` (`roseHifi-*`)

### Knob ↔ Bridge Protocol (WebSocket JSON)

```jsonc
// Knob → Bridge
{"cmd": "volume", "value": 25}
{"cmd": "play_pause"}
{"cmd": "next"}
{"cmd": "prev"}
{"cmd": "power", "value": "toggle"}
{"cmd": "mute"}

// Bridge → Knob
{"evt": "state", "volume": 25, "mute": false, "playing": true, "title": "...", "artist": "..."}
{"evt": "volume", "value": 30}
{"evt": "artwork", "url": "/art/current.jpg"}  // or binary frame
{"evt": "connected", "device": "RoseSalon"}
```

### RS520 API Endpoints Used by Bridge

| Endpoint | Method | Purpose |
|----------|--------|---------|
| `/device_name` | POST | Ping / identify device |
| `/device_connected` | POST | Register bridge as controller |
| `/get_current_state` | POST | Full playback state (title, artist, position) |
| `/get_control_info` | GET | Volume, playback state, source |
| `/volume` | POST | Set volume (`{"volume": N}`) |
| `/mute.state.get` | POST | Get mute state |
| `/current_play_state` | POST | Transport: play/pause(17), next(18), prev(19) |
| `/remote_bar_order` | POST | System: power_onoff, mute, reboot, screen_onoff |
| `/v1/albumarts/{id}` | GET (:8000) | Album artwork |

## Target Hardware

| Component | Spec |
|-----------|------|
| **Board** | Waveshare ESP32-S3-Knob-Touch-LCD-1.8 |
| **MCU** | ESP32-S3 — dual-core LX7, 240 MHz |
| **Flash** | 16 MB |
| **PSRAM** | 8 MB (Octal SPI, 80 MHz) |
| **Display** | 1.8" 360×360 IPS LCD — SH8601 via QSPI |
| **Touch** | CST816 capacitive — I2C @ 0x15 |
| **Encoder** | Incremental quadrature — GPIO 7/8, with push button |
| **Haptic** | DRV2605 motor driver — I2C @ 0x5A |
| **Battery** | LiPo with on-board charge IC + voltage ADC |
| **Backlight** | PWM on GPIO 47 |

## Features Breakdown

### P0 — Must Have (Release Blocker)

- [ ] Display init (SH8601 QSPI) + LVGL rendering at ≥30 FPS
- [ ] Rotary encoder → volume control (debounced, smooth)
- [ ] DRV2605 haptic tick on encoder rotation
- [ ] WiFi auto-connect (NVS-stored credentials)
- [ ] WebSocket client to Go bridge (connect, reconnect, JSON protocol)
- [ ] RS520 control via bridge: volume get/set, transport (play/pause/next/prev), power on/off
- [ ] Touch buttons: play/pause, next, previous, power
- [ ] Volume arc/bar visualization on display
- [ ] Album art display (bridge-resized JPEG → decode → framebuffer)
- [ ] Battery level indicator on UI
- [ ] Deep sleep on idle, wake on encoder/touch
- [ ] Go bridge (Docker) — WS server, RS520 HTTPS client, notification listener, artwork proxy

### P1 — Should Have

- [ ] WiFi provisioning (SoftAP captive portal for first-time setup)
- [ ] OTA firmware updates
- [ ] Low-battery warning + auto-shutdown
- [ ] Backlight dimming on idle (PWM fade)
- [ ] Track title/artist text overlay on artwork

### P2 — Nice to Have

- [ ] Swipe gestures (next/prev track via swipe)
- [ ] Configurable idle timeout
- [ ] Encoder acceleration (fast spin = bigger volume jump)
- [ ] Boot animation

## Constraints

### Memory

| Resource | Location | Budget |
|----------|----------|--------|
| LVGL draw buffers (×2) | Internal DMA RAM | ~25 KB each (50 KB total) |
| Album art decoded | PSRAM | ≤253 KB (360×360×RGB565) |
| Font data | Flash (rodata) | 10–20 KB per size |
| Widget tree | Internal heap | Keep minimal |
| Total internal DMA | — | ~300 KB available — tight |

**Rule**: DMA buffers = `MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL`. PSRAM breaks DMA. Large assets (artwork, fonts) → PSRAM or flash.

### Performance

- LVGL target: **≥30 FPS** (partial rendering, double-buffered)
- LVGL tick: 2 ms period
- `lv_timer_handler()` every 5–10 ms
- SH8601 needs **2-pixel aligned** draw regions (rounder callback)
- Byte-swap needed: ESP32 little-endian → SH8601 big-endian (RGB565)

### Power

- Deep sleep current target: <10 µA (ESP32-S3 spec)
- Wake sources: encoder rotation (GPIO interrupt), touch (CST816 interrupt)
- Display off after configurable idle timeout
- WiFi disconnect before sleep, reconnect on wake (<2 s)

### Firmware Size

- Dual OTA partitions: 3 MB each (see partition table)
- SPIFFS: 1.9 MB for assets (artwork cache, web files)
- Stay within 3 MB app partition — strip debug, use `-Os`

## Decisions Already Made

| Decision | Choice | Rationale |
|----------|--------|-----------|
| **Framework** | ESP-IDF v6.0 | Latest stable, best ESP32-S3 support, C++20 |
| **Language** | C++20 | RAII, constexpr, type safety — no `new`/`malloc` at runtime |
| **UI toolkit** | LVGL 9.x | Industry standard embedded UI, hardware-accelerated |
| **Color depth** | 16-bit RGB565 | Hardware native for SH8601, halves memory vs RGB888 |
| **Rendering** | Partial + double-buffer | Best FPS/memory tradeoff on 360×360 |
| **No exceptions** | `-fno-exceptions` | Code size + deterministic timing |
| **No RTTI** | `-fno-rtti` | Code size savings |
| **No runtime alloc** | Pre-allocate at init | Deterministic, no fragmentation |
| **Haptic driver** | DRV2605 (I2C) | On-board, supports LRA/ERM, effect library |
| **Bridge language** | Go | Fast, single binary, excellent HTTP/WS stdlib, easy Docker deploy |
| **Bridge transport** | WebSocket (bridge↔knob) | Persistent, bidirectional, low-latency — RS520 has no native WS |
| **Bridge deploy** | Docker container | Runs on home server/NAS, always-on, no config needed on knob |
| **RS520 comms** | HTTPS :9283 + HTTP :9284 push | Reverse-engineered from Rose Connect app — self-signed TLS |
| **No direct TLS from ESP32** | Bridge handles it | ESP32 TLS stack too heavy for self-signed RSA-4096 + keeps firmware lean |
| **Task tracking** | GitHub Issues | Source of truth — no Markdown TODOs |
| **Git workflow** | Feature branches + PRs | CI on PR, never push to main directly |
