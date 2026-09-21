#include <remote_app.h>
#include <app_types.h>
#include <lvgl.h>
#include <esp_log.h>
#include <common_apis.h>
#include <global_locks.h>
#include <ui_theme.h>
#include <ui_utils.h>
#include <common_apis.h>
#include <remote_codes.h>

#include "assets/remote_ico.h"

static const char* TAG = "Remote App";
static application_t remote_app;
static lv_obj_t* background = NULL;
static lv_obj_t* temp_lbl = NULL;
static const size_t bluestar_payload_size = 6;

static void button_key_cb(lv_event_t* event) {
    if(lv_event_get_key(event) == LV_KEY_ENTER) {
        // Replace call and type with actual struct def 
        uint8_t* data = (uint8_t*)lv_event_get_user_data(event);
        assert(data);
        send_ir_packet(&remote_app, bluestar_ac.frequency, bluestar_ac.mark, bluestar_ac.bit0_space, 
                       bluestar_ac.mark, bluestar_ac.bit1_space, bluestar_ac.header_mark, bluestar_ac.header_space,
                       bluestar_ac.inter_frame_mark, bluestar_ac.inter_frame_space, bluestar_ac.gap_mark, 
                       bluestar_ac.gap_space, bluestar_payload_size, data, bluestar_ac.bursts);
        lv_event_stop_bubbling(event);
    }
}

static void temp_key_event_cb(lv_event_t* event) {
    switch(lv_event_get_key(event)) {
        case LV_KEY_RIGHT : 
            WITH_UI_LOCK() {
                assert(temp_lbl);
                char* temp_str = lv_label_get_text(temp_lbl);
                int temp = 24;
                if(sscanf(temp_str, "%d", &temp) == 1) {
                    if(temp >= 16 && temp < 30) {
                        temp = temp + 1;
                        lv_label_set_text_fmt(temp_lbl, "%d", temp);
                        size_t payload_idx = temp - 16;
                        assert(payload_idx <(sizeof(temp_payloads)/sizeof(temp_payloads[0])));
                        send_ir_packet(&remote_app, bluestar_ac.frequency, bluestar_ac.mark, bluestar_ac.bit0_space, 
                             bluestar_ac.mark, bluestar_ac.bit1_space, bluestar_ac.header_mark, bluestar_ac.header_space,
                             bluestar_ac.inter_frame_mark, bluestar_ac.inter_frame_space, bluestar_ac.gap_mark, 
                             bluestar_ac.gap_space, bluestar_payload_size, temp_payloads[payload_idx],
                             bluestar_ac.bursts);
                    }
                }
                else {
                    ESP_LOGE(TAG, "Unknown current temp, skipping");
                }
                lv_event_stop_bubbling(event);
            }
            break;
        case LV_KEY_LEFT :
            WITH_UI_LOCK() {
                assert(temp_lbl);
                char* temp_str = lv_label_get_text(temp_lbl);
                int temp = 24;
                if(sscanf(temp_str, "%d", &temp) == 1) {
                    if(temp > 16 && temp <= 30) {
                        temp = temp - 1;
                        lv_label_set_text_fmt(temp_lbl, "%d", temp);
                        // Call the fire IR packet api
                        size_t payload_idx = temp - 16;
                        assert(payload_idx <(sizeof(temp_payloads)/sizeof(temp_payloads[0])));
                        send_ir_packet(&remote_app, bluestar_ac.frequency, bluestar_ac.mark, bluestar_ac.bit0_space, 
                             bluestar_ac.mark, bluestar_ac.bit1_space, bluestar_ac.header_mark, bluestar_ac.header_space,
                             bluestar_ac.inter_frame_mark, bluestar_ac.inter_frame_space, bluestar_ac.gap_mark, 
                             bluestar_ac.gap_space, bluestar_payload_size, temp_payloads[payload_idx], 
                             bluestar_ac.bursts);
                    }
                }
                else {
                    ESP_LOGE(TAG, "Unknown current temp, skipping");
                }
                lv_event_stop_bubbling(event);
            }
            break;
        case LV_KEY_ENTER : 
            WITH_UI_LOCK() {
                lv_event_stop_bubbling(event);
            }
        default :
            break;
    }
}

static void draw_remote_app(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Starting remote app");
    WITH_UI_LOCK() {
        background = lv_obj_create(parent);
        lv_obj_set_size(background, lv_pct(100), lv_pct(100));
        lv_obj_set_style_pad_hor(background, 5, LV_STATE_DEFAULT);
        remove_shadow_and_outline(background);
        lv_obj_set_style_bg_color(background, COLOR_THEME_SECONDARY, LV_STATE_DEFAULT);
        lv_obj_add_flag(background, LV_OBJ_FLAG_EVENT_BUBBLE);

        lv_obj_t* power_bar = lv_obj_create(background);
        lv_obj_set_style_bg_opa(power_bar, LV_OPA_TRANSP, LV_STATE_DEFAULT);
        lv_obj_set_style_margin_hor(power_bar, 5, LV_STATE_DEFAULT);
        lv_obj_set_style_pad_ver(power_bar, 5, LV_STATE_DEFAULT);
        lv_obj_set_layout(power_bar, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(power_bar, LV_FLEX_FLOW_ROW);
        lv_obj_add_flag(power_bar, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_width(power_bar, lv_pct(100));
        lv_obj_align(power_bar, LV_ALIGN_TOP_MID, 0, 10);
        lv_obj_set_style_pad_column(power_bar, 5, LV_STATE_DEFAULT);
        lv_obj_set_flex_align(power_bar, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t* pwr_on_btn = lv_button_create(power_bar);
        make_obj_navigable(pwr_on_btn);
        lv_obj_set_style_pad_ver(pwr_on_btn, 10, LV_STATE_DEFAULT);
        lv_obj_set_style_pad_hor(pwr_on_btn, 5, LV_STATE_DEFAULT);
        lv_obj_set_width(pwr_on_btn, lv_pct(45));
        lv_obj_set_style_bg_color(pwr_on_btn, COLOR_THEME_TERTIARY, LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(pwr_on_btn, COLOR_THEME_TEXT_HIGLIGHT, LV_STATE_FOCUSED);
        
        lv_obj_t *pwr_on_lbl = lv_label_create(pwr_on_btn);
        lv_obj_center(pwr_on_lbl);
        lv_obj_set_style_text_font(pwr_on_lbl, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(pwr_on_lbl, COLOR_THEME_TEXT_PRIMARY, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text(pwr_on_lbl, "On");
        lv_obj_add_event_cb(pwr_on_btn, button_key_cb, LV_EVENT_KEY, (void*)power_on_payload);

        lv_obj_t* pwr_off_btn = lv_button_create(power_bar);
        make_obj_navigable(pwr_off_btn);
        lv_obj_set_style_pad_ver(pwr_off_btn, 10, LV_STATE_DEFAULT);
        lv_obj_set_style_pad_hor(pwr_off_btn, 5, LV_STATE_DEFAULT);
        lv_obj_set_width(pwr_off_btn, lv_pct(45));
        lv_obj_set_style_bg_color(pwr_off_btn, COLOR_THEME_TERTIARY, LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(pwr_off_btn, COLOR_THEME_TEXT_HIGLIGHT, LV_STATE_FOCUSED);
        lv_obj_add_flag(pwr_off_btn, LV_OBJ_FLAG_EVENT_BUBBLE);
        
        lv_obj_t *pwr_off_lbl = lv_label_create(pwr_off_btn);
        lv_obj_center(pwr_off_lbl);
        lv_obj_set_style_text_font(pwr_off_lbl, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(pwr_off_lbl, COLOR_THEME_TEXT_PRIMARY, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text(pwr_off_lbl, "Off");
        lv_obj_add_event_cb(pwr_off_btn, button_key_cb, LV_EVENT_KEY, (void*)power_off_payload);
        
        lv_obj_set_size(power_bar, lv_pct(100), LV_SIZE_CONTENT);

        lv_obj_t* temp_bar = lv_obj_create(background);
        make_obj_navigable(temp_bar);
        lv_obj_set_style_bg_color(temp_bar, COLOR_THEME_SECONDARY, LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(temp_bar, COLOR_THEME_TERTIARY, LV_STATE_FOCUSED);
        lv_obj_set_style_margin_hor(temp_bar, 20, LV_STATE_DEFAULT);
        lv_obj_set_style_pad_ver(temp_bar, 10, LV_STATE_DEFAULT);
        lv_obj_set_size(temp_bar, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_align(temp_bar, LV_ALIGN_CENTER, 0, 10);
        lv_obj_set_layout(temp_bar, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(temp_bar, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(temp_bar, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t *temp_bar_heading = lv_label_create(temp_bar);
        lv_obj_center(temp_bar_heading);
        lv_obj_align(temp_bar_heading, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_text_font(temp_bar_heading, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(temp_bar_heading, COLOR_THEME_TEXT_PRIMARY, LV_STATE_DEFAULT);
        lv_label_set_text(temp_bar_heading, "Temperature");
        
        temp_lbl = lv_label_create(temp_bar);
        make_obj_navigable(temp_lbl);
        lv_obj_center(temp_lbl);
        lv_obj_align(temp_lbl, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_text_font(temp_lbl, &lv_font_montserrat_30, 0);
        lv_obj_set_style_text_color(temp_lbl, COLOR_THEME_TEXT_PRIMARY, LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(temp_lbl, COLOR_THEME_TEXT_HIGLIGHT, LV_STATE_FOCUSED);
        lv_label_set_text(temp_lbl, "24");
        lv_obj_add_event_cb(temp_lbl, temp_key_event_cb, LV_EVENT_KEY, NULL);
    }
}

static void close_remote_app(void) {
    ESP_LOGI(TAG, "Closing remote app");
    WITH_UI_LOCK() {
        if(background) {
            lv_obj_delete(background);
            background = NULL;
            temp_lbl = NULL;
        }
    }
}

static application_t remote_app = {
    .app_perms = APP_PERM_SENSOR,
    .name = "Remote",
    .ico = &remote_ico,
    .close_app = close_remote_app,
    .draw_app = draw_remote_app,
};

application_t* get_remote_app(void) {
    return &remote_app;
}
