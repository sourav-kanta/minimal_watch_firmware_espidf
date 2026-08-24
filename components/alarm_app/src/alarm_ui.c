#include <alarm_app.h>
#include <ui_theme.h>
#include <ui_utils.h>
#include <common_types.h>
#include <app_types.h>
#include <esp_log.h>
#include <lvgl.h>
#include <global_locks.h>

#include "assets/alarm_ico.h"

static lv_obj_t* base_container = NULL;
static lv_obj_t* bg = NULL;
static const char* TAG = "Alarm app";
static popup_registry_t change_popup_registry;
static const lv_color_t COLOR_PRIMARY = { .red = 0xD4, .green = 0xAF, .blue = 0x37 };


static listview_t change_list[] = {
    {
        .title = "Edit",
        .value = NULL,
    },
    {
        .title = "Delete",
        .value = NULL,
    },
    {
        .title = "Copy",
        .value = NULL,
    },
};

static void popup_sel_cb(int index) {
    ESP_LOGI(TAG, "Selected : %s", change_list[index].title);
}

static void open_alarm_change_popup(void* data) {
    assert(bg);
    create_popup_listview(base_container, &change_popup_registry, change_list, 
                          sizeof(change_list) / sizeof(change_list[0]), popup_sel_cb);
}

static listview_t alarm_list[] = {
    {
        .title = "9:05 AM",
        .value = "22 Aug",
        .title_click_cb = open_alarm_change_popup,
    },
    {
        .title = "9:15 AM",
        .value = "22 Aug",
        .title_click_cb = open_alarm_change_popup,
    },
    {
        .title = "2:30 PM",
        .value = "24 Aug",
        .title_click_cb = open_alarm_change_popup,
    },
};

void draw_alarm_app_ui(lv_obj_t* parent) {
    assert(parent);
    WITH_UI_LOCK() {
        base_container = parent;
        bg = lv_obj_create(parent);
        lv_obj_set_style_bg_color(bg, COLOR_THEME_SECONDARY, LV_STATE_DEFAULT);
        lv_obj_set_scrollbar_mode(bg, LV_SCROLLBAR_MODE_OFF);
        remove_shadow_and_outline(bg);
        lv_obj_add_flag(bg, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_size(bg, lv_pct(100), lv_pct(100));
        lv_obj_set_layout(bg, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(bg, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_top(bg, 10, LV_STATE_DEFAULT);
        lv_obj_set_style_pad_hor(bg, 10, LV_STATE_DEFAULT);

        lv_obj_t* add_btn = lv_button_create(bg);
        make_obj_navigable(add_btn);
        lv_obj_set_style_pad_ver(add_btn, 6, LV_STATE_DEFAULT);
        lv_obj_set_style_radius(add_btn, 5, LV_STATE_DEFAULT);
        lv_obj_set_size(add_btn, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(add_btn, COLOR_PRIMARY, LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(add_btn, COLOR_THEME_TEXT_HIGLIGHT, LV_STATE_FOCUSED);
        lv_obj_t* btn_label = lv_label_create(add_btn);
        lv_obj_align(btn_label, LV_ALIGN_CENTER, 0, 0);
        lv_label_set_text(btn_label, "Add Alarm");
        lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_10, LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(btn_label, COLOR_THEME_SECONDARY, LV_STATE_DEFAULT);
        
        lv_obj_t* alarm_details = lv_obj_create(bg);
        lv_obj_add_flag(alarm_details, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_scrollbar_mode(alarm_details, LV_SCROLLBAR_MODE_OFF);
        remove_shadow_and_outline(alarm_details);
        lv_obj_set_style_bg_opa(alarm_details, LV_OPA_TRANSP, LV_STATE_DEFAULT);
        lv_obj_set_size(alarm_details, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_style_pad_top(alarm_details, 10, LV_STATE_DEFAULT);
        lv_obj_set_layout(alarm_details, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(alarm_details, LV_FLEX_FLOW_COLUMN);
        for(int i=0; i<(sizeof(change_list) / sizeof(change_list[0])); i++) {
            draw_horizoantal_divider(alarm_details);
            draw_listview_item(alarm_details, &alarm_list[i]);
        } 
    }
}

void delete_alarm_app_ui(void) {
    ESP_LOGI(TAG, "Closing alarm app");
    if(bg != NULL) {
        WITH_UI_LOCK() {
            lv_obj_delete(bg);
            if(base_container != NULL) lv_obj_clean(base_container);
        }
    }
    base_container = NULL;
    bg = NULL;
}

static application_t alarm_app = {
    .name = "Alarm",
    .app_perms = APP_PERM_BLE,
    .ico = &alarm_ico,
    .draw_app = draw_alarm_app_ui,
    .close_app = delete_alarm_app_ui,
};

application_t* get_alarm_app(void) {
    return &alarm_app;
}
