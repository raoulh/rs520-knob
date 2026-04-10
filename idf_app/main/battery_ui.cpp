#include "battery_ui.h"
#include "status_bar.h"

#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"

#include <cstdio>

namespace
{

constexpr const char* kTag = "battery_ui";

constexpr lv_color_t kColorWhite  = {.blue = 0xFF, .green = 0xFF, .red = 0xFF};
constexpr lv_color_t kColorGreen  = {.blue = 0x66, .green = 0xBB, .red = 0x44};
constexpr lv_color_t kColorOrange = {.blue = 0x33, .green = 0x99, .red = 0xEE};
constexpr lv_color_t kColorRed    = {.blue = 0x60, .green = 0x45, .red = 0xE9};

// Battery icon widget
static lv_obj_t* s_icon_label = nullptr;
// Percentage text
static lv_obj_t* s_pct_label  = nullptr;
// Warning overlay
static lv_obj_t* s_warn_container = nullptr;
// Auto-dismiss timer
static esp_timer_handle_t s_dismiss_timer = nullptr;
// Track if warning already shown for current low state
static bool s_warning_shown = false;

// Async update data
struct UpdateData
{
    rs520::BatteryState state;
    int percentage;
};
static UpdateData s_pending = {};

static const char* percent_to_icon(int pct, rs520::BatteryState state)
{
    if (state == rs520::BatteryState::kCharging)
    {
        return LV_SYMBOL_CHARGE;
    }
    if (pct >= 80) return LV_SYMBOL_BATTERY_FULL;
    if (pct >= 55) return LV_SYMBOL_BATTERY_3;
    if (pct >= 30) return LV_SYMBOL_BATTERY_2;
    if (pct >= 10) return LV_SYMBOL_BATTERY_1;
    return LV_SYMBOL_BATTERY_EMPTY;
}

static lv_color_t state_to_color(rs520::BatteryState state)
{
    switch (state)
    {
    case rs520::BatteryState::kCharging: return kColorGreen;
    case rs520::BatteryState::kLow:      return kColorOrange;
    case rs520::BatteryState::kCritical: return kColorRed;
    default:                             return kColorWhite;
    }
}

static void dismiss_timer_cb(void* /*arg*/)
{
    // Run on timer task — need async call into LVGL
    lv_async_call([](void* /*d*/) {
        rs520::battery_ui_hide_warning();
    }, nullptr);
}

static void update_cb(void* data)
{
    auto* ud = static_cast<UpdateData*>(data);
    if (!s_icon_label || !s_pct_label) return;

    // Update icon
    lv_label_set_text(s_icon_label, percent_to_icon(ud->percentage, ud->state));
    lv_obj_set_style_text_color(s_icon_label, state_to_color(ud->state), 0);

    // Update percentage text
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", ud->percentage);
    lv_label_set_text(s_pct_label, buf);
    lv_obj_set_style_text_color(s_pct_label, state_to_color(ud->state), 0);
}

/// Touch event on warning overlay → dismiss
static void warn_touch_cb(lv_event_t* e)
{
    (void)e;
    rs520::battery_ui_hide_warning();
}

}  // namespace

namespace rs520
{

void battery_ui_create()
{
    auto* parent = status_bar_container();
    if (!parent)
    {
        ESP_LOGE(kTag, "Status bar not created — call status_bar_create() first");
        return;
    }

    // Battery icon
    s_icon_label = lv_label_create(parent);
    lv_obj_set_style_text_font(s_icon_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_icon_label, kColorWhite, 0);
    lv_label_set_text(s_icon_label, LV_SYMBOL_BATTERY_FULL);

    // Percentage label
    s_pct_label = lv_label_create(parent);
    lv_obj_set_style_text_font(s_pct_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_pct_label, kColorWhite, 0);
    lv_label_set_text(s_pct_label, "--%");

    // Create auto-dismiss timer (one-shot, 5 seconds)
    esp_timer_create_args_t timer_args = {};
    timer_args.callback = dismiss_timer_cb;
    timer_args.name     = "batt_warn";
    esp_timer_create(&timer_args, &s_dismiss_timer);

    s_warning_shown = false;

    ESP_LOGI(kTag, "Battery UI created");
}

void battery_ui_update(BatteryState state, int percentage)
{
    s_pending.state      = state;
    s_pending.percentage = percentage;
    lv_async_call(update_cb, &s_pending);
}

void battery_ui_show_warning()
{
    if (s_warn_container) return;  // Already showing
    if (s_warning_shown)  return;  // Already shown this transition

    s_warning_shown = true;

    auto* scr = lv_scr_act();

    // Full-screen overlay
    s_warn_container = lv_obj_create(scr);
    lv_obj_set_size(s_warn_container, 360, 360);
    lv_obj_center(s_warn_container);
    lv_obj_set_style_bg_color(s_warn_container, lv_color_make(0x2e, 0x1a, 0x1a), 0);
    lv_obj_set_style_bg_opa(s_warn_container, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_warn_container, 0, 0);
    lv_obj_set_style_radius(s_warn_container, 180, 0);  // Round display
    lv_obj_set_style_pad_all(s_warn_container, 60, 0);
    lv_obj_set_flex_flow(s_warn_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_warn_container, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Dismiss on touch
    lv_obj_add_event_cb(s_warn_container, warn_touch_cb, LV_EVENT_CLICKED, nullptr);

    // Large battery icon
    auto* icon = lv_label_create(s_warn_container);
    lv_label_set_text(icon, LV_SYMBOL_BATTERY_EMPTY);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(icon, kColorRed, 0);
    lv_obj_set_style_text_align(icon, LV_TEXT_ALIGN_CENTER, 0);

    // Warning title
    auto* title = lv_label_create(s_warn_container);
    lv_label_set_text(title, "Low Battery");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, kColorRed, 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(title, 16, 0);

    // Message
    auto* msg = lv_label_create(s_warn_container);
    lv_label_set_text(msg, "Please charge\nyour device");
    lv_obj_set_style_text_font(msg, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(msg, lv_color_make(0xAA, 0xAA, 0xAA), 0);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(msg, 12, 0);

    // Auto-dismiss after 5 seconds
    esp_timer_start_once(s_dismiss_timer, 5000000);  // 5s in microseconds

    ESP_LOGW(kTag, "Low battery warning shown");
}

void battery_ui_hide_warning()
{
    if (s_warn_container)
    {
        esp_timer_stop(s_dismiss_timer);
        lv_obj_del(s_warn_container);
        s_warn_container = nullptr;
        ESP_LOGI(kTag, "Low battery warning dismissed");
    }
}

}  // namespace rs520
