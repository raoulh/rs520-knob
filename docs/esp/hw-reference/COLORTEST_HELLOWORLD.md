# SH8601 QSPI Display — Color Configuration (LVGL 9)

Correct color configuration for the SH8601 controller on the Waveshare ESP32-S3-Knob-Touch-LCD-1.8.

## Problem

Colors appear shifted/wrong. Common symptoms:
- Red appears as a different color
- Green/blue channels seem swapped or mixed
- Test patterns show wrong colors

## Root Cause: Byte Order Mismatch

SH8601 QSPI expects **big-endian RGB565**, but:
- ESP32-S3 is **little-endian**
- LVGL 9 renders in native (little-endian) RGB565

When LVGL generates red (`0xF800`), stored as bytes `00 F8`. SPI sends bytes in order → display receives `00 F8` instead of `F8 00`.

## Solution: Byte Swap in Flush Callback

```cpp
static void flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    const int w = area->x2 - area->x1 + 1;
    const int h = area->y2 - area->y1 + 1;
    const int count = w * h;
    auto* pixels = reinterpret_cast<uint16_t*>(px_map);

    for (int i = 0; i < count; i++) {
        pixels[i] = __builtin_bswap16(pixels[i]);
    }

    esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1,
                              area->x2 + 1, area->y2 + 1, px_map);
    lv_display_flush_ready(disp);
}
```

## JPEG Decoder Configuration

Use little-endian output — flush callback swaps it:

```cpp
jpeg_dec_config_t cfg = DEFAULT_JPEG_DEC_CONFIG();
cfg.output_type = JPEG_PIXEL_FORMAT_RGB565_LE;  // Flush callback swaps
cfg.rotate = JPEG_ROTATE_0D;

img_dsc.header.cf = LV_COLOR_FORMAT_RGB565;  // Standard format
```

## Raw RGB565 Data

When generating raw pixel data (test patterns, artwork from bridge), use standard LVGL RGB565 — flush callback handles byte swap:

```cpp
uint16_t color = lv_color_to_u16(lv_color_make(0xFF, 0x00, 0x00));  // Red
// No manual swap needed
img_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
```

## What Does NOT Work

**`LV_COLOR_FORMAT_RGB565_SWAPPED` on display:**
```cpp
// DO NOT DO THIS — breaks LVGL rendering entirely (blank/black output)
lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565_SWAPPED);
```

`RGB565_SWAPPED` is only valid for image data descriptors, not as a display output format.

## Summary

| Setting | Value |
|---------|-------|
| LVGL display format | `LV_COLOR_FORMAT_RGB565` (standard) |
| JPEG decoder output | `JPEG_PIXEL_FORMAT_RGB565_LE` |
| Image descriptors | `LV_COLOR_FORMAT_RGB565` |
| Byte swap | In flush callback (`__builtin_bswap16`) |
| Display expects | Big-endian RGB565 |

## References

- [Display Subsystem](../DISPLAY.md) — full display integration
- [Hardware Pins](HARDWARE_PINS.md) — QSPI pin map
