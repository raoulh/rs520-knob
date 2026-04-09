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
├── docs/                 # Documentation
└── scripts/              # Build/flash helpers
```

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
