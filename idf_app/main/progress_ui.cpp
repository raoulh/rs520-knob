#include "progress_ui.h"

#include "lvgl.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{

// Display geometry
constexpr int kDisplaySize  = 360;
constexpr int kArcWidth     = 20;
constexpr int kArcObjSize   = kDisplaySize - 20;         // 340
constexpr int kArcRadius    = kArcObjSize / 2;           // 170 outer
constexpr int kArcMidRadius = kArcRadius - kArcWidth / 2; // 160 centerline

// Arc sweep: 135° → 45° clockwise = 270° total
constexpr float kStartAngle = 135.0f;
constexpr float kSweep      = 270.0f;

// Tick dot
constexpr int kTickSize = 10;

// Colors
constexpr lv_color_t kColorBg      = {.blue = 0x33, .green = 0x33, .red = 0x33};
constexpr lv_color_t kColorFg      = {.blue = 0xE0, .green = 0x80, .red = 0x20};
constexpr lv_color_t kColorWhite   = {.blue = 0xFF, .green = 0xFF, .red = 0xFF};
constexpr lv_color_t kColorPopupBg = {.blue = 0x10, .green = 0x10, .red = 0x10};

constexpr uint32_t kPopupTimeoutMs   = 1500;
constexpr uint32_t kAnimDurationMs   = 150;
constexpr uint32_t kSettleDelayMs    = 500;  // wait after last encoder move

// Widgets
static lv_obj_t* s_arc         = nullptr;  // volume arc
static lv_obj_t* s_tick        = nullptr;  // target tick dot
static lv_obj_t* s_popup_box   = nullptr;  // rounded rect popup
static lv_obj_t* s_popup_label = nullptr;  // "42%"
static lv_timer_t* s_popup_timer  = nullptr;
static lv_timer_t* s_settle_timer = nullptr;

// State
static int  s_confirmed       = 0;   // last value confirmed by RS520
static int  s_target          = 0;   // where encoder wants to go
static int  s_pending         = -1;  // buffered confirmed value during encoder activity
static bool s_encoder_active  = false;

// --- Tick positioning ---

static void position_tick(int value)
{
    float angle_deg = kStartAngle + (static_cast<float>(value) * kSweep) / 100.0f;
    float angle_rad = angle_deg * static_cast<float>(M_PI) / 180.0f;

    // Screen center
    constexpr float cx = kDisplaySize / 2.0f;
    constexpr float cy = kDisplaySize / 2.0f;

    float x = cx + kArcMidRadius * cosf(angle_rad) - kTickSize / 2.0f;
    float y = cy + kArcMidRadius * sinf(angle_rad) - kTickSize / 2.0f;

    lv_obj_set_pos(s_tick, static_cast<int>(x), static_cast<int>(y));
}

static void show_tick(int value)
{
    position_tick(value);
    lv_obj_remove_flag(s_tick, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_tick);
}

static void hide_tick()
{
    lv_obj_add_flag(s_tick, LV_OBJ_FLAG_HIDDEN);
}

// --- Popup ---

static void show_popup()
{
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", s_target);
    lv_label_set_text(s_popup_label, buf);
    lv_obj_remove_flag(s_popup_box, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_popup_box);

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
}

// --- Arc animation ---

static void anim_arc_cb(void* obj, int32_t value)
{
    lv_arc_set_value(static_cast<lv_obj_t*>(obj), value);
}

static void animate_arc_to(int value)
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

// --- Settle timer: fires after encoder stops ---

static void settle_timer_cb(lv_timer_t* /*t*/)
{
    s_encoder_active = false;
    lv_timer_pause(s_settle_timer);

    // Apply last buffered confirmed value
    if (s_pending >= 0)
    {
        s_confirmed = s_pending;
        s_target = s_confirmed;
        s_pending = -1;
        animate_arc_to(s_confirmed);
    }

    hide_tick();
}

}  // namespace

namespace rs520
{

void progress_ui_create()
{
    auto* scr = lv_scr_act();

    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    // --- Volume arc ---
    s_arc = lv_arc_create(scr);
    lv_obj_set_size(s_arc, kArcObjSize, kArcObjSize);
    lv_obj_center(s_arc);
    lv_arc_set_range(s_arc, 0, 100);
    lv_arc_set_value(s_arc, 0);
    lv_arc_set_bg_angles(s_arc, 135, 45);
    lv_arc_set_rotation(s_arc, 0);
    lv_obj_remove_flag(s_arc, LV_OBJ_FLAG_CLICKABLE);

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

    // --- Tick dot (hidden, positioned on arc track) ---
    s_tick = lv_obj_create(scr);
    lv_obj_remove_style_all(s_tick);
    lv_obj_set_size(s_tick, kTickSize, kTickSize);
    lv_obj_set_style_bg_color(s_tick, kColorWhite, 0);
    lv_obj_set_style_bg_opa(s_tick, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_tick, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(s_tick, 0, 0);
    lv_obj_remove_flag(s_tick, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_add_flag(s_tick, LV_OBJ_FLAG_HIDDEN);

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

    // Timers (paused initially)
    s_popup_timer = lv_timer_create(popup_timer_cb, kPopupTimeoutMs, nullptr);
    lv_timer_pause(s_popup_timer);

    s_settle_timer = lv_timer_create(settle_timer_cb, kSettleDelayMs, nullptr);
    lv_timer_pause(s_settle_timer);

    s_confirmed = 0;
    s_target = 0;
    s_pending = -1;
    s_encoder_active = false;
}

void progress_ui_set_confirmed(int value)
{
    value = std::clamp(value, 0, 100);

    if (s_encoder_active)
    {
        // Buffer — don't animate while user is turning
        s_pending = value;
        return;
    }

    s_confirmed = value;
    s_target = value;
    animate_arc_to(value);
}

int progress_ui_set_target(int value)
{
    // Hard snap both values — for full state sync, no tick, no defer
    s_confirmed = std::clamp(value, 0, 100);
    s_target = s_confirmed;
    s_pending = -1;
    s_encoder_active = false;
    lv_arc_set_value(s_arc, s_confirmed);
    hide_tick();
    return s_confirmed;
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
    s_target = std::clamp(s_target + delta, 0, 100);
    s_encoder_active = true;

    // Show tick at target position
    show_tick(s_target);
    show_popup();

    // Reset settle timer
    if (s_settle_timer)
    {
        lv_timer_reset(s_settle_timer);
        lv_timer_resume(s_settle_timer);
    }

    return s_target;
}

int progress_ui_set(int value)
{
    return progress_ui_set_target(value);
}

int progress_ui_get()
{
    return s_target;
}

}  // namespace rs520
