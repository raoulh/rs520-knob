# Dual-Chip Architecture: ESP32 + ESP32-S3

The Waveshare ESP32-S3-Knob-Touch-LCD-1.8 board contains **two separate ESP32 chips** that communicate via UART. This enables capabilities not possible with either chip alone.

## Hardware Overview

| Chip | Model | Bluetooth | Primary Role |
|------|-------|-----------|--------------|
| ESP32-S3 | ESP32-S3-WROOM-1 (R8) | BLE 5.0 only | Display, Touch, WiFi, BLE HID |
| ESP32 | ESP32-WROOM | Classic BT + BLE | Audio DAC, AVRCP, A2DP |

## Why Two Chips?

The ESP32-S3 has superior graphics and GPIO but **lacks Classic Bluetooth**. The original ESP32 supports Classic Bluetooth (for AVRCP/A2DP audio profiles) but has weaker graphics.

- **ESP32-S3** → display rendering, touch, WiFi, BLE HID
- **ESP32** → Classic Bluetooth audio and DAC

## Inter-Chip Communication (UART)

| Chip | Direction | GPIO | Signal |
|------|-----------|------|--------|
| ESP32 | TX (out) | GPIO 23 | → S3 RX |
| ESP32 | RX (in) | GPIO 18 | ← S3 TX |
| ESP32-S3 | TX (out) | GPIO 38 | → ESP32 RX |
| ESP32-S3 | RX (in) | GPIO 48 | ← ESP32 TX |

> **Note**: GPIO pins verified via testing. The original GPIO17/18 assumption was incorrect — those pins are used by display QSPI (DATA2/DATA3). UART0 (GPIO1/3) cannot be used as it's the USB programming port.

**UART Configuration:**
- Baud rate: 1,000,000 (1 Mbps) for low latency
- 8N1 (8 data, no parity, 1 stop)
- No flow control
- Use UART1 on both chips (UART0 reserved for USB programming)

## USB Programming Switch

The board uses a **CH445P 4-SPDT analog switch** to share a single USB-C port between both chips:

1. Flip USB-C connector orientation to switch targets
2. CH445P routes USB to alternate chip
3. Use appropriate serial port for each

## ESP32 Peripherals

| Peripheral | Interface | Notes |
|------------|-----------|-------|
| Audio DAC (PCM5100PWR) | I2S | BCK, DIN, LRCK |
| Classic Bluetooth | BR/EDR | A2DP sink, AVRCP controller |
| Rotary encoder (secondary) | GPIO | Second encoder input |
| SD Card | SPI | External storage |

## ESP32-S3 Peripherals

| Peripheral | Interface | Notes |
|------------|-----------|-------|
| Display (SH8601) | QSPI | 360×360 IPS LCD |
| Touch (CST816) | I2C | Capacitive |
| Rotary encoder | GPIO 7/8 | Primary input |
| WiFi | 2.4GHz | 802.11 b/g/n |
| BLE 5.0 | LE only | No Classic BT |
| DRV2605 | I2C | Haptic driver |
| Battery ADC | GPIO 1 | Voltage monitoring |

## Relevance for RS520-knob

For this project, only the **ESP32-S3** is used. The ESP32 likely runs minimal USB-UART bridge firmware from the factory. The UART GPIOs (38, 48) on the S3 side are available but should be left unused unless inter-chip communication is implemented.

### Potential Future Use

The ESP32 could be reprogrammed to:
- Provide Classic Bluetooth audio output from the RS520
- Act as an A2DP sink for streaming audio
- Use the PCM5100PWR DAC for local audio playback

This would require custom firmware for the ESP32 chip and a communication protocol over UART.

## Related Docs

- [Hardware Pins](hw-reference/HARDWARE_PINS.md) — complete GPIO map
- [Board Overview](hw-reference/board.md) — board specs
