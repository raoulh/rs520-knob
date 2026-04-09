#include "progress_ui.h"

#include "lvgl.h"
#include <algorithm>
#include <cstdio>

namespace
{

// Display geometry
constexpr int kDisplaySize = 360;
constexpr int kArcWidth    = 20;

// Colors
constexpr lv_color_t kColorBg    = {.blue = 0x33, .green = 0x33, .red = 0x33};  // Dark gray track
constexpr lv_color_t kColorFg    = {.blue = 0xE0, .green = 0x80, .red = 0x20};  // Blue-ish accent
constexpr lv_color_t kColorWhite = {.blue = 0xFF, .green = 0xFF, .red = 0xFF};

static lv_obj_t* s_arc   = nullptr;
static lv_obj_t* s_label = nullptr;
static int s_value = 0;

static void update_label()
{
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", s_value);
    lv_label_set_text(s_label, buf);
}

}  // namespace

namespace rs520
{

void progress_ui_create()
{
    auto* scr = lv_scr_act();

    // Black background
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    // --- Arc (round progress bar) ---
    s_arc = lv_arc_create(scr);
    lv_obj_set_size(s_arc, kDisplaySize - 20, kDisplaySize - 20);  // Slight margin
    lv_obj_center(s_arc);

    // Range 0–100
    lv_arc_set_range(s_arc, 0, 100);
    lv_arc_set_value(s_arc, 0);
    lv_arc_set_bg_angles(s_arc, 135, 45);    // 270° sweep (bottom-left to bottom-right)
    lv_arc_set_rotation(s_arc, 0);

    // Disable user input on arc (encoder drives it, not touch)
    lv_obj_remove_flag(s_arc, LV_OBJ_FLAG_CLICKABLE);

    // Style: indicator (filled part)
    static lv_style_t style_indicator;
    lv_style_init(&style_indicator);
    lv_style_set_arc_color(&style_indicator, kColorFg);
    lv_style_set_arc_width(&style_indicator, kArcWidth);
    lv_style_set_arc_rounded(&style_indicator, true);
    lv_obj_add_style(s_arc, &style_indicator, LV_PART_INDICATOR);

    // Style: background track
    static lv_style_t style_bg;
    lv_style_init(&style_bg);
    lv_style_set_arc_color(&style_bg, kColorBg);
    lv_style_set_arc_width(&style_bg, kArcWidth);
    lv_style_set_arc_rounded(&style_bg, true);
    lv_obj_add_style(s_arc, &style_bg, LV_PART_MAIN);

    // Hide knob (round handle)
    static lv_style_t style_knob;
    lv_style_init(&style_knob);
    lv_style_set_pad_all(&style_knob, 0);
    lv_style_set_width(&style_knob, 0);
    lv_style_set_height(&style_knob, 0);
    lv_obj_add_style(s_arc, &style_knob, LV_PART_KNOB);

    // --- Percentage label (centered) ---
    s_label = lv_label_create(scr);
    lv_obj_set_style_text_font(s_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(s_label, kColorWhite, 0);
    lv_obj_center(s_label);

    s_value = 0;
    update_label();
}

int progress_ui_set(int value)
{
    s_value = std::clamp(value, 0, 100);
    lv_arc_set_value(s_arc, s_value);
    update_label();
    return s_value;
}

int progress_ui_get()
{
    return s_value;
}

int progress_ui_adjust(int delta)
{
    return progress_ui_set(s_value + delta);
}

}  // namespace rs520
