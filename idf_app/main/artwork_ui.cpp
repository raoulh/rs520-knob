#include "artwork_ui.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lvgl.h"

#include <cstring>

namespace
{

constexpr const char* kTag = "artwork_ui";
constexpr int kWidth  = 360;
constexpr int kHeight = 360;
constexpr size_t kBufSize = kWidth * kHeight * 2;  // RGB565

static uint8_t* s_buffer = nullptr;
static lv_obj_t* s_img = nullptr;
static lv_image_dsc_t s_dsc = {};

}  // namespace

namespace rs520
{

void artwork_ui_create()
{
    // Allocate PSRAM buffer for artwork pixels
    s_buffer = static_cast<uint8_t*>(
        heap_caps_malloc(kBufSize, MALLOC_CAP_SPIRAM));
    if (!s_buffer)
    {
        ESP_LOGE(kTag, "PSRAM alloc failed (%u bytes)", kBufSize);
        return;
    }
    memset(s_buffer, 0, kBufSize);

    // Setup LVGL image descriptor
    s_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    s_dsc.header.w  = kWidth;
    s_dsc.header.h  = kHeight;
    s_dsc.data_size = kBufSize;
    s_dsc.data      = s_buffer;

    // Create image widget as first child of screen (behind everything)
    s_img = lv_image_create(lv_screen_active());
    lv_obj_set_pos(s_img, 0, 0);
    lv_obj_set_size(s_img, kWidth, kHeight);
    lv_image_set_src(s_img, &s_dsc);

    // Hidden until artwork arrives
    lv_obj_add_flag(s_img, LV_OBJ_FLAG_HIDDEN);

    ESP_LOGI(kTag, "created (buffer=%p, %u bytes PSRAM)", s_buffer, kBufSize);
}

void artwork_ui_set(const uint8_t* rgb565_le, size_t len)
{
    if (!s_buffer || !s_img) return;
    if (len != kBufSize)
    {
        ESP_LOGW(kTag, "unexpected size: %u (expected %u)", len, kBufSize);
        return;
    }

    memcpy(s_buffer, rgb565_le, kBufSize);
    lv_image_set_src(s_img, &s_dsc);
    lv_obj_remove_flag(s_img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(s_img);

    ESP_LOGI(kTag, "updated %u bytes", kBufSize);
}

void artwork_ui_clear()
{
    if (!s_img) return;
    lv_obj_add_flag(s_img, LV_OBJ_FLAG_HIDDEN);
}

}  // namespace rs520
