# Touch Input

Capacitive touch input via the CST816 controller on the Waveshare ESP32-S3-Knob-Touch-LCD-1.8.

## Hardware Overview

| Component | Model | Interface | Address |
|-----------|-------|-----------|---------|
| Touch controller | CST816 | I2C | 0x15 |
| I2C SDA | GPIO 11 | — | — |
| I2C SCL | GPIO 12 | — | — |
| I2C Speed | 300 kHz | — | — |

The CST816 is a single-point capacitive touch controller. It reports touch coordinates over I2C when the user touches the screen.

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    LVGL Input Processing                     │
│                (button callbacks, gestures)                  │
└───────────────────────────┬─────────────────────────────────┘
                            │ LV_INDEV_STATE_PRESSED/RELEASED
┌───────────────────────────▼─────────────────────────────────┐
│                  Touch Read Callback                         │
│                    (LVGL indev)                               │
└───────────────────────────┬─────────────────────────────────┘
                            │ CST816 I2C read
┌───────────────────────────▼─────────────────────────────────┐
│                    CST816 Driver                             │
│                  I2C register reads                           │
└───────────────────────────┬─────────────────────────────────┘
                            │ I2C master bus
┌───────────────────────────▼─────────────────────────────────┐
│                  CST816 Touch IC                             │
└─────────────────────────────────────────────────────────────┘
```

## I2C Bus Setup

The I2C bus is shared between the touch controller and haptic motor driver:

```cpp
i2c_master_bus_config_t i2c_bus_config = {
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .i2c_port = I2C_NUM_0,
    .scl_io_num = GPIO_NUM_12,
    .sda_io_num = GPIO_NUM_11,
    .glitch_ignore_cnt = 7,
    .flags = {.enable_internal_pullup = true},
};
i2c_new_master_bus(&i2c_bus_config, &bus_handle);
```

Devices on this bus:

- **CST816** (0x15) — Touch controller
- **DRV2605** (0x5A) — Haptic motor driver

## CST816 Register Map

Reading 7 bytes from register 0x00:

| Byte | Content |
|------|---------|
| 0 | Gesture ID (optional) |
| 1 | Touch event (0=down, 1=up, 2=contact) |
| 2 | Number of touch points (0 or 1) |
| 3 | X coordinate high nibble |
| 4 | X coordinate low byte |
| 5 | Y coordinate high nibble |
| 6 | Y coordinate low byte |

Coordinates are 12-bit values (0–4095), auto-scaled to 360×360.

## Reading Touch Coordinates

```cpp
auto read_coordinates(uint16_t& x, uint16_t& y) -> bool {
    uint8_t data[7]{};
    i2c_read(touch_handle, 0x00, data, 7);

    if (data[2] > 0) {  // touch_count
        x = (static_cast<uint16_t>(data[3] & 0x0F) << 8) | data[4];
        y = (static_cast<uint16_t>(data[5] & 0x0F) << 8) | data[6];
        return true;
    }
    return false;
}
```

## LVGL Integration

LVGL polls touch state through an input device callback:

```cpp
static void touch_read_cb(lv_indev_t* indev, lv_indev_data_t* data) {
    uint16_t x, y;

    if (read_coordinates(x, y)) {
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// Registration
auto* indev = lv_indev_create();
lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
lv_indev_set_read_cb(indev, touch_read_cb);
```

LVGL calls this at ~30Hz. The callback must check touch state and report coordinates + PRESSED/RELEASED.

## Touch Events in LVGL

| Event | When |
|-------|------|
| `LV_EVENT_PRESSED` | Finger touches widget |
| `LV_EVENT_PRESSING` | Finger still down (repeating) |
| `LV_EVENT_RELEASED` | Finger lifted inside widget |
| `LV_EVENT_CLICKED` | Full tap completed |
| `LV_EVENT_LONG_PRESSED` | Held for long-press timeout |
| `LV_EVENT_PRESS_LOST` | Finger dragged outside widget |

## Screen Area Guidance (360×360)

- **Top band (~25–60px)** — Source/zone name; tap for source picker
- **Center area** — Track title/artist; tap for play/pause
- **Bottom area** — Transport controls (play/pause, prev, next)
- **Edges** — Keep ~12–16px margin from circular edge for comfort

## Reset & Recovery

If the touch controller becomes unresponsive:

1. Pulse RST low (≥10ms)
2. Delay ≥50ms
3. Clear latched INT with an initial status read
4. Resume normal polling

## Important Notes

- **No physical buttons** — this encoder has NO push switch. All button interactions use the touchscreen
- Touch coordinates are already mapped to display space by the CST816
- Keep encoder and touch actions consistent — both should drive the same input event codes

## Related Docs

- [Swipe Gestures](SWIPE_GESTURES.md) — gesture recognition on top of touch
- [CST816 Hardware Reference](hw-reference/cst816d.md) — controller datasheet notes
- [Hardware Pins](hw-reference/HARDWARE_PINS.md) — I2C pin assignments
