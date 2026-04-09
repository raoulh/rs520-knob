# Hardware Pin Configuration

GPIO pin assignments for the Waveshare ESP32-S3-Knob-Touch-LCD-1.8 (360×360).

## Display Pins (SH8601 via QSPI)

| Function | GPIO | Notes |
|----------|------|-------|
| LCD_CS | 14 | SPI Chip Select |
| LCD_PCLK | 13 | SPI Clock |
| LCD_DATA0 | 15 | QSPI Data 0 |
| LCD_DATA1 | 16 | QSPI Data 1 |
| LCD_DATA2 | 17 | QSPI Data 2 |
| LCD_DATA3 | 18 | QSPI Data 3 |
| LCD_RST | 21 | Reset line |
| BK_LIGHT | 47 | Backlight PWM (LEDC) |

## Rotary Encoder

| Function | GPIO | Notes |
|----------|------|-------|
| ENCODER_A (ECA) | 8 | Quadrature channel A, internal pull-up |
| ENCODER_B (ECB) | 7 | Quadrature channel B, internal pull-up |

**Note:** No push button — encoder is rotation-only. All button interactions use the touchscreen.

## Touch Controller (CST816)

| Function | GPIO | I2C Details |
|----------|------|-------------|
| SDA | 11 | I2C_NUM_0, pull-up enabled |
| SCL | 12 | I2C_NUM_0, pull-up enabled |

| Parameter | Value |
|-----------|-------|
| I2C Address | 0x15 (7-bit) |
| I2C Speed | 300 kHz |
| Resolution | 12-bit (0–4095 raw → 0–359 display) |

## Haptic Driver (DRV2605)

| Function | GPIO | I2C Details |
|----------|------|-------------|
| SDA | 11 | Shared with touch (I2C_NUM_0) |
| SCL | 12 | Shared with touch (I2C_NUM_0) |

| Parameter | Value |
|-----------|-------|
| I2C Address | 0x5A (7-bit) |

## Battery Monitoring (ADC)

| Function | GPIO | Notes |
|----------|------|-------|
| VBAT_ADC | 1 | ADC1_CH0, 12dB attenuation |

- Voltage divider: 2:1 (10kΩ / 10kΩ)
- ADC range: 0–3.3V (reads 0–2.1V after divider)
- Full charge: ~4.2V → ~2.1V at ADC
- USB power: ~4.9V → ~2.45V at ADC

## Inter-Chip UART (ESP32-S3 ↔ ESP32)

| Function | GPIO | Notes |
|----------|------|-------|
| S3 TX → ESP32 RX | 38 | UART1 |
| S3 RX ← ESP32 TX | 48 | UART1 |

Currently unused in rs520-knob (ESP32 runs factory firmware).

## GPIO Availability Summary

| GPIO | Status | Usage |
|------|--------|-------|
| 1 | **Used** | Battery ADC |
| 7 | **Used** | Encoder B |
| 8 | **Used** | Encoder A |
| 11 | **Used** | I2C SDA (touch + haptic) |
| 12 | **Used** | I2C SCL (touch + haptic) |
| 13 | **Used** | Display SCLK |
| 14 | **Used** | Display CS |
| 15 | **Used** | Display DATA0 |
| 16 | **Used** | Display DATA1 |
| 17 | **Used** | Display DATA2 |
| 18 | **Used** | Display DATA3 |
| 21 | **Used** | Display RST |
| 38 | **Reserved** | UART1 TX (inter-chip) |
| 47 | **Used** | Backlight |
| 48 | **Reserved** | UART1 RX (inter-chip) |
| Others | **Available** | Expansion |

## Strapping Pins (Caution)

- **GPIO 0**: Boot mode selection (keep floating or pulled high)
- **GPIO 45**: VDD_SPI voltage selection
- **GPIO 46**: Boot mode selection
