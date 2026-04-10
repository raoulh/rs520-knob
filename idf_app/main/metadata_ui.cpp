#include "metadata_ui.h"

#include "esp_log.h"
#include "lvgl.h"

#include <cstdio>

namespace
{

constexpr const char* kTag = "metadata_ui";
constexpr int kContainerWidth  = 272;
constexpr int kLabelMaxWidth   = 260;
constexpr int kBarWidth        = 260;
constexpr int kBarHeight       = 4;
constexpr int kScreenSize      = 360;

// Widgets
static lv_obj_t* s_container  = nullptr;
static lv_obj_t* s_backdrop   = nullptr;
static lv_obj_t* s_title      = nullptr;
static lv_obj_t* s_artist     = nullptr;
static lv_obj_t* s_track_info = nullptr;
static lv_obj_t* s_bar        = nullptr;
static lv_obj_t* s_time_label = nullptr;

// Styles
static lv_style_t s_style_backdrop;
static lv_style_t s_style_title;
static lv_style_t s_style_artist;
static lv_style_t s_style_track_info;
static lv_style_t s_style_bar_bg;
static lv_style_t s_style_bar_ind;

static void init_styles()
{
    // Semi-transparent dark backdrop
    lv_style_init(&s_style_backdrop);
    lv_style_set_bg_color(&s_style_backdrop, lv_color_black());
    lv_style_set_bg_opa(&s_style_backdrop, LV_OPA_60);
    lv_style_set_radius(&s_style_backdrop, 12);
    lv_style_set_pad_all(&s_style_backdrop, 16);
    lv_style_set_pad_row(&s_style_backdrop, 4);

    // Title: 22px, white
    lv_style_init(&s_style_title);
    lv_style_set_text_font(&s_style_title, &lv_font_montserrat_22);
    lv_style_set_text_color(&s_style_title, lv_color_white());

    // Artist: 14px, light gray
    lv_style_init(&s_style_artist);
    lv_style_set_text_font(&s_style_artist, &lv_font_montserrat_14);
    lv_style_set_text_color(&s_style_artist, lv_color_make(0xCC, 0xCC, 0xCC));

    // TrackInfo: 14px, dim gray
    lv_style_init(&s_style_track_info);
    lv_style_set_text_font(&s_style_track_info, &lv_font_montserrat_14);
    lv_style_set_text_color(&s_style_track_info, lv_color_make(0x88, 0x88, 0x88));

    // Progress bar background
    lv_style_init(&s_style_bar_bg);
    lv_style_set_bg_color(&s_style_bar_bg, lv_color_make(0x33, 0x33, 0x33));
    lv_style_set_bg_opa(&s_style_bar_bg, LV_OPA_COVER);
    lv_style_set_radius(&s_style_bar_bg, 2);

    // Progress bar indicator
    lv_style_init(&s_style_bar_ind);
    lv_style_set_bg_color(&s_style_bar_ind, lv_color_make(0x20, 0x80, 0xE0));
    lv_style_set_bg_opa(&s_style_bar_ind, LV_OPA_COVER);
    lv_style_set_radius(&s_style_bar_ind, 2);
}

static lv_obj_t* create_scrolling_label(lv_obj_t* parent, lv_style_t* style)
{
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_add_style(label, style, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(label, kLabelMaxWidth);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(label, "");
    return label;
}

static void format_time(int ms, char* buf, size_t buf_size)
{
    if (ms < 0) ms = 0;
    int total_sec = ms / 1000;
    int min = total_sec / 60;
    int sec = total_sec % 60;
    snprintf(buf, buf_size, "%d:%02d", min, sec);
}

}  // namespace

namespace rs520
{

void metadata_ui_create()
{
    init_styles();

    // Backdrop container — centered on screen
    s_backdrop = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(s_backdrop);
    lv_obj_add_style(s_backdrop, &s_style_backdrop, 0);
    lv_obj_set_size(s_backdrop, kContainerWidth, LV_SIZE_CONTENT);
    lv_obj_align(s_backdrop, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_flex_flow(s_backdrop, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_backdrop, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(s_backdrop, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    s_title = create_scrolling_label(s_backdrop, &s_style_title);

    // Artist
    s_artist = create_scrolling_label(s_backdrop, &s_style_artist);

    // TrackInfo
    s_track_info = create_scrolling_label(s_backdrop, &s_style_track_info);

    // Spacer before progress bar
    lv_obj_t* spacer = lv_obj_create(s_backdrop);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_size(spacer, 1, 8);

    // Progress bar
    s_bar = lv_bar_create(s_backdrop);
    lv_obj_set_size(s_bar, kBarWidth, kBarHeight);
    lv_bar_set_range(s_bar, 0, 1000);
    lv_bar_set_value(s_bar, 0, LV_ANIM_OFF);
    lv_obj_add_style(s_bar, &s_style_bar_bg, LV_PART_MAIN);
    lv_obj_add_style(s_bar, &s_style_bar_ind, LV_PART_INDICATOR);

    // Time label
    s_time_label = lv_label_create(s_backdrop);
    lv_obj_add_style(s_time_label, &s_style_track_info, 0);  // same dim gray style
    lv_obj_set_style_text_align(s_time_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s_time_label, kLabelMaxWidth);
    lv_label_set_text(s_time_label, "");

    // Hidden until first state event
    lv_obj_add_flag(s_backdrop, LV_OBJ_FLAG_HIDDEN);

    // Store container ref
    s_container = s_backdrop;

    ESP_LOGI(kTag, "created");
}

void metadata_ui_set_track(const char* title, const char* artist, const char* track_info)
{
    if (!s_container) return;

    if (title && title[0] != '\0')
    {
        lv_label_set_text(s_title, title);
        lv_obj_remove_flag(s_title, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_add_flag(s_title, LV_OBJ_FLAG_HIDDEN);
    }

    if (artist && artist[0] != '\0')
    {
        lv_label_set_text(s_artist, artist);
        lv_obj_remove_flag(s_artist, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_add_flag(s_artist, LV_OBJ_FLAG_HIDDEN);
    }

    if (track_info && track_info[0] != '\0')
    {
        lv_label_set_text(s_track_info, track_info);
        lv_obj_remove_flag(s_track_info, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_add_flag(s_track_info, LV_OBJ_FLAG_HIDDEN);
    }

    // Show container if at least title is set
    if (title && title[0] != '\0')
    {
        lv_obj_remove_flag(s_container, LV_OBJ_FLAG_HIDDEN);
    }
}

void metadata_ui_set_position(int cur_ms, int dur_ms)
{
    if (!s_container) return;

    // Update progress bar
    if (dur_ms > 0)
    {
        int permille = static_cast<int>(
            static_cast<int64_t>(cur_ms) * 1000 / dur_ms);
        if (permille > 1000) permille = 1000;
        if (permille < 0) permille = 0;
        lv_bar_set_value(s_bar, permille, LV_ANIM_OFF);
        lv_obj_remove_flag(s_bar, LV_OBJ_FLAG_HIDDEN);

        // Format time string
        char cur_str[16];
        char dur_str[16];
        format_time(cur_ms, cur_str, sizeof(cur_str));
        format_time(dur_ms, dur_str, sizeof(dur_str));

        char time_buf[40];
        snprintf(time_buf, sizeof(time_buf), "%s / %s", cur_str, dur_str);
        lv_label_set_text(s_time_label, time_buf);
        lv_obj_remove_flag(s_time_label, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        // No duration info — hide progress bar and time
        lv_obj_add_flag(s_bar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_time_label, LV_OBJ_FLAG_HIDDEN);
    }
}

void metadata_ui_show()
{
    if (s_container)
    {
        lv_obj_remove_flag(s_container, LV_OBJ_FLAG_HIDDEN);
    }
}

void metadata_ui_hide()
{
    if (s_container)
    {
        lv_obj_add_flag(s_container, LV_OBJ_FLAG_HIDDEN);
    }
}

}  // namespace rs520
