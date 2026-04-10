#include "wifi_status_ui.h"
#include "wifi_manager.h"
#include "status_bar.h"

#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "lvgl.h"

#include <cstdio>
#include <cstring>

namespace
{

constexpr int kIconSize    = 28;

constexpr lv_color_t kColorOk   = {.blue = 0xA3, .green = 0xCC, .red = 0x4E};  // Green
constexpr lv_color_t kColorWarn  = {.blue = 0x4A, .green = 0xC4, .red = 0xE9};  // Yellow-ish
constexpr lv_color_t kColorErr   = {.blue = 0x60, .green = 0x45, .red = 0xE9};  // Red-ish
constexpr lv_color_t kColorWhite = {.blue = 0xFF, .green = 0xFF, .red = 0xFF};

// WiFi icon widget
static lv_obj_t* s_icon_label = nullptr;

// Provisioning overlay
static lv_obj_t* s_prov_container = nullptr;
static lv_obj_t* s_prov_ssid_label = nullptr;

// Data for async LVGL update
struct StatusData
{
    rs520::WifiState state;
    int8_t rssi;
};

static StatusData s_pending = {};

static const char* rssi_to_icon(int8_t rssi)
{
    // Use simple text symbols for signal strength
    // Could be replaced with custom font icons later
    if (rssi > -50) return LV_SYMBOL_WIFI;       // Strong
    if (rssi > -67) return LV_SYMBOL_WIFI;       // Medium
    if (rssi > -80) return LV_SYMBOL_WIFI;       // Weak
    return LV_SYMBOL_WIFI;                        // Very weak
}

static int rssi_to_bars(int8_t rssi)
{
    if (rssi > -50) return 3;
    if (rssi > -67) return 2;
    if (rssi > -80) return 1;
    return 0;
}

static void update_icon_cb(void* data)
{
    auto* sd = static_cast<StatusData*>(data);
    if (!s_icon_label) return;

    switch (sd->state)
    {
    case rs520::WifiState::kConnected:
    {
        int bars = rssi_to_bars(sd->rssi);
        lv_label_set_text(s_icon_label, rssi_to_icon(sd->rssi));

        lv_color_t color = kColorOk;
        if (bars <= 1) color = kColorWarn;
        if (bars == 0) color = kColorErr;
        lv_obj_set_style_text_color(s_icon_label, color, 0);
        break;
    }
    case rs520::WifiState::kConnecting:
        lv_label_set_text(s_icon_label, LV_SYMBOL_REFRESH);
        lv_obj_set_style_text_color(s_icon_label, kColorWarn, 0);
        break;

    case rs520::WifiState::kProvisioning:
        lv_label_set_text(s_icon_label, LV_SYMBOL_SETTINGS);
        lv_obj_set_style_text_color(s_icon_label, kColorWarn, 0);
        break;

    case rs520::WifiState::kDisconnected:
    default:
        lv_label_set_text(s_icon_label, LV_SYMBOL_CLOSE);
        lv_obj_set_style_text_color(s_icon_label, kColorErr, 0);
        break;
    }
}

// Hide provisioning overlay (called via lv_async_call on LVGL thread)
static void hide_provision_cb(void* /*data*/)
{
    if (s_prov_container)
    {
        lv_obj_del(s_prov_container);
        s_prov_container  = nullptr;
        s_prov_ssid_label = nullptr;
    }
}

// WiFi state change callback — called from WiFi event task
static void wifi_state_cb(rs520::WifiState state, void* /*ctx*/)
{
    s_pending.state = state;
    s_pending.rssi  = rs520::wifi_rssi();
    lv_async_call(update_icon_cb, &s_pending);

    // Auto-hide provisioning overlay when connected
    if (state == rs520::WifiState::kConnected && s_prov_container)
    {
        lv_async_call(hide_provision_cb, nullptr);
    }
}

}  // namespace

namespace rs520
{

void wifi_status_ui_create()
{
    auto* parent = status_bar_container();
    if (!parent)
    {
        ESP_LOGE("wifi_ui", "Status bar not created — call status_bar_create() first");
        return;
    }

    s_icon_label = lv_label_create(parent);
    lv_obj_set_style_text_font(s_icon_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_icon_label, kColorErr, 0);
    lv_label_set_text(s_icon_label, LV_SYMBOL_CLOSE);  // disconnected initially

    // Register for WiFi state changes
    wifi_on_state_change(wifi_state_cb, nullptr);
}

void wifi_status_ui_update(WifiState state, int8_t rssi)
{
    s_pending.state = state;
    s_pending.rssi  = rssi;
    lv_async_call(update_icon_cb, &s_pending);
}

void wifi_status_ui_show_provision(const char* ap_ssid)
{
    auto* scr = lv_scr_act();

    if (s_prov_container)
    {
        lv_obj_del(s_prov_container);
    }

    // Full-screen overlay
    s_prov_container = lv_obj_create(scr);
    lv_obj_set_size(s_prov_container, 360, 360);
    lv_obj_center(s_prov_container);
    lv_obj_set_style_bg_color(s_prov_container, lv_color_make(0x1a, 0x1a, 0x2e), 0);
    lv_obj_set_style_bg_opa(s_prov_container, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_prov_container, 0, 0);
    lv_obj_set_style_radius(s_prov_container, 180, 0);  // Round for circular display
    lv_obj_set_style_pad_all(s_prov_container, 40, 0);
    lv_obj_set_flex_flow(s_prov_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_prov_container, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Title
    auto* title = lv_label_create(s_prov_container);
    lv_label_set_text(title, LV_SYMBOL_WIFI " Setup");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, lv_color_make(0xe9, 0x45, 0x60), 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

    // Instructions
    auto* instr = lv_label_create(s_prov_container);
    lv_label_set_text(instr, "Connect your phone\nto this WiFi:");
    lv_obj_set_style_text_font(instr, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(instr, lv_color_make(0xaa, 0xaa, 0xaa), 0);
    lv_obj_set_style_text_align(instr, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(instr, 16, 0);

    // SSID display
    s_prov_ssid_label = lv_label_create(s_prov_container);
    lv_label_set_text(s_prov_ssid_label, ap_ssid);
    lv_obj_set_style_text_font(s_prov_ssid_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_prov_ssid_label, kColorWhite, 0);
    lv_obj_set_style_text_align(s_prov_ssid_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(s_prov_ssid_label, 8, 0);

    // Sub-instruction
    auto* sub = lv_label_create(s_prov_container);
    lv_label_set_text(sub, "Then open browser");
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(sub, lv_color_make(0x99, 0x99, 0x99), 0);
    lv_obj_set_style_text_align(sub, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(sub, 20, 0);
}

void wifi_status_ui_hide_provision()
{
    if (s_prov_container)
    {
        lv_obj_del(s_prov_container);
        s_prov_container  = nullptr;
        s_prov_ssid_label = nullptr;
    }
}

}  // namespace rs520
