#include <stopwatch_app.h>
#include <ui_theme.h>
#include <ui_utils.h>
#include <common_types.h>
#include <app_types.h>
#include <esp_log.h>
#include <lvgl.h>
#include <global_locks.h>
#include <common_apis.h>

#include "assets/stopwatch_ico.h"

static const char* TAG = "Stopwatch app";
static lv_obj_t* background = NULL;
static lv_obj_t *lap_values[3];
static lv_obj_t* start_btn_lbl = NULL;
static lv_obj_t* time_h_lbl = NULL;
static lv_obj_t* time_l_lbl = NULL;
static int n_laps = 0;
static lv_timer_t* stopwatch_timer = NULL;
static const uint32_t TIMER_INTERVAL = 120;
static const uint32_t TIMER_INTERVAL_DIV_10 = TIMER_INTERVAL / 10;
static uint32_t elapsed_ms_div_10 = 0;
static bool is_wakelock_acquired = false;
static application_t stopwatch_app;

static void stopwatch_timer_cb(lv_timer_t* timer) {
    elapsed_ms_div_10 += TIMER_INTERVAL_DIV_10;
    WITH_UI_LOCK() {
        lv_label_set_text_fmt(time_l_lbl, "%02lu", elapsed_ms_div_10%100);
        lv_label_set_text_fmt(time_h_lbl, "%02lu:%02lu", elapsed_ms_div_10/(60*100), 
                              (elapsed_ms_div_10/100)%60);
    }    
}

static void start_stop_btn_click_cb(lv_event_t* event) {
    if(lv_event_get_key(event) == LV_KEY_ENTER) {
        if(stopwatch_timer == NULL) {
            // Started stopwatch
            WITH_UI_LOCK() {
                stopwatch_timer = lv_timer_create(stopwatch_timer_cb, TIMER_INTERVAL, NULL);
                assert(start_btn_lbl);
                lv_label_set_text(start_btn_lbl, "Stop");
                n_laps = 0;
                lv_label_set_text_fmt(lap_values[0], "00:00:00");
                lv_label_set_text_fmt(lap_values[1], "00:00:00");
                lv_label_set_text_fmt(lap_values[2], "00:00:00");
            }
            assert(is_wakelock_acquired == false);
            if(acquire_wakelock(&stopwatch_app)) {
                is_wakelock_acquired = true;
            }
            else {
                ESP_LOGE(TAG, "Failed to acquire wakelock");
            }
        }
        else {
            // Stopped stopwatch
            WITH_UI_LOCK() {
                lv_timer_delete(stopwatch_timer);
                stopwatch_timer = NULL;
                lv_label_set_text(start_btn_lbl, "Start");
                // Update current lap
                if(n_laps == 3) {
                    lv_label_set_text(lap_values[0], lv_label_get_text(lap_values[1]));
                    lv_label_set_text(lap_values[1], lv_label_get_text(lap_values[2]));
                    n_laps--;
                }

                lv_label_set_text_fmt(lap_values[n_laps], "%02lu:%02lu:%02lu",  elapsed_ms_div_10/(60*100),
                                      (elapsed_ms_div_10/100)%60, elapsed_ms_div_10%100);
                elapsed_ms_div_10 = 0; 
                lv_label_set_text(time_l_lbl, "00");
                lv_label_set_text(time_h_lbl, "00:00");
            }
            assert(is_wakelock_acquired == true);
            if(release_wakelock(&stopwatch_app)) {
                is_wakelock_acquired = false;
            }
            else {
                ESP_LOGE(TAG, "Failed to release wakelock");
            }
        }
        lv_event_stop_bubbling(event);
    }
}

static void lap_btn_click_cb(lv_event_t* event) {
    if(lv_event_get_key(event) == LV_KEY_ENTER) {
        if(stopwatch_timer == NULL) {
            ESP_LOGE(TAG, "Timer not running, ignore lap btn");
        }
        else {
            uint32_t lap_elapsed = elapsed_ms_div_10;
            if(n_laps == 3) {
                WITH_UI_LOCK() {
                    lv_label_set_text(lap_values[0], lv_label_get_text(lap_values[1]));
                    lv_label_set_text(lap_values[1], lv_label_get_text(lap_values[2]));
                }
                n_laps--;
            }

            WITH_UI_LOCK() {
                lv_label_set_text_fmt(lap_values[n_laps++], "%02lu:%02lu:%02lu",  lap_elapsed/(60*100),
                                      (lap_elapsed/100)%60, lap_elapsed%100);
            }
        }
        lv_event_stop_bubbling(event);
    }
} 

void draw_listview_card(lv_obj_t* base_obj, const char* title, const char* value, lv_obj_t** out_val) {
    assert(base_obj);
    assert(title);
    assert(value);

    lv_obj_t* card = lv_obj_create(base_obj);
    lv_obj_set_style_bg_opa(card, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    remove_shadow_and_outline(card);
    lv_obj_set_size(card, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(card, 2, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_ver(card, 5, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_STATE_FOCUSED);
    lv_obj_set_style_bg_color(card, COLOR_THEME_FOCUS_BG, LV_STATE_FOCUSED);
    lv_obj_add_flag(card, LV_OBJ_FLAG_EVENT_BUBBLE);
    
    lv_obj_t* label_title = lv_label_create(card);
    lv_obj_set_style_text_font(label_title, &lv_font_montserrat_10, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label_title, COLOR_THEME_TEXT_PRIMARY, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label_title, COLOR_THEME_TEXT_HIGLIGHT, LV_STATE_FOCUSED);
    lv_obj_align(label_title, LV_ALIGN_LEFT_MID, 0, 0);
    lv_label_set_text(label_title, title);
    
    *out_val = lv_label_create(card);
    lv_obj_set_style_text_font(*out_val, &lv_font_montserrat_10, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(*out_val, COLOR_THEME_TEXT_PRIMARY, LV_STATE_DEFAULT);
    lv_obj_align(*out_val, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_label_set_text(*out_val, value);
}

void draw_stopwatch_app_ui(lv_obj_t* parent) {
    assert(parent);
    WITH_UI_LOCK() {
        background = lv_obj_create(parent);
        lv_obj_set_size(background, lv_pct(100), lv_pct(100));
        lv_obj_set_style_bg_color(background, COLOR_THEME_SECONDARY, LV_STATE_DEFAULT);
        lv_obj_add_flag(background, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_style_pad_all(background, 5, LV_STATE_DEFAULT);
        lv_obj_set_style_pad_top(background, 15, LV_STATE_DEFAULT);

        lv_obj_t* time_container = lv_obj_create(background);
        lv_obj_set_style_bg_opa(time_container, LV_OPA_TRANSP, LV_STATE_DEFAULT);
        lv_obj_set_layout(time_container, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(time_container, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(time_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
        lv_obj_set_style_pad_column(time_container, 3, LV_STATE_DEFAULT);

        time_h_lbl = lv_label_create(time_container);
        lv_obj_set_style_text_font(time_h_lbl, &lv_font_montserrat_30, LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(time_h_lbl, COLOR_THEME_TEXT_PRIMARY, LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(time_h_lbl, LV_OPA_TRANSP, LV_STATE_DEFAULT);
        lv_label_set_text(time_h_lbl, "00:00");

        time_l_lbl = lv_label_create(time_container);
        lv_obj_set_style_text_font(time_l_lbl, &lv_font_montserrat_14, LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(time_l_lbl, COLOR_THEME_TEXT_PRIMARY, LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(time_l_lbl, LV_OPA_TRANSP, LV_STATE_DEFAULT);
        lv_label_set_text(time_l_lbl, "00");
        lv_obj_set_style_pad_bottom(time_l_lbl, 3, LV_STATE_DEFAULT);

        lv_obj_align(time_container, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_size(time_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

        lv_obj_t* btn_container = lv_obj_create(background);
        lv_obj_set_style_bg_opa(btn_container, LV_OPA_TRANSP, LV_STATE_DEFAULT);
        lv_obj_set_layout(btn_container, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(btn_container, 5, LV_STATE_DEFAULT);
        lv_obj_set_size(btn_container, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_add_flag(btn_container, LV_OBJ_FLAG_EVENT_BUBBLE);

        lv_obj_t* start_btn = lv_button_create(btn_container);
        make_obj_navigable(start_btn);
        lv_obj_set_style_pad_ver(start_btn, 6, LV_STATE_DEFAULT);
        lv_obj_set_style_radius(start_btn, 5, LV_STATE_DEFAULT);
        lv_obj_set_size(start_btn, lv_pct(45), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(start_btn, COLOR_THEME_TERTIARY, LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(start_btn, COLOR_THEME_TEXT_HIGLIGHT, LV_STATE_FOCUSED);
        start_btn_lbl = lv_label_create(start_btn);
        lv_obj_align(start_btn_lbl, LV_ALIGN_CENTER, 0, 0);
        lv_label_set_text(start_btn_lbl, "Start");
        lv_obj_set_style_text_font(start_btn_lbl, &lv_font_montserrat_10, LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(start_btn_lbl, COLOR_THEME_TEXT_PRIMARY, LV_STATE_DEFAULT);
        lv_obj_add_event_cb(start_btn, start_stop_btn_click_cb, LV_EVENT_KEY, NULL); 

        lv_obj_t* lap_btn = lv_button_create(btn_container);
        make_obj_navigable(lap_btn);
        lv_obj_set_style_pad_ver(lap_btn, 6, LV_STATE_DEFAULT);
        lv_obj_set_style_radius(lap_btn, 5, LV_STATE_DEFAULT);
        lv_obj_set_size(lap_btn, lv_pct(45), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(lap_btn, COLOR_THEME_TERTIARY, LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(lap_btn, COLOR_THEME_TEXT_HIGLIGHT, LV_STATE_FOCUSED);
        lv_obj_t* lap_btn_lbl = lv_label_create(lap_btn);
        lv_obj_align(lap_btn_lbl, LV_ALIGN_CENTER, 0, 0);
        lv_label_set_text(lap_btn_lbl, "Lap");
        lv_obj_set_style_text_font(lap_btn_lbl, &lv_font_montserrat_10, LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(lap_btn_lbl, COLOR_THEME_TEXT_PRIMARY, LV_STATE_DEFAULT);
        lv_obj_add_event_cb(lap_btn, lap_btn_click_cb, LV_EVENT_KEY, NULL); 
        
        lv_obj_align_to(btn_container, time_container, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
        
        lv_obj_t* lap_container = lv_obj_create(background);
        lv_obj_set_style_bg_opa(lap_container, LV_OPA_TRANSP, LV_STATE_DEFAULT);
        lv_obj_set_layout(lap_container, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(lap_container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(lap_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END);
        lv_obj_add_flag(lap_container, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_style_pad_hor(lap_container, 5, LV_STATE_DEFAULT);
        lv_obj_set_size(lap_container, lv_pct(100), LV_SIZE_CONTENT);

        draw_horizoantal_divider(lap_container);
        draw_listview_card(lap_container, "LAP 1", "00:00:00", &lap_values[0]);
        draw_horizoantal_divider(lap_container);
        draw_listview_card(lap_container, "LAP 2", "00:00:00", &lap_values[1]);
        draw_horizoantal_divider(lap_container);
        draw_listview_card(lap_container, "LAP 3", "00:00:00", &lap_values[2]);
        draw_horizoantal_divider(lap_container);
        
        lv_obj_align_to(lap_container, btn_container, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
    }
}

void delete_stopwatch_app_ui(void) {
    if(stopwatch_timer) {
        WITH_UI_LOCK() {
            lv_timer_delete(stopwatch_timer);
            stopwatch_timer = NULL;
        }
    }
    if(background) {
        WITH_UI_LOCK() {
            lv_obj_delete(background);
        }
        background = NULL;
    }
    start_btn_lbl = NULL;
    time_h_lbl = NULL;
    time_l_lbl = NULL;
    n_laps = 0;
    elapsed_ms_div_10 = 0;
    lap_values[0] = lap_values[1] = lap_values[2] = NULL;
    if(is_wakelock_acquired) {
        if(!release_wakelock(&stopwatch_app)) {
            ESP_LOGE(TAG, "Error releasing wakelock");
        }
        else {
            is_wakelock_acquired = false;
        }
    }
}

static application_t stopwatch_app = {
    .name = "Stopwatch",
    .app_perms = APP_PERM_WAKELOCK,
    .ico = &stopwatch_ico,
    .draw_app = draw_stopwatch_app_ui,
    .close_app = delete_stopwatch_app_ui,
};

application_t* get_stopwatch_app(void) {
    return &stopwatch_app;
}
