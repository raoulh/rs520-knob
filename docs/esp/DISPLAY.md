# Display Subsystem

Display driving for 360×360 IPS LCD using ESP-IDF and LVGL on the Waveshare ESP32-S3-Knob-Touch-LCD-1.8.

## Hardware Overview

| Component | Model | Interface | Notes |
|-----------|-------|-----------|-------|
| Display controller | SH8601 | QSPI (4-wire) | IPS LCD, 16-bit RGB565 |
| Resolution | 360×360 | — | Round display |
| Backlight | PWM-controlled | GPIO 47 | 8-bit brightness (0–255) via LEDC |

The SH8601 accepts pixel data over Quad SPI — 4 data lines simultaneously for faster transfers.

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Application (LVGL UI)                      │
│               Widgets, animations, touch handlers            │
└───────────────────────────┬─────────────────────────────────┘
                            │ lv_* API calls
┌───────────────────────────▼─────────────────────────────────┐
│                         LVGL 9.x                             │
│                (managed_components/lvgl)                      │
└───────────────────────────┬─────────────────────────────────┘
                            │ flush callback
┌───────────────────────────▼─────────────────────────────────┐
│                   Display Driver (C++)                        │
│            LVGL display driver + ESP-IDF glue                │
└───────────────────────────┬─────────────────────────────────┘
                            │ esp_lcd_panel_draw_bitmap()
┌───────────────────────────▼─────────────────────────────────┐
│                    ESP-IDF LCD API                            │
│                   (esp_lcd_panel_ops.h)                       │
└───────────────────────────┬─────────────────────────────────┘
                            │ SPI DMA transfer
┌───────────────────────────▼─────────────────────────────────┐
│                     SH8601 Display                           │
└─────────────────────────────────────────────────────────────┘
```

## Pin Mapping

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

## LVGL Integration

LVGL 9.x maintains an internal scene graph. When something changes, it marks dirty regions, renders to a draw buffer, then calls the flush callback to push pixels to hardware. This partial-rendering approach needs only a small buffer — not a full framebuffer.

We use `esp_lvgl_port` (Espressif component) to manage the LVGL task, tick timer, mutex, draw buffers, and flush callback. This eliminates manual boilerplate.

### Initialization via esp_lvgl_port

```cpp
// 1. Init LVGL port (creates task, tick timer, mutex)
const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
lvgl_port_init(&lvgl_cfg);

// 2. Init SPI bus + SH8601 panel (hardware)
// ... spi_bus_initialize(), esp_lcd_new_panel_sh8601() ...

// 3. Register display with esp_lvgl_port
const lvgl_port_display_cfg_t disp_cfg = {
    .io_handle = io_handle,
    .panel_handle = panel_handle,
    .buffer_size = 360 * 36,     // 1/10 of display
    .double_buffer = true,
    .hres = 360,
    .vres = 360,
    .color_format = LV_COLOR_FORMAT_RGB565,
    .rounder_cb = rounder_cb,    // 2-pixel alignment
    .flags = {
        .buff_dma = true,        // Internal DMA RAM
        .swap_bytes = true,      // LE → BE for SH8601
    },
};
lv_display_t* display = lvgl_port_add_disp(&disp_cfg);
```

**Critical**: `buff_dma = true` ensures internal DMA RAM. PSRAM breaks DMA transfers.

### What esp_lvgl_port handles

- LVGL tick timer (2ms period)
- Draw buffer allocation (double-buffered, DMA-capable)
- Flush callback with byte-swap (`swap_bytes = true`)
- Dedicated LVGL task with built-in mutex
- `lv_timer_handler()` loop

### Rounder Callback

SH8601 requires 2-pixel alignment for memory writes:

```cpp
static void rounder_cb(lv_event_t* e) {
    auto* area = static_cast<lv_area_t*>(lv_event_get_param(e));
    area->x1 &= ~1;     // Round down to even
    area->y1 &= ~1;
    area->x2 |= 1;      // Round up to odd
    area->y2 |= 1;
}
```

Without this, visual glitches appear when rendering to odd pixel boundaries.

## Color Configuration

| Setting | Value | Why |
|---------|-------|-----|
| Color depth | 16-bit RGB565 | Hardware native |
| Byte order | Big-endian on wire | ESP32 LE → `swap_bytes = true` in esp_lvgl_port |
| Pixel alignment | 2px | SH8601 requirement |
| LVGL format | `LV_COLOR_FORMAT_RGB565` | Standard format, flush handles byte swap |

### What Does NOT Work

- **`LV_COLOR_FORMAT_RGB565_SWAPPED` on display** — only valid for image descriptors, not display output format. Causes blank/black screen.

## Backlight Control

PWM via LEDC peripheral on GPIO 47:

```cpp
ledc_timer_config_t timer_conf = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .duty_resolution = LEDC_TIMER_8_BIT,  // 0–255
    .timer_num = LEDC_TIMER_0,
    .freq_hz = 5000,
};

ledc_channel_config_t channel_conf = {
    .gpio_num = 47,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel = LEDC_CHANNEL_0,
    .duty = 255,  // Full brightness
};
```

## Panel Configuration

```cpp
esp_lcd_panel_dev_config_t panel_config = {
    .reset_gpio_num = 21,
    .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
    .bits_per_pixel = 16,  // RGB565
    .vendor_config = &vendor_config,
};
esp_lcd_new_panel_sh8601(io_handle, &panel_config, &panel_handle);
```

The `vendor_config` contains SH8601-specific init commands — register writes for gamma, timing, and power.

## Memory Budget

| Resource | Location | Size |
|----------|----------|------|
| Draw buffers (×2) | Internal DMA RAM | ~25KB each |
| Artwork | PSRAM | 360×360×2 = ~253KB |
| Font data | Flash (rodata) | 10–20KB per size |
| Widget tree | Internal heap | Variable |

Internal DMA RAM is limited (~300KB total). Keep draw buffers minimal.

## FPS Target

- 30 FPS minimum for smooth UI
- Tick timer: 2ms period
- Task handler: call `lv_timer_handler()` every 5–10ms

## Related Docs

- [LVGL Guidelines](../lvgl/LVGL_GUIDELINES.md) — memory, performance, widget patterns
- [Hardware Pins](hw-reference/HARDWARE_PINS.md) — full GPIO map
- [Color Configuration](hw-reference/COLORTEST_HELLOWORLD.md) — byte order details
