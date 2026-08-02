#include <settings_app.h>
#include <ui_theme.h>
#include <ui_utils.h>
#include <common_types.h>
#include <app_types.h>
#include <esp_log.h>
#include <app_utils.h>
#include <lvgl.h>
#include <global_locks.h>
#include <listview.h>

#include "assets/settings_ico.h"

static lv_obj_t* base_container = NULL;
static const int divider_width = 1;
static listview_t list_items[] = { 
    {
        .title = "Watchface",
        .value = "Moon",
        .title_click_cb = NULL
    },
    {
        .title = "Sleep timeout",
        .value = "5",
        .title_click_cb = NULL
    },
    {
        .title = "Brightness",
        .value = "100%",
        .title_click_cb = NULL
    },
    {
        .title = "Unpair Phone",
        .value = NULL,
        .title_click_cb = NULL
    },
};

static void draw_horizoantal_divider(lv_obj_t* parent) {
    lv_obj_t* div = lv_obj_create(parent);
    lv_obj_set_size(div, lv_pct(100), divider_width);
    lv_obj_align(div, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(div, COLOR_THEME_FOCUS_BG, LV_STATE_DEFAULT); 
}

void draw_settings_app_ui(lv_obj_t* parent) {
    
    WITH_UI_LOCK() {
        base_container = lv_obj_create(parent);
        remove_shadow_and_outline(base_container);
        lv_obj_set_style_pad_all(base_container, 5, LV_STATE_DEFAULT);
        lv_obj_set_size(base_container, lv_pct(100), lv_pct(100));
        lv_obj_set_layout(base_container, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(base_container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_bg_color(base_container, COLOR_THEME_SECONDARY, LV_STATE_DEFAULT);
        lv_obj_add_flag(base_container, LV_OBJ_FLAG_EVENT_BUBBLE);

        draw_horizoantal_divider(base_container);

        int item_count = sizeof(list_items)/sizeof(list_items[0]);
        for(int i=0 ; i<item_count; i++) {
            draw_listview_item(base_container, &list_items[i]);
            draw_horizoantal_divider(base_container);
        }
    }
}

void delete_settings_app_ui(void) {
    lv_obj_delete(base_container);
    base_container = NULL;
}

static application_t settings_app = {
    .name = "Settings",
    .ico = &settings_ico,
    .draw_app = draw_settings_app_ui,
    .close_app = delete_settings_app_ui,
};

application_t* get_settings_app(void) {
    return &settings_app;
}
