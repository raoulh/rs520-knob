# RS520-Knob

Battery-powered WiFi remote control for the **HiFi Rose RS520** amplifier, built around the Waveshare ESP32-S3-Knob-Touch-LCD-1.8. Rotary encoder for volume, 360×360 round touch display for transport controls and now-playing artwork, haptic feedback via DRV2605.

## Architecture

```
ESP32-S3 Knob ──WebSocket──▶ Go Bridge (Docker) ──HTTPS :9283──▶ Rose RS520
                              ├─ notification listener :9284 ◀── RS520 push
                              ├─ artwork proxy (resize 360×360)
                              └─ state cache (push diffs via WS)
```

The knob speaks **WebSocket only** — no TLS, no HTTP server, minimal overhead. A Go bridge running in Docker handles the RS520's self-signed TLS, push notifications, artwork resizing, and state caching.

## Hardware

| Component | Detail |
|-----------|--------|
| MCU | ESP32-S3 (dual-core, 16 MB flash, 8 MB PSRAM) |
| Display | 360×360 IPS LCD (SH8601 via QSPI) |
| Touch | CST816 capacitive (I2C) |
| Encoder | Incremental quadrature (GPIO 7/8) |
| Haptic | DRV2605 (I2C) |
| Battery | LiPo with charge monitoring |

## Project Structure

```
idf_app/          ESP32 firmware (ESP-IDF v6.0, C++20, LVGL 9.x)
bridge/           Go bridge — WebSocket ↔ RS520 HTTPS API
docs/             Hardware reference, API docs, dev guides
.devcontainer/    Devcontainer for ESP-IDF toolchain
```

## Bridge

### Run with Docker Compose (recommended)

```bash
cd bridge
RS520_HOST=192.168.1.50 docker compose up -d
```

### Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `RS520_HOST` | `192.168.30.135` | RS520 IP address |
| `RS520_PORT` | `9283` | RS520 HTTPS API port |
| `WS_PORT` | `8080` | WebSocket server port |
| `NOTIFY_PORT` | `9284` | Push notification listener port |
| `POLL_INTERVAL` | `5s` | State poll interval |

### Build from Source

```bash
cd bridge
go build -o bridge ./cmd/bridge
RS520_HOST=192.168.1.50 ./bridge
```

### Run Tests

```bash
cd bridge
go test ./... -v
```

## Firmware

### Prerequisites

- [ESP-IDF v6.0](https://docs.espressif.com/projects/esp-idf/en/release-v6.0/esp32s3/get-started/index.html) or the included devcontainer
- Target: ESP32-S3

### Using the Devcontainer (recommended)

Open the project in VS Code with the Dev Containers extension. The `.devcontainer/Dockerfile` includes ESP-IDF, LVGL tools, and all dependencies.

### Build

```bash
cd idf_app
idf.py set-target esp32s3   # once
idf.py build
```

### Flash

```bash
idf.py flash -p /dev/ttyUSB0
```

### Monitor

```bash
idf.py monitor -p /dev/ttyUSB0
```

### Flash + Monitor

```bash
idf.py flash -p /dev/ttyUSB0 && idf.py monitor -p /dev/ttyUSB0
```

### OTA Updates

The partition table includes dual OTA slots (`ota_0` / `ota_1`). Release builds produce an OTA-ready `.bin` file attached to [GitHub Releases](../../releases).

## CI/CD

| Workflow | Trigger | What |
|----------|---------|------|
| **Bridge** | Push/PR to `bridge/` | Run tests, build & push Docker image to `ghcr.io` |
| **IDF Build** | Push/PR to `idf_app/` | Build firmware in devcontainer |
| **Release** | Push `v*` tag | Build firmware, create GitHub Release with OTA binary |

## Contributing

1. Create a feature branch: `git checkout -b feature/short-description`
2. Commit with [Conventional Commits](https://www.conventionalcommits.org/): `feat:`, `fix:`, `docs:`
3. Open a PR — never push directly to master
4. Test before merge (flash firmware, verify behavior)
