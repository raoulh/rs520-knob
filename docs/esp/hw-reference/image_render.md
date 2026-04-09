# Album Artwork Rendering — Notes

Delivering and rendering now-playing artwork on the ESP32-S3's 360×360 display.

## Constraints

| Resource | Limit | Notes |
|----------|-------|-------|
| PSRAM | 8 MB total | Large allocations OK, but contiguous limits apply |
| Internal RAM | ~300 KB | DMA buffers, widget tree |
| Network | WiFi local | Keep requests short-lived, responses < 64 KB ideal |
| CPU | Dual-core 240 MHz | JPEG decode OK with lightweight decoder |

## Transport: Go Bridge

The Go bridge proxies and resizes artwork from the RS520:

```
GET /art/current?width=360&height=360&format=rgb565
```

### Response Options

**Option A: Raw RGB565 (recommended)**
- `Content-Type: application/octet-stream`
- `Content-Length: 259200` (360 × 360 × 2 bytes)
- Bridge resizes + converts; zero decode on device
- Trade-off: ~260 KB transfer vs ~30–50 KB JPEG

**Option B: JPEG**
- `Content-Type: image/jpeg`
- `Content-Length: <size>` (must include, no chunked)
- Smaller transfer (~30–50 KB) but requires decode on device
- Use `esp_jpeg` component with `JPEG_PIXEL_FORMAT_RGB565_LE`

## RGB565 Flow (Zero Decode)

1. **Fetch** — HTTP GET returns 259,200 bytes from bridge
2. **Validate** — Check size = 360 × 360 × 2
3. **Copy** — `memcpy` to global PSRAM buffer
4. **Display** — LVGL image descriptor points to buffer
5. **Byte swap** — Flush callback converts to big-endian for SH8601

## JPEG Flow (Device Decode)

1. **Fetch** — HTTP GET returns JPEG (cap at ~128 KB)
2. **Decode** — `esp_jpeg` → RGB565 LE into PSRAM buffer
3. **Display** — LVGL image descriptor with `LV_COLOR_FORMAT_RGB565`
4. **Byte swap** — Same flush callback handles it

## LVGL Image Display

```cpp
static lv_img_dsc_t art_dsc = {
    .header = {
        .cf = LV_COLOR_FORMAT_RGB565,
        .w = 360,
        .h = 360,
    },
    .data_size = 360 * 360 * 2,
    .data = art_buffer,  // PSRAM
};

lv_img_set_src(art_widget, &art_dsc);
```

## Caching

- Store last artwork ETag per zone
- Send `If-None-Match` header → bridge returns 304 if unchanged
- Keep 1–2 decoded thumbnails in PSRAM LRU cache (optional)

## Error Handling

- Reject images > 128 KB (safety cap)
- Log warning on unexpected size
- Hide artwork and continue (graceful degradation)
- Whitelist content types (`image/jpeg`, `application/octet-stream`)

## Memory Budget

| Component | Location | Size |
|-----------|----------|------|
| Artwork buffer | PSRAM | 360×360×2 = ~253 KB |
| JPEG decode workspace | PSRAM | ~50 KB transient |
| HTTP response buffer | PSRAM | = Content-Length |

## References

- [Display Subsystem](../DISPLAY.md) — flush callback, byte swap
- [Color Configuration](COLORTEST_HELLOWORLD.md) — RGB565 byte order
- [LVGL Guidelines](../../lvgl/LVGL_GUIDELINES.md) — memory budget
