#include "progress_ui.h"

#include "lvgl.h"
#include <algorithm>
#include <cstdio>

namespace
{

// Display geometry
constexpr int kDisplaySize = 360;
constexpr int kArcWidth    = 20;
constexpr int kGhostWidth  = 14;  // thinner than main arc

// Colors
constexpr lv_color_t kColorBg      = {.blue = 0x33, .green = 0x33, .red = 0x33};
constexpr lv_color_t kColorFg      = {.blue = 0xE0, .green = 0x80, .red = 0x20};  // Blue-ish accent
constexpr lv_color_t kColorGhost   = {.blue = 0xF0, .green = 0xC0, .red = 0x60};  // Lighter accent
constexpr lv_color_t kColorWhite   = {.blue = 0xFF, .green = 0xFF, .red = 0xFF};
constexpr lv_color_t kColorPopupBg = {.blue = 0x10, .green = 0x10, .red = 0x10};

constexpr uint32_t kPopupTimeoutMs   = 1500;
constexpr uint32_t kAnimDurationMs   = 150;

// Widgets
static lv_obj_t* s_arc         = nullptr;  // confirmed volume (solid)
static lv_obj_t* s_ghost_arc   = nullptr;  // target volume (ghost)
static lv_obj_t* s_popup_box   = nullptr;  // rounded rect popup
static lv_obj_t* s_popup_label = nullptr;  // "42%"
static lv_timer_t* s_popup_timer = nullptr;

// Values
static int s_confirmed = 0;
static int s_target    = 0;

// --- Helpers ---

static void update_ghost_visibility()
{
    if (s_target == s_confirmed)
    {
        lv_obj_add_flag(s_ghost_arc, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_remove_flag(s_ghost_arc, LV_OBJ_FLAG_HIDDEN);
    }
}

static void show_popup()
{
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", s_target);
    lv_label_set_text(s_popup_label, buf);
    lv_obj_remove_flag(s_popup_box, LV_OBJ_FLAG_HIDDEN);

    // Reset timer
    if (s_popup_timer)
    {
        lv_timer_reset(s_popup_timer);
        lv_timer_resume(s_popup_timer);
    }
}

static void popup_timer_cb(lv_timer_t* /*t*/)
{
    lv_obj_add_flag(s_popup_box, LV_OBJ_FLAG_HIDDEN);
    lv_timer_pause(s_popup_timer);
    update_ghost_visibility();
}

// Animation callback for confirmed arc
static void anim_arc_cb(void* obj, int32_t value)
{
    lv_arc_set_value(static_cast<lv_obj_t*>(obj), value);
}

static void animate_confirmed_to(int value)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_arc);
    lv_anim_set_values(&a, lv_arc_get_value(s_arc), value);
    lv_anim_set_duration(&a, kAnimDurationMs);
    lv_anim_set_exec_cb(&a, anim_arc_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

// Apply common arc styling (shared between confirmed and ghost)
static void style_arc_common(lv_obj_t* arc)
{
    lv_obj_set_size(arc, kDisplaySize - 20, kDisplaySize - 20);
    lv_obj_center(arc);
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_value(arc, 0);
    lv_arc_set_bg_angles(arc, 135, 45);
    lv_arc_set_rotation(arc, 0);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
}

}  // namespace

namespace rs520
{

void progress_ui_create()
{
    auto* scr = lv_scr_act();

    // Black background
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    // --- Ghost arc (behind, created first) ---
    s_ghost_arc = lv_arc_create(scr);
    style_arc_common(s_ghost_arc);

    static lv_style_t ghost_ind;
    lv_style_init(&ghost_ind);
    lv_style_set_arc_color(&ghost_ind, kColorGhost);
    lv_style_set_arc_width(&ghost_ind, kGhostWidth);
    lv_style_set_arc_rounded(&ghost_ind, true);
    lv_obj_add_style(s_ghost_arc, &ghost_ind, LV_PART_INDICATOR);

    static lv_style_t ghost_bg;
    lv_style_init(&ghost_bg);
    lv_style_set_arc_color(&ghost_bg, kColorBg);
    lv_style_set_arc_width(&ghost_bg, kGhostWidth);
    lv_style_set_arc_opa(&ghost_bg, LV_OPA_0);  // invisible track
    lv_style_set_arc_rounded(&ghost_bg, true);
    lv_obj_add_style(s_ghost_arc, &ghost_bg, LV_PART_MAIN);

    static lv_style_t ghost_knob;
    lv_style_init(&ghost_knob);
    lv_style_set_pad_all(&ghost_knob, 0);
    lv_style_set_width(&ghost_knob, 0);
    lv_style_set_height(&ghost_knob, 0);
    lv_obj_add_style(s_ghost_arc, &ghost_knob, LV_PART_KNOB);

    lv_obj_add_flag(s_ghost_arc, LV_OBJ_FLAG_HIDDEN);

    // --- Confirmed arc (on top) ---
    s_arc = lv_arc_create(scr);
    style_arc_common(s_arc);

    static lv_style_t style_indicator;
    lv_style_init(&style_indicator);
    lv_style_set_arc_color(&style_indicator, kColorFg);
    lv_style_set_arc_width(&style_indicator, kArcWidth);
    lv_style_set_arc_rounded(&style_indicator, true);
    lv_obj_add_style(s_arc, &style_indicator, LV_PART_INDICATOR);

    static lv_style_t style_bg;
    lv_style_init(&style_bg);
    lv_style_set_arc_color(&style_bg, kColorBg);
    lv_style_set_arc_width(&style_bg, kArcWidth);
    lv_style_set_arc_rounded(&style_bg, true);
    lv_obj_add_style(s_arc, &style_bg, LV_PART_MAIN);

    static lv_style_t style_knob;
    lv_style_init(&style_knob);
    lv_style_set_pad_all(&style_knob, 0);
    lv_style_set_width(&style_knob, 0);
    lv_style_set_height(&style_knob, 0);
    lv_obj_add_style(s_arc, &style_knob, LV_PART_KNOB);

    // --- Volume popup (centered, hidden) ---
    s_popup_box = lv_obj_create(scr);
    lv_obj_remove_style_all(s_popup_box);
    lv_obj_set_size(s_popup_box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_center(s_popup_box);
    lv_obj_set_style_bg_color(s_popup_box, kColorPopupBg, 0);
    lv_obj_set_style_bg_opa(s_popup_box, LV_OPA_70, 0);
    lv_obj_set_style_radius(s_popup_box, 16, 0);
    lv_obj_set_style_pad_hor(s_popup_box, 20, 0);
    lv_obj_set_style_pad_ver(s_popup_box, 12, 0);
    lv_obj_set_style_border_width(s_popup_box, 0, 0);

    s_popup_label = lv_label_create(s_popup_box);
    lv_obj_set_style_text_font(s_popup_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(s_popup_label, kColorWhite, 0);
    lv_label_set_text(s_popup_label, "0%");

    lv_obj_add_flag(s_popup_box, LV_OBJ_FLAG_HIDDEN);

    // Popup auto-hide timer (paused initially)
    s_popup_timer = lv_timer_create(popup_timer_cb, kPopupTimeoutMs, nullptr);
    lv_timer_pause(s_popup_timer);

    s_confirmed = 0;
    s_target = 0;
}

void progress_ui_set_confirmed(int value)
{
    s_confirmed = std::clamp(value, 0, 100);
    animate_confirmed_to(s_confirmed);
    update_ghost_visibility();
}

int progress_ui_set_target(int value)
{
    s_target = std::clamp(value, 0, 100);
    lv_arc_set_value(s_ghost_arc, s_target);
    lv_obj_remove_flag(s_ghost_arc, LV_OBJ_FLAG_HIDDEN);
    show_popup();
    return s_target;
}

int progress_ui_get_target()
{
    return s_target;
}

int progress_ui_get_confirmed()
{
    return s_confirmed;
}

int progress_ui_adjust(int delta)
{
    return progress_ui_set_target(s_target + delta);
}

int progress_ui_set(int value)
{
    s_confirmed = std::clamp(value, 0, 100);
    s_target = s_confirmed;
    lv_arc_set_value(s_arc, s_confirmed);
    lv_arc_set_value(s_ghost_arc, s_target);
    update_ghost_visibility();
    return s_confirmed;
}

int progress_ui_get()
{
    return s_target;
}

}  // namespace rs520
