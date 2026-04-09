# Font System

Font strategy for the RS520 knob's 360×360 round display.

## Approach: Pre-rendered Bitmap Fonts

Fonts are converted from TTF to LVGL bitmap format **at build time** using `lv_font_conv`. Runtime TrueType rendering (TinyTTF) is unstable on ESP32-S3 due to memory constraints:

- Glyph rasterization needs 10–30KB heap per glyph at 36–40px
- LVGL's internal heap is limited (~170KB max in internal SRAM)
- PSRAM latency causes watchdog timeouts during rendering
- Crash: `assert failed: stbtt__new_active` — allocation failure

### Bitmap Benefits

- **Zero runtime memory pressure** — glyphs are pre-rendered
- **Predictable performance** — no rasterization delays
- **Reliable** — eliminates crash-prone code path

### Tradeoffs

- Larger flash usage (~1.5MB for all sizes vs ~1MB TTF)
- Fixed sizes — must choose at build time
- Limited Unicode — specify character ranges at build time

## Recommended Font Sizes

| Purpose | Size | Font |
|---------|------|------|
| Metadata (artist, album) | 22px | Lato or similar |
| Content (track title, volume) | 28px | Noto Sans or similar |
| Icons (transport controls) | 22/44/60px | Material Symbols |

## Unicode Coverage

### Text Fonts

Essential ranges for music metadata:

- Basic Latin (U+0020–007F) — ASCII
- Latin-1 Supplement (U+00A0–00FF) — Western European
- Latin Extended-A (U+0100–017F) — Central European
- Latin Extended-B (U+0180–024F) — Additional Latin
- General Punctuation (U+2010–2027) — dashes, quotes, ellipsis

### Icon Font

Transport and status icons:
- Media: play, pause, skip, stop, shuffle, repeat
- Volume: up, down, mute
- Network: WiFi, signal strength
- Status: check, close, error, warning, power

## Generating Fonts

```bash
npm install -g lv_font_conv

# Example: generate Noto Sans 28px with extended Latin
lv_font_conv \
    --bpp 4 \
    --size 28 \
    --font NotoSans-Regular.ttf \
    --range 0x20-0x7F,0xA0-0xFF,0x100-0x17F,0x2010-0x2027 \
    --format lvgl \
    --output notosans_28.c
```

## Memory Usage

| Component | Approximate Size |
|-----------|-----------------|
| Text font 22px | ~600KB |
| Text font 28px | ~1.0MB |
| Icon font (all sizes) | ~100KB |

Fonts live in flash (rodata) — no RAM impact.

## Future: Language Packs

For CJK, Arabic, or extended Cyrillic:

1. Generate separate bitmap font files per script
2. Load conditionally based on detected metadata language
3. Or offer as build-time configuration options

## Related Docs

- [Display](DISPLAY.md) — display subsystem and LVGL setup
- [LVGL Guidelines](../lvgl/LVGL_GUIDELINES.md) — memory budget and performance
