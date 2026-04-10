#include "connection_ui.h"

#include "lvgl.h"

namespace
{

constexpr lv_color_t kColorWhite = {.blue = 0xFF, .green = 0xFF, .red = 0xFF};
constexpr lv_color_t kColorDimBg = {.blue = 0x18, .green = 0x18, .red = 0x18};

static lv_obj_t* s_overlay = nullptr;   // full-screen dimmer
static lv_obj_t* s_box     = nullptr;   // rounded rect container
static lv_obj_t* s_spinner = nullptr;   // loading spinner
static lv_obj_t* s_label   = nullptr;   // message text

}  // namespace

namespace rs520
{

void connection_ui_create()
{
    auto* scr = lv_scr_act();

    // --- Full-screen overlay (semi-transparent black) ---
    s_overlay = lv_obj_create(scr);
    lv_obj_remove_style_all(s_overlay);
    lv_obj_set_size(s_overlay, 360, 360);
    lv_obj_center(s_overlay);
    lv_obj_set_style_bg_color(s_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_60, 0);
    // Block clicks on things behind
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_CLICKABLE);

    // --- Rounded rect box ---
    s_box = lv_obj_create(s_overlay);
    lv_obj_remove_style_all(s_box);
    lv_obj_set_size(s_box, 240, 140);
    lv_obj_center(s_box);
    lv_obj_set_style_bg_color(s_box, kColorDimBg, 0);
    lv_obj_set_style_bg_opa(s_box, LV_OPA_90, 0);
    lv_obj_set_style_radius(s_box, 20, 0);
    lv_obj_set_style_border_width(s_box, 0, 0);
    lv_obj_set_style_pad_all(s_box, 16, 0);
    lv_obj_set_flex_flow(s_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s_box, 12, 0);

    // --- Spinner ---
    s_spinner = lv_spinner_create(s_box);
    lv_obj_set_size(s_spinner, 40, 40);
    lv_spinner_set_anim_params(s_spinner, 1000, 240);
    lv_obj_set_style_arc_color(s_spinner, kColorWhite, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s_spinner, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_spinner, kColorDimBg, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_spinner, 4, LV_PART_MAIN);

    // --- Message label ---
    s_label = lv_label_create(s_box);
    lv_obj_set_style_text_font(s_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_label, kColorWhite, 0);
    lv_obj_set_style_text_align(s_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s_label, 200);
    lv_label_set_text(s_label, "Searching for\nRS520 Bridge...");

    // Start hidden
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
}

void connection_ui_show(const char* msg)
{
    if (!s_overlay) return;
    lv_label_set_text(s_label, msg);
    lv_obj_remove_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
}

void connection_ui_hide()
{
    if (!s_overlay) return;
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
}

}  // namespace rs520
