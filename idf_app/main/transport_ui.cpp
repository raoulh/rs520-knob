#include "transport_ui.h"
#include "bridge_discovery.h"

#include "esp_log.h"
#include "lvgl.h"

namespace
{

constexpr const char* kTag = "transport_ui";
constexpr int kScreenSize     = 360;
constexpr int kBtnSize        = 48;
constexpr int kShazamBtnSize  = 44;
constexpr int kTransportY     = 82;   // offset below center
constexpr int kShazamY        = -82;  // offset above center
constexpr int kBtnSpacing     = 60;   // center-to-center distance for prev/play/next

// Widgets
static lv_obj_t* s_btn_shazam    = nullptr;
static lv_obj_t* s_btn_prev      = nullptr;
static lv_obj_t* s_btn_play      = nullptr;
static lv_obj_t* s_btn_next      = nullptr;

// Styles
static lv_style_t s_style_btn;
static lv_style_t s_style_btn_pressed;
static lv_style_t s_style_icon;

static void init_styles()
{
    // Transparent round button
    lv_style_init(&s_style_btn);
    lv_style_set_bg_color(&s_style_btn, lv_color_black());
    lv_style_set_bg_opa(&s_style_btn, LV_OPA_50);
    lv_style_set_radius(&s_style_btn, LV_RADIUS_CIRCLE);
    lv_style_set_border_width(&s_style_btn, 0);
    lv_style_set_shadow_width(&s_style_btn, 0);
    lv_style_set_pad_all(&s_style_btn, 0);

    // Pressed state
    lv_style_init(&s_style_btn_pressed);
    lv_style_set_bg_opa(&s_style_btn_pressed, LV_OPA_80);

    // Icon label inside button
    lv_style_init(&s_style_icon);
    lv_style_set_text_color(&s_style_icon, lv_color_white());
    lv_style_set_text_font(&s_style_icon, &lv_font_montserrat_22);
}

static void shazam_cb(lv_event_t* e)
{
    (void)e;
    ESP_LOGI(kTag, "shazam pressed");
    rs520::bridge_send_shazam();
}

static void prev_cb(lv_event_t* e)
{
    (void)e;
    ESP_LOGI(kTag, "prev pressed");
    rs520::bridge_send_prev();
}

static void play_pause_cb(lv_event_t* e)
{
    (void)e;
    ESP_LOGI(kTag, "play_pause pressed");
    rs520::bridge_send_play_pause();
}

static void next_cb(lv_event_t* e)
{
    (void)e;
    ESP_LOGI(kTag, "next pressed");
    rs520::bridge_send_next();
}

static lv_obj_t* create_btn(lv_obj_t* parent, const char* symbol,
                             int size, lv_align_t align, int x_ofs, int y_ofs,
                             lv_event_cb_t cb)
{
    lv_obj_t* btn = lv_obj_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_add_style(btn, &s_style_btn, 0);
    lv_obj_add_style(btn, &s_style_btn_pressed, LV_STATE_PRESSED);
    lv_obj_set_size(btn, size, size);
    lv_obj_align(btn, align, x_ofs, y_ofs);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* label = lv_label_create(btn);
    lv_obj_add_style(label, &s_style_icon, 0);
    lv_label_set_text(label, symbol);
    lv_obj_center(label);

    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);

    return btn;
}

}  // namespace

namespace rs520
{

void transport_ui_create()
{
    init_styles();

    lv_obj_t* scr = lv_screen_active();

    // Shazam button — centered above metadata block
    s_btn_shazam = create_btn(scr, LV_SYMBOL_AUDIO,
                              kShazamBtnSize, LV_ALIGN_CENTER, 0, kShazamY,
                              shazam_cb);

    // Transport row — centered below metadata block
    s_btn_prev = create_btn(scr, LV_SYMBOL_PREV,
                            kBtnSize, LV_ALIGN_CENTER, -kBtnSpacing, kTransportY,
                            prev_cb);

    s_btn_play = create_btn(scr, LV_SYMBOL_PLAY,
                            kBtnSize, LV_ALIGN_CENTER, 0, kTransportY,
                            play_pause_cb);

    s_btn_next = create_btn(scr, LV_SYMBOL_NEXT,
                            kBtnSize, LV_ALIGN_CENTER, kBtnSpacing, kTransportY,
                            next_cb);

    ESP_LOGI(kTag, "created");
}

}  // namespace rs520
