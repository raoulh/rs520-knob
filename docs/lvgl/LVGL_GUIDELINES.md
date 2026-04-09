# LVGL 9.x Guidelines — ESP32-S3

> **See also:**
> - [Display Subsystem](../esp/DISPLAY.md) — SH8601 driver, flush callback, byte-swap
> - [Touch Input](../esp/TOUCH_INPUT.md) — CST816 LVGL integration
> - [Fonts](../esp/FONTS.md) — bitmap font strategy and generation
> - [Color Configuration](../esp/hw-reference/COLORTEST_HELLOWORLD.md) — RGB565 byte order
> - [Artwork Rendering](../esp/hw-reference/image_render.md) — album art pipeline

## Display Setup

| Param | Value | Why |
|-------|-------|-----|
| Resolution | 360×360 | SH8601 round LCD |
| Color depth | 16-bit RGB565 | Hardware native |
| Byte order | Big-endian on wire | ESP32 little-endian → swap in flush |
| Pixel alignment | 2px | SH8601 requirement |

## Double Buffering

Managed by `esp_lvgl_port`. Two DMA-capable internal RAM buffers configured via `lvgl_port_display_cfg_t`:

```cpp
const lvgl_port_display_cfg_t disp_cfg = {
    .io_handle = io_handle,
    .panel_handle = panel_handle,
    .buffer_size = 360 * 36,       // 1/10 of display
    .double_buffer = true,
    .hres = 360,
    .vres = 360,
    .color_format = LV_COLOR_FORMAT_RGB565,
    .rounder_cb = rounder_cb,
    .flags = {
        .buff_dma = true,          // Internal DMA RAM (NOT PSRAM)
        .swap_bytes = true,        // SH8601 big-endian RGB565
    },
};
lv_display_t* display = lvgl_port_add_disp(&disp_cfg);
```

**Critical**: `buff_dma = true` ensures internal DMA RAM. PSRAM breaks DMA.

## Memory Budget

| Resource | Location | Size |
|----------|----------|------|
| Draw buffers (×2) | Internal DMA RAM | ~25KB each |
| Artwork cache | PSRAM | 360×360×2 = ~253KB |
| Font data | Flash (rodata) | 10-20KB per size |
| Widget tree | Internal heap | Variable |

Internal DMA RAM limited (~300KB total). Keep draw buffers minimal.

## FPS Target

- 30 FPS minimum for smooth UI
- Tick timer + task handler managed by `esp_lvgl_port` (no manual setup needed)

```cpp
// Initialization (once in app_main)
const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
lvgl_port_init(&lvgl_cfg);
```

## Flush Callback

Handled automatically by `esp_lvgl_port` when `.flags.swap_bytes = true`.
Byte-swap (ESP32 LE → SH8601 BE) and `esp_lcd_panel_draw_bitmap` are done internally.

No custom `flush_cb` needed.

## Rounder Callback

SH8601 needs 2-pixel aligned regions:

```cpp
static void rounder_cb(lv_event_t* e) {
    auto* area = static_cast<lv_area_t*>(lv_event_get_param(e));
    area->x1 &= ~1;     // Round down to even
    area->y1 &= ~1;
    area->x2 |= 1;      // Round up to odd
    area->y2 |= 1;
}
```

## Touch Integration

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

## Style Guidelines

```cpp
// Use styles, not inline property setting
static lv_style_t style_title;
lv_style_init(&style_title);
lv_style_set_text_font(&style_title, &lv_font_montserrat_28);
lv_style_set_text_color(&style_title, lv_color_white());
lv_obj_add_style(label, &style_title, 0);

// Group related widgets
// Prefer lv_obj_set_flex_flow() over manual positioning
lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
lv_obj_set_flex_align(container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
```

## Thread Safety

LVGL not thread-safe. Use `esp_lvgl_port` lock/unlock for all `lv_*` calls from outside the LVGL task:

```cpp
lvgl_port_lock(0);  // 0 = wait forever
lv_label_set_text(label, "Hello");
lvgl_port_unlock();
```

Callbacks registered with LVGL (touch read, event handlers) run inside the LVGL task — no lock needed there.

## Common Pitfalls

| Problem | Cause | Fix |
|---------|-------|-----|
| Garbage pixels | Missing byte-swap | Set `.flags.swap_bytes = true` in display config |
| Frozen animations | esp_lvgl_port not initialized | Call `lvgl_port_init()` before `lvgl_port_add_disp()` |
| Crash on flush | Buffer in PSRAM | Use `MALLOC_CAP_DMA` |
| Visual glitches at edges | Odd pixel alignment | Add rounder callback |
| UI freeze | Blocking in LVGL task | Keep callbacks fast, offload work |
| Corrupt display on wake | Missing `disp_on_off(true)` before backlight | Panel on → then backlight |
