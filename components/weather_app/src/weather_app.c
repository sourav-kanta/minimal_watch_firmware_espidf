#include <lvgl.h>
#include <string.h>
#include <ui_theme.h>
#include <ui_utils.h>
#include <time.h>
#include <common_types.h>
#include <app_types.h>
#include <stdio.h>
#include <esp_log.h>
#include <common_apis.h>
#include <app_utils.h>

#include "assets/weather_bg.h"
#include "assets/cloudy.h"
#include "assets/rainy.h"
#include "assets/sunny.h"
#include "assets/cloud.h"

#define TOTAL_HOURS 24

static const char* TAG = "Weather app";
static const char HEX_COLOR_TEXT_PRIMARY[]         = "#28799c";
static const char HEX_COLOR_TEXT_PRIMARY_LIGHT[]   = "#e3e2e6";
static const char HEX_COLOR_TEXT_SECONDARY[]       = "#4f8ee0";
static const char HEX_COLOR_TEXT_TERTIARY[]        = "#38bf36";
static const char HEX_COLOR_TEXT_TERTIARY_LIGHT[]  = "#85f29f";
static const char HEX_COLOR_FOCUS_BG[]             = "#f02939";
static const char HEX_COLOR_FOCUS_BG_LIGHT[]       = "#ed0510";

static const int TIME_CONVERSION_SECONDS_PER_DAY = 86400;
static const int ZOOM_SCALE_MAIN_ICON            = 180;
static const int ZOOM_SCALE_HOURLY_ICON          = 100;
static const int SIZE_LOADING_SPINNER_DIMENSION  = 30;

static hourly_weather_t hourly_forecast[TOTAL_HOURS];
static date_time_t dt;

static lv_obj_t *date_container;
static lv_obj_t *date_lbl;
static lv_obj_t *current_temp_lbl;
static lv_obj_t *humidity_lbl;
static lv_obj_t *precip_lbl;
static lv_obj_t *wind_lbl;
static lv_obj_t *main_icon;

static lv_obj_t *hourly_time_lbl;
static lv_obj_t *hourly_temp_lbl;
static lv_obj_t *hourly_hum_lbl;
static lv_obj_t *hourly_wind_lbl;
static lv_obj_t *hourly_icon_img;

static lv_obj_t *bottom_panel_container;
static lv_obj_t *single_card;
static lv_obj_t *loading_spinner = NULL;

static int current_hour_offset = 0;
static int date_day_offset = 0;
static bool date_active_held = false;
static bool custom_date_active = false; 

static const char *days_of_week[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static const char *months_of_year[] = {
    "", "Jan", "Feb", "Mar", "Apr", "May", "Jun", 
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static void trigger_dated_weather_request(const date_time_t *target_date);

static const lv_img_dsc_t* get_icon_from_code(uint8_t code) {
    switch (code) {
        case 0: case 1:
            return &sunny;
        case 2: case 3: case 45: case 48:
            return &cloud;
        case 51: case 53: case 55: case 56: case 57:
        case 61: case 63: case 65: case 66: case 67:
        case 80: case 81: case 82: case 95: case 96: case 99:
            return &rainy;
        default: 
            return &sunny;
    }
}

static void update_card_data(const char *lbl_clr, const char *val_clr) {
    if (!hourly_time_lbl || !hourly_temp_lbl || !hourly_hum_lbl || !hourly_wind_lbl || !hourly_icon_img) {
        return;
    }

    if (current_hour_offset < 0) current_hour_offset = 0;
    if (current_hour_offset >= TOTAL_HOURS) current_hour_offset = TOTAL_HOURS - 1;

    uint8_t target_raw_hour = (uint8_t)current_hour_offset;
    hourly_weather_t data = hourly_forecast[target_raw_hour];

    uint8_t display_hour = target_raw_hour % 12;
    if (display_hour == 0) display_hour = 12;
    const char *ampm = (target_raw_hour >= 12) ? "PM" : "AM";

    int display_temp = data.temperature / 10;

    char time_str[64];
    snprintf(time_str, sizeof(time_str), "%s %d %s#", val_clr, display_hour, ampm);
    lv_label_set_text(hourly_time_lbl, time_str);

    char temp_str[64];
    snprintf(temp_str, sizeof(temp_str), "%s TEMP:# %s %d°C#", lbl_clr, val_clr, display_temp);
    lv_label_set_text(hourly_temp_lbl, temp_str);

    char hum_str[64];
    snprintf(hum_str, sizeof(hum_str), "%s HUMID:# %s %d%%#", lbl_clr, val_clr, data.humidity);
    lv_label_set_text(hourly_hum_lbl, hum_str);

    char wind_str[64];
    snprintf(wind_str, sizeof(wind_str), "%s WIND:# %s %dkm/h#", lbl_clr, val_clr, data.wind_speed);
    lv_label_set_text(hourly_wind_lbl, wind_str);

    lv_img_set_src(hourly_icon_img, get_icon_from_code(data.weather_code));
}

static void reset_weather_data(void) {
    memset(hourly_forecast, 0, sizeof(hourly_forecast));

    if (current_temp_lbl) lv_label_set_text(current_temp_lbl, "0°C");
    if (humidity_lbl)     lv_label_set_text(humidity_lbl, "H: 0%");
    if (precip_lbl)        lv_label_set_text(precip_lbl, "P: 0%");
    if (wind_lbl)          lv_label_set_text(wind_lbl, "W: 0km/h");
    if (main_icon)        lv_img_set_src(main_icon, &sunny);

    update_card_data(HEX_COLOR_TEXT_SECONDARY, HEX_COLOR_TEXT_PRIMARY);
}

static void update_date_string(const char *forced_color, bool commit_new_date) {
    if (!date_lbl) return;

    const char *date_clr = (forced_color) ? forced_color : HEX_COLOR_TEXT_PRIMARY; 

    struct tm broken_time = {0};
    broken_time.tm_year = dt.year - 1900; 
    broken_time.tm_mon  = dt.month - 1;   
    broken_time.tm_mday = dt.day;
    broken_time.tm_hour = dt.hr;
    broken_time.tm_min  = dt.min;
    broken_time.tm_sec  = dt.sec;
    broken_time.tm_isdst = -1;

    time_t epoch_seconds = mktime(&broken_time);
    epoch_seconds += (time_t)date_day_offset * TIME_CONVERSION_SECONDS_PER_DAY;

    struct tm *normalized_time = localtime(&epoch_seconds);

    uint8_t wday = (normalized_time->tm_wday < 7) ? normalized_time->tm_wday : 0;
    uint8_t mon  = (normalized_time->tm_mon >= 0 && normalized_time->tm_mon < 12) ?
                     (normalized_time->tm_mon + 1) : 1;
    int display_day = normalized_time->tm_mday;

    if (commit_new_date) {
        dt.day   = (uint8_t)display_day;
        dt.month = (uint8_t)mon;
        dt.year  = (uint16_t)(normalized_time->tm_year + 1900);
        date_day_offset = 0; 
        custom_date_active = true; 
    }

    char date_str[64];
    snprintf(date_str, sizeof(date_str), "%s %s, %s %d#",
             date_clr, days_of_week[wday], months_of_year[mon], display_day);
       
    lv_label_set_text(date_lbl, date_str);
}

static void get_offset_date_snapshot(date_time_t *out_date) {
    struct tm broken_time = {0};
    broken_time.tm_year = dt.year - 1900; 
    broken_time.tm_mon  = dt.month - 1;   
    broken_time.tm_mday = dt.day;
    broken_time.tm_hour = dt.hr;
    broken_time.tm_min  = dt.min;
    broken_time.tm_sec  = dt.sec;
    broken_time.tm_isdst = -1;

    time_t epoch_seconds = mktime(&broken_time);
    epoch_seconds += (time_t)date_day_offset * TIME_CONVERSION_SECONDS_PER_DAY;

    struct tm *normalized_time = localtime(&epoch_seconds);
    
    uint8_t mon = (normalized_time->tm_mon >= 0 && normalized_time->tm_mon < 12) ?
                  (normalized_time->tm_mon + 1) : 1;

    out_date->day   = (uint8_t)normalized_time->tm_mday;
    out_date->month = (uint8_t)mon;
    out_date->year  = (uint16_t)(normalized_time->tm_year + 1900);
}

static void refresh_weather_ui_data(void) {
    if (!current_temp_lbl || !humidity_lbl || !precip_lbl || !wind_lbl || !main_icon) {
        ESP_LOGW(TAG, "Weather UI elements not active. Data cached cleanly in background.");
        return;
    }

    if (loading_spinner) {
        lv_obj_del(loading_spinner);
        loading_spinner = NULL;
    }
    if (single_card) {
        lv_obj_clear_flag(single_card, LV_OBJ_FLAG_HIDDEN);
    }

    ESP_LOGI(TAG, "Refreshing weather UI elements with fresh BLE payload data...");

    uint8_t target_hour = 0;
    
    if (!custom_date_active) {
        get_date_time(&dt); 
        target_hour = dt.hr % TOTAL_HOURS;
    } else {
        target_hour = 0;
    }

    hourly_weather_t current_now = hourly_forecast[target_hour];
    int current_main_temp = current_now.temperature / 10;

    char cur_temp_str[16];
    snprintf(cur_temp_str, sizeof(cur_temp_str), "%d°C", current_main_temp);
    lv_label_set_text(current_temp_lbl, cur_temp_str);

    char cur_hum_str[16];
    snprintf(cur_hum_str, sizeof(cur_hum_str), "H: %d%%", current_now.humidity);
    lv_label_set_text(humidity_lbl, cur_hum_str);

    char cur_precip_str[16];
    snprintf(cur_precip_str, sizeof(cur_precip_str), "P: %d%%", current_now.precip_prob);
    lv_label_set_text(precip_lbl, cur_precip_str);

    char cur_wind_str[16];
    snprintf(cur_wind_str, sizeof(cur_wind_str), "W: %dkm/h", current_now.wind_speed);
    lv_label_set_text(wind_lbl, cur_wind_str);

    lv_img_set_src(main_icon, get_icon_from_code(current_now.weather_code));

    current_hour_offset = (int)target_hour;
    
    if (single_card && lv_obj_has_state(single_card, LV_STATE_FOCUSED)) {
        update_card_data(HEX_COLOR_TEXT_SECONDARY, HEX_COLOR_FOCUS_BG);
    } else if (bottom_panel_container && lv_obj_has_state(bottom_panel_container, LV_STATE_FOCUSED)) {
        update_card_data(HEX_COLOR_TEXT_SECONDARY, HEX_COLOR_TEXT_TERTIARY);
    } else {
        update_card_data(HEX_COLOR_TEXT_SECONDARY, HEX_COLOR_TEXT_PRIMARY);
    }

    update_date_string(HEX_COLOR_TEXT_TERTIARY_LIGHT, false);
}

static void date_key_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        
        if (key == LV_KEY_ENTER) {
            date_active_held = !date_active_held;
            
            if (!date_active_held) {
                date_time_t query_date;
                get_offset_date_snapshot(&query_date);
                update_date_string(HEX_COLOR_FOCUS_BG_LIGHT, true); 
                trigger_dated_weather_request(&query_date);
            } else {
                update_date_string(HEX_COLOR_FOCUS_BG_LIGHT, false);
            }
            
            lv_event_stop_bubbling(e);
        }
        else if (key == LV_KEY_RIGHT) {
            if (date_active_held) {
                date_day_offset++;
                update_date_string(HEX_COLOR_FOCUS_BG_LIGHT, false); 
                lv_event_stop_bubbling(e);
            }
        } 
        else if (key == LV_KEY_LEFT) {
            if (date_active_held) {
                date_day_offset--;
                update_date_string(HEX_COLOR_FOCUS_BG_LIGHT, false); 
                lv_event_stop_bubbling(e);
            }
        }
    }
    else if (code == LV_EVENT_FOCUSED) {
        update_date_string(HEX_COLOR_TEXT_TERTIARY_LIGHT, false); 
        lv_event_stop_bubbling(e);
    }
    else if (code == LV_EVENT_DEFOCUSED) {
        if (date_active_held) {
            date_active_held = false;
            date_time_t query_date;
            get_offset_date_snapshot(&query_date);
            update_date_string(HEX_COLOR_TEXT_PRIMARY_LIGHT, true); 
            trigger_dated_weather_request(&query_date);
        } else {
            update_date_string(HEX_COLOR_TEXT_PRIMARY_LIGHT, false); 
        }
        lv_event_stop_bubbling(e);
    }
}

static void card_key_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        
        if (key == LV_KEY_RIGHT) {
            if (current_hour_offset >= (TOTAL_HOURS - 1)) return;
            current_hour_offset++;
            update_card_data(HEX_COLOR_TEXT_SECONDARY, HEX_COLOR_FOCUS_BG);
            lv_event_stop_bubbling(e);
        } 
        else if (key == LV_KEY_LEFT) {
            if (current_hour_offset <= 0) return;
            current_hour_offset--;
            update_card_data(HEX_COLOR_TEXT_SECONDARY, HEX_COLOR_FOCUS_BG);
            lv_event_stop_bubbling(e);
        }
    }
    else if (code == LV_EVENT_FOCUSED) {
        update_card_data(HEX_COLOR_TEXT_SECONDARY, HEX_COLOR_FOCUS_BG); 
        lv_event_stop_bubbling(e);              
    }
    else if (code == LV_EVENT_DEFOCUSED) {
        update_card_data(HEX_COLOR_TEXT_SECONDARY, HEX_COLOR_TEXT_TERTIARY); 
        lv_event_stop_bubbling(e);              
    }
}

static void container_key_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        
        if (key == LV_KEY_ENTER) {
            ESP_LOGI(TAG, "Container Enter pressed -> Focusing single_card");
            if (single_card) lv_group_focus_obj(single_card);
            lv_event_stop_bubbling(e); 
        }
    }
    else if (code == LV_EVENT_FOCUSED) {
        update_card_data(HEX_COLOR_TEXT_SECONDARY, HEX_COLOR_TEXT_TERTIARY); 
    }
    else if (code == LV_EVENT_DEFOCUSED) {
        update_card_data(HEX_COLOR_TEXT_SECONDARY, HEX_COLOR_TEXT_PRIMARY); 
    }
}

static void draw_app_ui(lv_obj_t* parent) {
    date_active_held = false;
    date_day_offset = 0;
    custom_date_active = false; 
    loading_spinner = NULL; 
    
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    get_weather_day(hourly_forecast);
    get_date_time(&dt);    
    
    uint8_t live_hour = dt.hr % 24;
    hourly_weather_t current_now = hourly_forecast[live_hour];
    int current_main_temp = current_now.temperature / 10;

    current_hour_offset = live_hour + 1;
    if (current_hour_offset >= TOTAL_HOURS) {
        current_hour_offset = TOTAL_HOURS - 1; 
    }

    lv_obj_t *bg_img = lv_img_create(parent);
    lv_img_set_src(bg_img, &weather_bg);
    lv_obj_align(bg_img, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_size(bg_img, LV_PCT(100), LV_PCT(100));

    current_temp_lbl = lv_label_create(parent);
    char cur_temp_str[16];
    snprintf(cur_temp_str, sizeof(cur_temp_str), "%d°C", current_main_temp);
    lv_label_set_text(current_temp_lbl, cur_temp_str);
    lv_obj_set_style_text_font(current_temp_lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(current_temp_lbl, COLOR_THEME_TEXT_PRIMARY, 0);
    lv_obj_align(current_temp_lbl, LV_ALIGN_TOP_LEFT, 10, 10);

    humidity_lbl = lv_label_create(parent);
    char cur_hum_str[16];
    snprintf(cur_hum_str, sizeof(cur_hum_str), "H: %d%%", current_now.humidity);
    lv_label_set_text(humidity_lbl, cur_hum_str);
    lv_obj_set_style_text_font(humidity_lbl, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(humidity_lbl, COLOR_THEME_TEXT_SECONDARY, 0);
    lv_obj_align(humidity_lbl, LV_ALIGN_TOP_LEFT, 10, 38);

    precip_lbl = lv_label_create(parent);
    char cur_precip_str[16];
    snprintf(cur_precip_str, sizeof(cur_precip_str), "P: %d%%", current_now.precip_prob);
    lv_label_set_text(precip_lbl, cur_precip_str);
    lv_obj_set_style_text_font(precip_lbl, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(precip_lbl, COLOR_THEME_TEXT_SECONDARY, 0);
    lv_obj_align(precip_lbl, LV_ALIGN_TOP_LEFT, 10, 50);

    wind_lbl = lv_label_create(parent);
    char cur_wind_str[16];
    snprintf(cur_wind_str, sizeof(cur_wind_str), "W: %dkm/h", current_now.wind_speed);
    lv_label_set_text(wind_lbl, cur_wind_str);
    lv_obj_set_style_text_font(wind_lbl, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(wind_lbl, COLOR_THEME_TEXT_SECONDARY, 0);
    lv_obj_align(wind_lbl, LV_ALIGN_TOP_LEFT, 10, 62);

    main_icon = lv_img_create(parent);
    lv_img_set_src(main_icon, get_icon_from_code(current_now.weather_code));
    lv_img_set_zoom(main_icon, ZOOM_SCALE_MAIN_ICON);
    lv_obj_align(main_icon, LV_ALIGN_TOP_RIGHT, -2, 12);

    date_container = lv_obj_create(parent);
    lv_obj_set_style_bg_opa(date_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(date_container, 0, 0);
    lv_obj_set_style_outline_width(date_container, 0, 0);
    lv_obj_set_style_pad_all(date_container, 0, 0);
    lv_obj_set_size(date_container, LV_PCT(100), 20);
    lv_obj_align(date_container, LV_ALIGN_TOP_MID, 0, 80);
    lv_obj_clear_flag(date_container, LV_OBJ_FLAG_SCROLLABLE);

    date_lbl = lv_label_create(date_container);
    lv_label_set_recolor(date_lbl, true); 
    lv_obj_set_style_text_font(date_lbl, &lv_font_montserrat_10, 0);
    lv_obj_align(date_lbl, LV_ALIGN_CENTER, 0, 0);

    lv_obj_add_flag(date_container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(date_container, date_key_event_cb, LV_EVENT_ALL, NULL);
    make_obj_navigable(date_container);

    update_date_string(HEX_COLOR_TEXT_PRIMARY_LIGHT, false);

    bottom_panel_container = lv_obj_create(parent);
    lv_obj_set_style_bg_opa(bottom_panel_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bottom_panel_container, 0, 0);
    lv_obj_set_style_outline_width(bottom_panel_container, 0, 0);
    lv_obj_set_style_pad_all(bottom_panel_container, 0, 0);
    lv_obj_set_size(bottom_panel_container, LV_PCT(100), 46);
    lv_obj_align(bottom_panel_container, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_obj_clear_flag(bottom_panel_container, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_add_flag(bottom_panel_container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(bottom_panel_container, container_key_event_cb, LV_EVENT_ALL, NULL);

    single_card = lv_obj_create(bottom_panel_container);
    lv_obj_set_style_bg_opa(single_card, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(single_card, 0, 0);
    lv_obj_set_style_outline_width(single_card, 0, 0);
    lv_obj_set_style_pad_all(single_card, 0, 0);
    lv_obj_set_style_pad_hor(single_card, 5, 0);
    lv_obj_set_size(single_card, LV_PCT(100), 44);
    lv_obj_align(single_card, LV_ALIGN_CENTER, 0, 0);
    
    lv_obj_set_layout(single_card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(single_card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(single_card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(single_card, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_add_flag(single_card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(single_card, card_key_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_t *metrics_col = lv_obj_create(single_card);
    lv_obj_set_style_bg_opa(metrics_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(metrics_col, 0, 0);
    lv_obj_set_style_pad_all(metrics_col, 0, 0);
    lv_obj_set_style_pad_hor(metrics_col, 5, 0);
    lv_obj_set_size(metrics_col, LV_PCT(70), 40);
    lv_obj_clear_flag(metrics_col, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_layout(metrics_col, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(metrics_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(metrics_col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(metrics_col, 0, 0); 

    hourly_time_lbl = lv_label_create(metrics_col);
    lv_label_set_recolor(hourly_time_lbl, true);
    lv_obj_set_style_text_font(hourly_time_lbl, &lv_font_montserrat_10, 0);

    hourly_temp_lbl = lv_label_create(metrics_col);
    lv_label_set_recolor(hourly_temp_lbl, true);
    lv_obj_set_style_text_font(hourly_temp_lbl, &lv_font_montserrat_10, 0);

    hourly_hum_lbl = lv_label_create(metrics_col);
    lv_label_set_recolor(hourly_hum_lbl, true);
    lv_obj_set_style_text_font(hourly_hum_lbl, &lv_font_montserrat_10, 0);

    hourly_wind_lbl = lv_label_create(metrics_col);
    lv_label_set_recolor(hourly_wind_lbl, true);
    lv_obj_set_style_text_font(hourly_wind_lbl, &lv_font_montserrat_10, 0);

    hourly_icon_img = lv_img_create(single_card);
    lv_obj_set_size(hourly_icon_img, LV_PCT(30), LV_PCT(100));
    lv_img_set_zoom(hourly_icon_img, ZOOM_SCALE_HOURLY_ICON);
    lv_obj_align(hourly_icon_img, LV_ALIGN_RIGHT_MID, 0, 0);

    make_obj_navigable(bottom_panel_container);
    make_obj_navigable(single_card);
    
    update_card_data(HEX_COLOR_TEXT_SECONDARY, HEX_COLOR_TEXT_PRIMARY); 
}

static void delete_app_ui() {
    current_temp_lbl = NULL;
    humidity_lbl = NULL;
    precip_lbl = NULL;
    wind_lbl = NULL;
    main_icon = NULL;
    
    date_lbl = NULL;
    hourly_time_lbl = NULL;
    hourly_temp_lbl = NULL;
    hourly_hum_lbl = NULL;
    hourly_wind_lbl = NULL;
    hourly_icon_img = NULL;
    
    single_card = NULL;
    bottom_panel_container = NULL;
    date_container = NULL;
    loading_spinner = NULL; 
}

static void receive_event(const app_update_t* update) {
    switch(update->req) {
        case DATED_WEATHER_REQUEST :
            ESP_LOGI(TAG, "Received fresh 24-hour block via BLE channel.");
            memcpy(hourly_forecast, update->data, sizeof(hourly_weather_t)*TOTAL_HOURS);
            date_active_held = false;
            refresh_weather_ui_data();
            break;
        default:
            ESP_LOGW(TAG, "Unhandled weather app update opcode: %d", update->req);
            break;
    }
}

static application_t weather_app = {
    .name = "Weather",
    .ico = &cloudy,
    .draw_app = draw_app_ui,
    .close_app = delete_app_ui,
    .handle_event = receive_event 
};

static void trigger_dated_weather_request(const date_time_t *target_date) {
    if (!target_date) return;

    reset_weather_data();

    if (bottom_panel_container && single_card && !loading_spinner) {
        lv_obj_add_flag(single_card, LV_OBJ_FLAG_HIDDEN);
        
        loading_spinner = lv_spinner_create(bottom_panel_container);
        lv_obj_set_size(loading_spinner, SIZE_LOADING_SPINNER_DIMENSION, SIZE_LOADING_SPINNER_DIMENSION);
        lv_obj_align(loading_spinner, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_arc_color(loading_spinner, COLOR_THEME_TEXT_SECONDARY, LV_PART_INDICATOR);
    }

    ESP_LOGI(TAG, "Batch action committed. Sending BLE request for: %02d/%02d/%04d", 
            target_date->day, target_date->month, target_date->year);

    request_ble_resource(DATED_WEATHER_REQUEST, (void*)target_date, get_system_app_id(&weather_app));
}

application_t* get_weather_app(void) {
    return &weather_app;
}
