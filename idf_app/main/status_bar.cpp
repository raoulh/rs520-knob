#include "status_bar.h"

#include "lvgl.h"

namespace
{

static lv_obj_t* s_bar = nullptr;

// Bottom-center positioning for round 360×360 display
// Inset enough to stay within visible circular area
constexpr int kBarWidth     = 160;
constexpr int kBarHeight    = 32;
constexpr int kBottomInset  = 28;   // px from bottom edge (inside round bezel)

}  // namespace

namespace rs520
{

void status_bar_create()
{
    auto* scr = lv_scr_act();

    s_bar = lv_obj_create(scr);
    lv_obj_set_size(s_bar, kBarWidth, kBarHeight);

    // Bottom-center, inset from edge for round display
    lv_obj_align(s_bar, LV_ALIGN_BOTTOM_MID, 0, -kBottomInset);

    // Transparent, no border — just a layout container
    lv_obj_set_style_bg_opa(s_bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_bar, 0, 0);
    lv_obj_set_style_pad_all(s_bar, 0, 0);
    lv_obj_set_style_pad_column(s_bar, 12, 0);  // gap between items

    // Flex row layout, centered
    lv_obj_set_flex_flow(s_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_bar, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // No scrollbar
    lv_obj_remove_flag(s_bar, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t* status_bar_container()
{
    return s_bar;
}

}  // namespace rs520
