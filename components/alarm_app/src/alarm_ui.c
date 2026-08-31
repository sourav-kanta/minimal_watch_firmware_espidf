#include <alarm_app.h>
#include <ui_theme.h>
#include <ui_utils.h>
#include <common_types.h>
#include <app_types.h>
#include <esp_log.h>
#include <lvgl.h>
#include <global_locks.h>
#include <common_apis.h>
#include <draw_alarm_create_page.h>

#include "assets/alarm_ico.h"

static lv_obj_t* base_container = NULL;
static lv_obj_t* bg = NULL;
static lv_obj_t* alarm_details = NULL;
static const char* TAG = "Alarm app";
static popup_registry_t change_popup_registry;
static const lv_color_t COLOR_PRIMARY = { .red = 0xD4, .green = 0xAF, .blue = 0x37 };
static application_t alarm_app;
static alarm_t* all_alarms_snapshot = NULL;
static uint8_t n_alarms = 0;
static listview_t *alarm_list = NULL;
static listview_t *change_list = NULL;
static const char *alarm_change_options[] = { "Edit", "Copy", "Delete" };
static alarm_t *selected_alarm = NULL;

static int find_alarm_index(alarm_t* alarm) {
    assert(alarm);
    assert(all_alarms_snapshot);
    for(int i=0; i<n_alarms; i++) {
        if(&all_alarms_snapshot[i] == alarm) {
            return i;
        }
    }
    return -1;
}

static void popup_sel_cb(int index) {
    ESP_LOGI(TAG, "Selected : %s", change_list[index].title);
    if(index == 0) {
        // TODO - actually implement the edit using the edit_alarm_by_index
        // api instead of this hack that will fail if user cancels the edit
        draw_modify_alarm_page(base_container, selected_alarm);
        int del_idx = find_alarm_index(selected_alarm);
        if(del_idx>=0) delete_alarm_by_index(&alarm_app, del_idx);
        invalidate_and_repopulate_alarm_list();
        selected_alarm = NULL;
    }
    else if(index == 1) {
        draw_modify_alarm_page(base_container, selected_alarm);
        selected_alarm = NULL;
    }
    else if(index == 2) {
        int del_idx = find_alarm_index(selected_alarm);
        if(del_idx>=0) delete_alarm_by_index(&alarm_app, del_idx);
        invalidate_and_repopulate_alarm_list();
        selected_alarm = NULL;
    }
}

static void open_alarm_change_popup(void* data) {
    assert(bg);
    selected_alarm = (alarm_t*) data;
    create_popup_listview(base_container, &change_popup_registry, change_list, 
                          sizeof(alarm_change_options)/ sizeof(alarm_change_options[0]), popup_sel_cb);
}

static void add_alarm_click_cb(lv_event_t* event) {
    if(lv_event_get_key(event) == LV_KEY_ENTER) {
        draw_modify_alarm_page(base_container, NULL);
        lv_event_stop_bubbling(event);
    }
}

static void populate_alarms_list(lv_obj_t* parent) {
    assert(alarm_list == NULL);
    if(selected_alarm) selected_alarm = NULL;
    n_alarms = get_all_alarms(all_alarms_snapshot);
    if(n_alarms == 0) return;
    alarm_list = calloc(n_alarms, sizeof(listview_t));
    if(!alarm_list) {
        ESP_LOGE(TAG, "Error allocating memory, stopping app");
        return;
    }
    WITH_UI_LOCK() {
        for(int i=0; i<n_alarms; i++) {
            date_time_t alarm_date;
            get_date_time_from_epoch(all_alarms_snapshot[i].epoch, &alarm_date);
            char title[8];
            char value[16];
            snprintf(title, sizeof(title), "%02u:%02u", alarm_date.hr, alarm_date.min);
            snprintf(value, sizeof(value), "%02u/%02u/%04u", alarm_date.day, alarm_date.month, alarm_date.year);
            alarm_list[i].value = value;
            alarm_list[i].title = title;
            alarm_list[i].data = &all_alarms_snapshot[i];
            alarm_list[i].title_click_cb = open_alarm_change_popup;
            draw_horizoantal_divider(parent);
            draw_listview_item(parent, &alarm_list[i]);
        } 
    }
}

void invalidate_and_repopulate_alarm_list(void) {
    if(alarm_list) {
        free(alarm_list);
        alarm_list = NULL;
        selected_alarm = NULL;
    }
    assert(alarm_details);
    WITH_UI_LOCK() {
        lv_obj_clean(alarm_details);
    }
    populate_alarms_list(alarm_details);
}

void draw_alarm_app_ui(lv_obj_t* parent) {
    assert(parent);
    assert(change_list == NULL);
    change_list = calloc(sizeof(alarm_change_options) / sizeof(alarm_change_options[0]), sizeof(listview_t));
    if(!change_list) {
        ESP_LOGE(TAG, "Error allocating memory, stopping app");
        return;
    }
    for(int i=0; i<(sizeof(alarm_change_options)/sizeof(alarm_change_options[0])); i++){
        change_list[i].title = alarm_change_options[i];
    }
    assert(all_alarms_snapshot == NULL);
    all_alarms_snapshot = calloc(MAX_WATCH_ALARMS, sizeof(alarm_t));
    if(!all_alarms_snapshot) {
        ESP_LOGE(TAG, "Error allocating memory, stopping app");
        return;
    }

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
        lv_obj_add_event_cb(add_btn, add_alarm_click_cb, LV_EVENT_KEY, NULL); 
        
        alarm_details = lv_obj_create(bg);
        lv_obj_add_flag(alarm_details, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_scrollbar_mode(alarm_details, LV_SCROLLBAR_MODE_OFF);
        remove_shadow_and_outline(alarm_details);
        lv_obj_set_style_bg_opa(alarm_details, LV_OPA_TRANSP, LV_STATE_DEFAULT);
        lv_obj_set_size(alarm_details, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_style_pad_top(alarm_details, 10, LV_STATE_DEFAULT);
        lv_obj_set_layout(alarm_details, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(alarm_details, LV_FLEX_FLOW_COLUMN);
        
        populate_alarms_list(alarm_details);
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
    if(alarm_list) {
        free(alarm_list);
        alarm_list = NULL;
    }
    if(change_list) {
        free(change_list);
        change_list = NULL;
    }
    if(all_alarms_snapshot) {
        free(all_alarms_snapshot);
        all_alarms_snapshot = NULL;
    }
    n_alarms = 0;
    base_container = NULL;
    bg = NULL;
    alarm_details = NULL;
    selected_alarm = NULL;
}

static application_t alarm_app = {
    .name = "Alarm",
    .app_perms = APP_PERM_SYSTEM,
    .ico = &alarm_ico,
    .draw_app = draw_alarm_app_ui,
    .close_app = delete_alarm_app_ui,
};

application_t* get_alarm_app(void) {
    return &alarm_app;
}
