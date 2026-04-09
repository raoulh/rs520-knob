# Development Guide

> **Hardware subsystem docs:** See `docs/esp/` for detailed docs on display, touch, encoder, battery, fonts, gestures, and networking. See `docs/esp/hw-reference/` for component datasheets and pin assignments.

## Stack

| Component | Version |
|-----------|---------|
| ESP-IDF | v6.0 |
| LVGL | 9.x |
| Language | C++20 |
| Target | ESP32-S3 |

## Prerequisites

1. [ESP-IDF v6.0](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/) installed (typically `~/esp/esp-idf`)
2. USB-C cable to Waveshare ESP32-S3-Knob-Touch-LCD-1.8

## Setup

```bash
export IDF_PATH=~/esp/esp-idf
source "$IDF_PATH/export.sh"

cd idf_app
idf.py set-target esp32s3   # Once
```

## Build & Flash

```bash
cd idf_app
idf.py build
idf.py flash -p /dev/ttyUSB0
idf.py monitor -p /dev/ttyUSB0    # Ctrl+] to exit
```

Combined:
```bash
idf.py flash monitor -p /dev/ttyUSB0
```

## Menuconfig

```bash
idf.py menuconfig
```

Navigate menus, save, rebuild. See sdkconfig pitfall below.

## sdkconfig Pitfall

`sdkconfig` overrides `sdkconfig.defaults`. After editing defaults:
```bash
rm sdkconfig
idf.py build
```

## Clean Build

```bash
idf.py fullclean && idf.py build
```

## Project Structure

```
rs520-knob/
├── idf_app/              # ESP-IDF project root
│   ├── main/             # Application source (C++20)
│   ├── components/       # Custom components
│   ├── sdkconfig.defaults
│   └── partitions.csv
├── bridge/               # Go bridge (WebSocket ↔ RS520 HTTPS)
│   ├── cmd/bridge/       # Entry point, config, graceful shutdown
│   ├── internal/
│   │   ├── rs520/        # RS520 HTTPS client (:9283)
│   │   ├── ws/           # WebSocket server (hub, handler, protocol)
│   │   ├── notify/       # Push notification listener (:9284)
│   │   ├── state/        # State cache (diff detection)
│   │   └── artwork/      # Artwork proxy (resize, RGB565, LRU cache)
│   ├── scripts/          # curl integration tests
│   ├── Dockerfile
│   ├── docker-compose.yml
│   └── Makefile
├── docs/                 # Documentation
└── scripts/              # Build/flash helpers
```

## Go Bridge

The Go bridge sits between the ESP32-S3 knob and the RS520 amplifier. The knob talks plain WebSocket to the bridge; the bridge handles HTTPS/TLS to the RS520.

### Prerequisites

- Go 1.22+ installed
- RS520 reachable on the network (default: `192.168.30.135`)

### Build & Run

```bash
cd bridge
go build -o bridge ./cmd/bridge

# Run with defaults (RS520 at 192.168.30.135)
./bridge

# Or override via env vars
RS520_HOST=192.168.1.50 WS_PORT=8080 ./bridge
```

### Docker

```bash
cd bridge
docker compose up --build
```

Requires `network_mode: host` so the RS520 can reach the notification listener on `:9284`.

### Config (env vars)

| Variable | Default | Description |
|----------|---------|-------------|
| `RS520_HOST` | `192.168.30.135` | RS520 IP address |
| `RS520_PORT` | `9283` | RS520 HTTPS port |
| `WS_PORT` | `8080` | WebSocket server port |
| `NOTIFY_PORT` | `9284` | Push notification listener port |
| `POLL_INTERVAL` | `5s` | Background state poll interval |

### Exposed Endpoints

| Endpoint | Description |
|----------|-------------|
| `ws://{bridge}:8080/ws` | WebSocket — knob connects here |
| `http://{bridge}:8080/art/current?id={artId}&format=jpeg\|rgb565` | Artwork proxy (resized 360×360) |
| `http://{bridge}:8080/health` | Health check (JSON) |

### Tests

```bash
cd bridge

# Unit tests (mocked RS520, no network needed)
go test ./... -v

# Lint
go vet ./...

# curl integration tests against real RS520
bash scripts/curl_integration_test.sh 192.168.30.135
```

### Connecting from idf_app

The ESP32 firmware connects to the bridge via plain WebSocket (no TLS):

```
ws://{bridge_ip}:8080/ws
```

**On connect**, the bridge sends two JSON messages:
1. Full state snapshot: `{"evt":"state", "volume":25, "mute":false, "playing":true, "title":"...", "artist":"...", ...}`
2. Connected confirmation: `{"evt":"connected", "device":"RS520"}`

**Commands** (knob → bridge, JSON text frames):

| Command | JSON | Effect |
|---------|------|--------|
| Set volume | `{"cmd":"volume","value":25}` | Sets RS520 volume (0–100) |
| Play/Pause | `{"cmd":"play_pause"}` | Toggles playback |
| Next track | `{"cmd":"next"}` | Skip forward |
| Prev track | `{"cmd":"prev"}` | Skip backward |
| Mute toggle | `{"cmd":"mute"}` | Toggles mute |
| Power toggle | `{"cmd":"power"}` | Toggles power on/off |

**Events** (bridge → knob, JSON text frames):

| Event | JSON fields | When |
|-------|-------------|------|
| `state` | `volume`, `mute`, `playing`, `title`, `artist`, `album`, `source`, `powerOn` | State changes (push or poll) |
| `volume` | `volume` | Volume changed |
| `artwork` | `url` | New artwork available (URL to fetch via HTTP) |
| `connected` | `device` | Bridge confirmed RS520 connection |
| `error` | `title` | Command error (bad JSON, unknown cmd) |

**Artwork**: When an `artwork` event arrives with a `url` like `/art/current?id=abc&format=jpeg`, the knob fetches it via HTTP GET from the bridge (same host, port 8080). For RGB565 raw pixels (zero decode on device), use `format=rgb565` — returns exactly 259,200 bytes (360×360×2, big-endian).

**Reconnection**: The knob should reconnect on disconnect with exponential backoff.

## Debugging

```bash
# Monitor with reset
idf.py monitor -p /dev/ttyUSB0

# Check stack usage
uxTaskGetStackHighWaterMark(NULL)

# Heap info
heap_caps_get_free_size(MALLOC_CAP_INTERNAL)
heap_caps_get_free_size(MALLOC_CAP_SPIRAM)
```

## Task Management

GitHub Issues only. No Markdown TODOs.

```bash
gh issue list
gh issue create --title "Description"
gh issue close <number>
```
