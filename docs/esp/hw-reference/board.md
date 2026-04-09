# Waveshare ESP32-S3-Knob-Touch-LCD-1.8 — Board Overview

## Summary

- **MCU:** ESP32-S3 dual-core Xtensa LX7 @ 240 MHz
- **Wireless:** 2.4 GHz Wi-Fi (802.11 b/g/n) + Bluetooth 5 (LE)
- **Memory:** 16 MB Flash + 8 MB PSRAM (octal) + 512 KB internal SRAM
- **Display:** 1.8" round IPS LCD, 360×360, SH8601 via QSPI
- **Touch:** CST816 capacitive via I2C
- **Input:** Rotary encoder (quadrature, no push button)
- **Haptic:** DRV2605 haptic driver (I2C, ERM/LRA motor)
- **Power:** LiPo battery support (800mAh) + USB-C charging
- **Second chip:** ESP32-WROOM (Classic BT + audio DAC, connected via UART)

## Key Specs

| Feature | Specification |
|---------|---------------|
| MCU | ESP32-S3-WROOM-1 (R8, dual-core) |
| Clock | Up to 240 MHz |
| Flash | 16 MB (quad SPI) |
| PSRAM | 8 MB (octal SPI, 80 MHz) |
| SRAM | 512 KB internal |
| Display | 360×360 IPS LCD, SH8601 controller |
| Display Interface | QSPI (4-wire SPI with quad data) |
| Touch | CST816, I2C @ 0x15 |
| Encoder | EC11-style quadrature (rotation only) |
| Haptic | DRV2605 @ 0x5A (ERM/LRA driver) |
| Battery | 800mAh LiPo (PH1.25 connector) |
| USB | USB-C (native USB for programming/serial + charging) |
| Second chip | ESP32-WROOM (Classic BT, PCM5100PWR DAC, UART link) |

## What Matters for RS520-Knob Firmware

### Display
- 360×360 SH8601 IPS LCD over QSPI
- Byte-swapped RGB565 (big-endian) — handled in LVGL flush callback
- PWM backlight control via GPIO 47

### Touch
- CST816 on I2C (GPIO 11/12, shared bus with DRV2605)
- 12-bit coordinate resolution, single touch
- LVGL pointer input device

### Input
- Rotary encoder on GPIO 7/8 for volume control
- **No physical buttons** — touch for transport, menus, play/pause

### Haptic Feedback
- DRV2605 on I2C (0x5A, same bus as touch)
- Short click per encoder detent
- Stronger buzz on play/pause, transport actions

### Memory
- 8 MB PSRAM for artwork cache, network buffers, large allocations
- DMA-capable internal RAM required for display transfers
- PSRAM allocations >16KB auto-placed via `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384`

### Battery
- LiPo monitoring via ADC1_CH0 (GPIO 1)
- 2:1 voltage divider (10kΩ/10kΩ)
- Brownout detection at 2.50V (Level 4)

## Related Docs

- [Pin assignments](HARDWARE_PINS.md)
- [CST816 touch controller](cst816d.md)
- [DRV2605 haptic driver](drv2605.md)
- [Rotary encoder](encoder.md)
- [Battery hardware](battery.md)
- [Dual-chip architecture](../DUAL_CHIP_ARCHITECTURE.md)
