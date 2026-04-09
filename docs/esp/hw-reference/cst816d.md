# CST816 Capacitive Touch Controller — Hardware Notes

App-facing reference for integrating the CST816 touch panel with ESP32-S3 / LVGL.

## Overview

- Single-touch, self-capacitive controller with gesture detection
- I2C interface, optional interrupt output, reset input
- Used on Waveshare ESP32-S3-Knob-Touch-LCD-1.8

## Electrical

| Parameter | Value |
|-----------|-------|
| Supply | 3.3V typical |
| I2C Address | 0x15 (7-bit) |
| I2C Speed | 100–400 kHz (we use 300 kHz) |
| SDA | GPIO 11 |
| SCL | GPIO 12 |

- Decoupling: 0.1µF + 1µF at controller
- Pull-ups: 4.7kΩ–10kΩ to 3.3V (internal pull-ups enabled in firmware)

## Reset Sequence

1. Drive RST low ≥10ms after power stable
2. Release RST high, delay ≥50ms before first I2C access
3. Clear latched INT with an initial status read

## Register Map (Touch Read)

Reading 7 bytes from register 0x00:

| Byte | Content |
|------|---------|
| 0 | Gesture ID |
| 1 | Touch event (0=down, 1=up, 2=contact) |
| 2 | Finger count (0 or 1) |
| 3 | X high nibble (bits 11:8) |
| 4 | X low byte (bits 7:0) |
| 5 | Y high nibble (bits 11:8) |
| 6 | Y low byte (bits 7:0) |

Coordinates: 12-bit (0–4095), auto-scaled to 360×360 by controller.

## LVGL Integration

```cpp
static void touch_read_cb(lv_indev_t* indev, lv_indev_data_t* data) {
    uint16_t x, y;
    if (cst816_read(&x, &y)) {
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}
```

Register as `LV_INDEV_TYPE_POINTER`. LVGL polls at ~30Hz.

## Coordinate Mapping (360×360 Round LCD)

Depending on panel orientation, transforms may be needed:
- Swap X/Y
- Invert X and/or Y
- Scale raw → 0..359
- Circular mask (discard points outside radius for round display)

## Error Recovery

If I2C read fails or controller becomes unresponsive:
1. Pulse RST low (≥10ms)
2. Delay ≥50ms
3. Retry init + initial status read

After brownouts or deep-sleep, always run the reset sequence.

## Known Gotchas

- Shared I2C bus with DRV2605 (0x5A) — no address conflicts but mutual access must be serialized
- Self-capacitive sensors sensitive to EMC — keep FPC short, avoid routing high-speed signals under sensor
- Some modules have same default I2C address (0x15) — verify on your board
- Auto-sleep: controller may enter low-power; touch wakes it and asserts INT

## References

- CST816D Datasheet V1.3 (Waveshare mirror)
- [Touch Input](../TOUCH_INPUT.md) — firmware integration
- [Hardware Pins](HARDWARE_PINS.md) — pin map
