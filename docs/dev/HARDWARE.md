# Hardware Reference — Waveshare ESP32-S3-Knob-Touch-LCD-1.8

> **Detailed docs:** For in-depth subsystem documentation, see:
> - [Display Subsystem](../esp/DISPLAY.md) — SH8601 QSPI, LVGL flush, byte-swap
> - [Touch Input](../esp/TOUCH_INPUT.md) — CST816 I2C integration
> - [Rotary Encoder](../esp/ROTARY_ENCODER.md) — quadrature decoding, debounce
> - [Battery Monitoring](../esp/BATTERY_MONITORING.md) — ADC, discharge curve
> - [Fonts](../esp/FONTS.md) — bitmap font strategy
> - [Swipe Gestures](../esp/SWIPE_GESTURES.md) — gesture detection
> - [Network Identity](../esp/NETWORK_IDENTITY.md) — mDNS, DHCP hostname
> - [Dual-Chip Architecture](../esp/DUAL_CHIP_ARCHITECTURE.md) — ESP32 + ESP32-S3
> - [Hardware Pins](../esp/hw-reference/HARDWARE_PINS.md) — complete GPIO map
> - [Board Overview](../esp/hw-reference/board.md) — board specs
> - [CST816 Reference](../esp/hw-reference/cst816d.md) — touch controller
> - [DRV2605 Reference](../esp/hw-reference/drv2605.md) — haptic driver
> - [Color Config](../esp/hw-reference/COLORTEST_HELLOWORLD.md) — RGB565 byte order
> - [Artwork Rendering](../esp/hw-reference/image_render.md) — album art pipeline

## Board Overview

| Spec | Value |
|------|-------|
| MCU | ESP32-S3 |
| Flash | 16MB |
| PSRAM | 8MB (Octal SPI, 80MHz) |
| Display | 1.8" 360×360 IPS LCD (SH8601) |
| Touch | CST816 capacitive |
| Encoder | Incremental quadrature + push button |
| Battery | LiPo charging + voltage monitoring |
| Motor | Vibration motor (DRV2605 haptic driver) |

## Pin Mapping

### Display (QSPI)

| Signal | GPIO |
|--------|------|
| SCLK | 13 |
| DATA0 | 15 |
| DATA1 | 16 |
| DATA2 | 17 |
| DATA3 | 18 |
| CS | 14 |
| RST | 21 |
| Backlight | 47 |

### Touch (I2C)

| Signal | GPIO | I2C Addr |
|--------|------|----------|
| SDA | 11 | 0x15 |
| SCL | 12 | — |

### Rotary Encoder

| Signal | GPIO | Pull |
|--------|------|------|
| Channel A | 8 | Up |
| Channel B | 7 | Up |
| Push Button | — | — |

### I2C Devices

| Device | Address | Purpose |
|--------|---------|---------|
| CST816 | 0x15 | Touch controller |
| DRV2605 | 0x5A | Haptic motor driver |

## Flash Layout

16MB flash with custom partition table for OTA:

```
# Name,    Type,  SubType,  Offset,   Size
nvs,       data,  nvs,      0x9000,   0x6000
phy_init,  data,  phy,      0xf000,   0x1000
otadata,   data,  ota,      0x10000,  0x2000
ota_0,     app,   ota_0,    0x20000,  0x300000
ota_1,     app,   ota_1,    0x320000, 0x300000
spiffs,    data,  spiffs,   0x620000, 0x1E0000
```

## PSRAM Config

```
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_SPIRAM_BOOT_INIT=y
CONFIG_SPIRAM_USE_MALLOC=y
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384
```

- Allocations <16KB → internal RAM (faster)
- Large buffers (artwork, fonts) → PSRAM
- DMA buffers MUST be internal RAM

## Brownout Detection

```
CONFIG_ESP_BROWNOUT_DET=y
CONFIG_ESP_BROWNOUT_DET_LVL_SEL_4=y
```

Level 4 (2.50V) — safe for battery operation. Default Level 7 (2.80V) triggers false brownouts during WiFi TX spikes.

## Display Details

- SH8601 controller via QSPI
- RGB565 format (16-bit, big-endian on wire)
- Requires 2-pixel alignment for memory writes
- Backlight: PWM via LEDC (GPIO 47, 0-255 duty)

## Encoder Details

- Quadrature decoding (poll at 3ms interval)
- Software debounce (2 consecutive stable reads)
- Internal pull-ups on both channels

## Useful Links

- [Waveshare Wiki](https://www.waveshare.com/wiki/ESP32-S3-Knob-Touch-LCD-1.8)
- [ESP-IDF v6.0 Docs](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)
- [LVGL 9.x Docs](https://docs.lvgl.io/9/)
