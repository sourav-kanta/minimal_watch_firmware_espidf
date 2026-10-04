#include <weather_app.h>
#include <weather_utils.h>

#include <lvgl.h>
#include <ui_theme.h>
#include <ui_utils.h>
#include <common_types.h>
#include <common_apis.h>
#include <app_types.h>
#include <global_locks.h>
#include <stdio.h>
#include <esp_log.h>
#include <string.h>

#include "assets/cloudy.h"
#include "assets/sunny.h"

#define TOP_CARD_SIZE_PX            100
#define BOTTOM_CARD_SIZE_PX         60
#define DATE_CARD_SIZE_PX           20
#define ZOOM_SCALE_MAIN_ICON        180
#define ZOOM_SCALE_HR_ICON          100
#define LOADING_SPINNER_SIZE_PX     30
#define TOTAL_HOURS 24

static const char* TAG = "Weather app";

typedef struct {
    hourly_weather_t weather_data[TOTAL_HOURS];
    date_time_t active_date_time;
    int date_offset;
    int hr_offset;
} weather_app_data_ctx_t;

typedef struct {
    weather_app_data_ctx_t data;
    lv_grad_dsc_t bg_grad;
    lv_obj_t* base_obj;
    lv_obj_t* curr_temp_lbl;
    lv_obj_t* curr_humidity_lbl;
    lv_obj_t* curr_wind_lbl;
    lv_obj_t* curr_precipitation_lbl;
    lv_obj_t* curr_state_icon;
    lv_obj_t* hourly_lbl;
    lv_obj_t* hourly_temp_lbl;
    lv_obj_t* hourly_humidity_lbl;
    lv_obj_t* hourly_wind_lbl;
    lv_obj_t* hourly_state_icon;
    lv_obj_t* date_lbl;
    lv_obj_t* bottom_card;
    lv_obj_t* spinner;
} weather_app_ctx_t;

static weather_app_ctx_t* app_ctx = NULL;
static application_t weather_app;

static void trigger_dated_weather_request(const date_time_t *target_date) {
    if (!target_date) return;
    request_ble_resource(&weather_app, DATED_WEATHER_REQUEST, (void*)target_date);
}

static void update_top_card(const hourly_weather_t* curr_weather) {
    assert(curr_weather);
    if(app_ctx != NULL) {
        WITH_UI_LOCK() {
            if(app_ctx->curr_temp_lbl)
                lv_label_set_text_fmt(app_ctx->curr_temp_lbl, "%d\u00B0C", curr_weather->temperature/10);
            if(app_ctx->curr_humidity_lbl)
                lv_label_set_text_fmt(app_ctx->curr_humidity_lbl, "H: %u%%", curr_weather->humidity);
            if(app_ctx->curr_precipitation_lbl)
                lv_label_set_text_fmt(app_ctx->curr_precipitation_lbl, "P: %u%%", curr_weather->precip_prob);
            if(app_ctx->curr_wind_lbl)
                lv_label_set_text_fmt(app_ctx->curr_wind_lbl, "W: %ukm/hr", curr_weather->wind_speed);
            if(app_ctx->curr_state_icon)
                lv_img_set_src(app_ctx->curr_state_icon, get_icon_from_code(curr_weather->weather_code));
        }
    }
}

static void update_bottom_card(const hourly_weather_t* hourly_weather) {
    assert(hourly_weather);
    if(app_ctx != NULL) {
        WITH_UI_LOCK() {
            if(app_ctx->hourly_temp_lbl)
                lv_label_set_text_fmt(app_ctx->hourly_temp_lbl, "%d\u00B0C", hourly_weather->temperature/10);
            if(app_ctx->hourly_humidity_lbl)
                lv_label_set_text_fmt(app_ctx->hourly_humidity_lbl, "H: %u%%", hourly_weather->humidity);
            if(app_ctx->hourly_wind_lbl)
                lv_label_set_text_fmt(app_ctx->hourly_wind_lbl, "W: %ukm/hr", hourly_weather->wind_speed);
            if(app_ctx->hourly_state_icon)
                lv_img_set_src(app_ctx->hourly_state_icon, get_icon_from_code(hourly_weather->weather_code));
        }
    }
}

static void update_button_labels(void) {
    if(app_ctx != NULL) {
        char fmt_hr_str[6];
        get_date_time_hr_string_fmt(app_ctx->data.active_date_time.hr, app_ctx->data.hr_offset, fmt_hr_str);
        char fmt_date_str[7];
        get_date_time_date_string_fmt(&app_ctx->data.active_date_time, app_ctx->data.date_offset, fmt_date_str);
        WITH_UI_LOCK() {
            if(app_ctx->hourly_lbl)
                lv_label_set_text(app_ctx->hourly_lbl, fmt_hr_str);
            if(app_ctx->date_lbl)
                lv_label_set_text(app_ctx->date_lbl, fmt_date_str);
        }
    }
}

static void refresh_bottom_ui(void) {
    if(app_ctx != NULL) {
        uint8_t hr_idx = calculate_hr_with_offset(&app_ctx->data.active_date_time, app_ctx->data.hr_offset);
        update_bottom_card(&app_ctx->data.weather_data[hr_idx]);
        update_button_labels();
    }
}

static void refresh_all_ui(void) {
    if(app_ctx != NULL) {
        update_top_card(&app_ctx->data.weather_data[app_ctx->data.active_date_time.hr]);
        refresh_bottom_ui();
    }
}

static void initialize_weather_ui(void) {
    if(app_ctx) {
        if(!validate_date_time(&app_ctx->data.active_date_time)) {
            memset(&app_ctx->data.active_date_time, 0, sizeof(date_time_t));
            return;
        }

        if(validate_hr_offset_increment(&app_ctx->data.active_date_time, app_ctx->data.hr_offset)) {
            app_ctx->data.hr_offset++;
        }
        refresh_all_ui();
    }
}

static void enter_loading_state(void) {
    if(app_ctx == NULL || app_ctx->bottom_card == NULL ||
       app_ctx->base_obj == NULL || app_ctx->spinner != NULL) {
        ESP_LOGE(TAG, "Errorneous state, skipping loading screen");
        return;
    }

    refresh_all_ui();
    WITH_UI_LOCK() {
        lv_obj_add_flag(app_ctx->bottom_card, LV_OBJ_FLAG_HIDDEN);
        app_ctx->spinner = lv_spinner_create(app_ctx->base_obj);
        lv_obj_set_size(app_ctx->spinner, LOADING_SPINNER_SIZE_PX, LOADING_SPINNER_SIZE_PX);
        lv_obj_add_flag(app_ctx->spinner, LV_OBJ_FLAG_FLOATING);
        lv_obj_align(app_ctx->spinner, LV_ALIGN_BOTTOM_MID, 0, -10);
        lv_obj_set_style_arc_color(app_ctx->spinner, COLOR_THEME_TEXT_HIGLIGHT, LV_STATE_DEFAULT);
    }
}

static void exit_loading_state(void) {
    if(app_ctx == NULL || app_ctx->bottom_card == NULL ||
       app_ctx->base_obj == NULL || app_ctx->spinner == NULL) {
        ESP_LOGE(TAG, "Errorneous state, skipping loading screen");
        return;
    }
    WITH_UI_LOCK() {
        lv_obj_delete(app_ctx->spinner);
        app_ctx->spinner = NULL;
        lv_obj_remove_flag(app_ctx->bottom_card, LV_OBJ_FLAG_HIDDEN);
    }
    initialize_weather_ui();
}

static void initialize_weather_app(void) {
    if(app_ctx) {
        get_date_time(&app_ctx->data.active_date_time);
        get_weather_day(app_ctx->data.weather_data);
        initialize_weather_ui();
    }
}

static void handle_label_key_event(lv_event_t* event) {
    if(app_ctx == NULL || app_ctx->hourly_lbl == NULL || app_ctx->date_lbl == NULL) return;
    if(lv_event_get_key(event) == LV_KEY_LEFT) {
        if(lv_event_get_target(event) == app_ctx->date_lbl) {
            if(validate_date_offset_decrement(&app_ctx->data.active_date_time,
                                              app_ctx->data.date_offset)) {
                app_ctx->data.date_offset--;
                update_button_labels();
            }
            lv_event_stop_bubbling(event);
        }
        else if(lv_event_get_target(event) == app_ctx->hourly_lbl) {
            if(validate_hr_offset_decrement(&app_ctx->data.active_date_time,
                                            app_ctx->data.hr_offset)) {
                app_ctx->data.hr_offset--;
                refresh_bottom_ui();
            }
            lv_event_stop_bubbling(event);
        }
    }
    else if(lv_event_get_key(event) == LV_KEY_RIGHT) {
        if(lv_event_get_target(event) == app_ctx->date_lbl) {
            if(validate_date_offset_increment(&app_ctx->data.active_date_time,
                                              app_ctx->data.date_offset)) {
                app_ctx->data.date_offset++;
                update_button_labels();
            }
            lv_event_stop_bubbling(event);
        }
        else if(lv_event_get_target(event) == app_ctx->hourly_lbl) {
            if(validate_hr_offset_increment(&app_ctx->data.active_date_time,
                                            app_ctx->data.hr_offset)) {
                app_ctx->data.hr_offset++;
                refresh_bottom_ui();
            }
            lv_event_stop_bubbling(event);
        }
    }
    else if(lv_event_get_key(event) == LV_KEY_ENTER) {
        lv_event_stop_bubbling(event);
        uint32_t key = LV_KEY_ESC;
        lv_obj_send_event(lv_event_get_target(event), LV_EVENT_KEY, &key);
    }
}

static void handle_date_unfocused_event(lv_event_t *event) {
    if(app_ctx == NULL || app_ctx->date_lbl == NULL) return;
    if(lv_event_get_target(event) == app_ctx->date_lbl) {
        if(app_ctx->data.date_offset != 0) {
            date_time_t req_date;
            calculate_date_with_offset(&app_ctx->data.active_date_time, app_ctx->data.date_offset, &req_date);
            if(validate_date_time(&req_date)) {
                app_ctx->data.active_date_time = req_date;
                app_ctx->data.hr_offset = 0;
                app_ctx->data.date_offset = 0;
                memset(app_ctx->data.weather_data, 0, sizeof(hourly_weather_t) * TOTAL_HOURS);
                enter_loading_state();
                trigger_dated_weather_request(&app_ctx->data.active_date_time);
            }
        }
    }
}

static void draw_app_ui(lv_obj_t* parent) {
    app_ctx = (weather_app_ctx_t*) calloc(1, sizeof(weather_app_ctx_t));
    if(!app_ctx) {
        ESP_LOGE(TAG, "Error allocating memory for weather app");
        return;
    }
    WITH_UI_LOCK() {
        app_ctx->bg_grad.dir = LV_GRAD_DIR_VER;
        app_ctx->bg_grad.stops_count = 2;
        app_ctx->bg_grad.stops[0].color = COLOR_THEME_ACCENT;
        app_ctx->bg_grad.stops[0].opa = LV_OPA_COVER;
        app_ctx->bg_grad.stops[1].color = COLOR_THEME_TEXT_PRIMARY;
        app_ctx->bg_grad.stops[1].opa = LV_OPA_COVER;
        app_ctx->bg_grad.stops[0].frac = 120;
        app_ctx->bg_grad.stops[1].frac = 255;

        app_ctx->base_obj = lv_obj_create(parent);
        lv_obj_set_size(app_ctx->base_obj, lv_pct(100), lv_pct(100));
        remove_shadow_and_outline(app_ctx->base_obj);
        lv_obj_set_layout(app_ctx->base_obj, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(app_ctx->base_obj, LV_FLEX_FLOW_COLUMN);
        lv_obj_add_flag(app_ctx->base_obj, LV_OBJ_FLAG_EVENT_BUBBLE);

        lv_obj_t* top_card = lv_obj_create(app_ctx->base_obj);
        lv_obj_set_size(top_card, lv_pct(100), TOP_CARD_SIZE_PX);
        lv_obj_set_style_bg_grad(top_card, &app_ctx->bg_grad, LV_STATE_DEFAULT);
        lv_obj_set_layout(top_card, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(top_card, LV_FLEX_FLOW_ROW);

        lv_obj_t* text_container = lv_obj_create(top_card);
        lv_obj_set_size(text_container, lv_pct(50), lv_pct(100));
        lv_obj_set_style_bg_opa(text_container, LV_OPA_TRANSP, LV_STATE_DEFAULT);
        lv_obj_set_layout(text_container, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(text_container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_hor(text_container, 10, LV_STATE_DEFAULT);
        lv_obj_set_flex_align(text_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_scrollbar_mode(text_container, LV_SCROLLBAR_MODE_OFF);

        app_ctx->curr_temp_lbl = lv_label_create(text_container);
        lv_label_set_text(app_ctx->curr_temp_lbl, "0\u00B0C");
        lv_obj_set_style_text_font(app_ctx->curr_temp_lbl, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(app_ctx->curr_temp_lbl, COLOR_THEME_PRIMARY, 0);

        app_ctx->curr_humidity_lbl = lv_label_create(text_container);
        lv_label_set_text_fmt(app_ctx->curr_humidity_lbl, "H: 0%%");
        lv_obj_set_style_text_font(app_ctx->curr_humidity_lbl, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(app_ctx->curr_humidity_lbl, COLOR_THEME_PRIMARY, 0);

        app_ctx->curr_precipitation_lbl = lv_label_create(text_container);
        lv_label_set_text(app_ctx->curr_precipitation_lbl, "P: 0%");
        lv_obj_set_style_text_font(app_ctx->curr_precipitation_lbl, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(app_ctx->curr_precipitation_lbl, COLOR_THEME_PRIMARY, 0);

        app_ctx->curr_wind_lbl = lv_label_create(text_container);
        lv_label_set_text(app_ctx->curr_wind_lbl, "W: 0km/h");
        lv_obj_set_style_text_font(app_ctx->curr_wind_lbl, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(app_ctx->curr_wind_lbl, COLOR_THEME_PRIMARY, 0);

        lv_obj_t* icon_container = lv_obj_create(top_card);
        lv_obj_set_size(icon_container, lv_pct(50), lv_pct(100));
        lv_obj_set_style_bg_opa(icon_container, LV_OPA_TRANSP, LV_STATE_DEFAULT);
        lv_obj_set_scrollbar_mode(icon_container, LV_SCROLLBAR_MODE_OFF);

        app_ctx->curr_state_icon = lv_img_create(icon_container);
        lv_img_set_src(app_ctx->curr_state_icon, &sunny);
        lv_img_set_zoom(app_ctx->curr_state_icon, ZOOM_SCALE_MAIN_ICON);
        lv_obj_align(app_ctx->curr_state_icon, LV_ALIGN_CENTER, 0, 0);

        app_ctx->bottom_card = lv_obj_create(app_ctx->base_obj);
        lv_obj_set_size(app_ctx->bottom_card, lv_pct(100), BOTTOM_CARD_SIZE_PX);
        lv_obj_set_layout(app_ctx->bottom_card, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(app_ctx->bottom_card, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_hor(app_ctx->bottom_card, 10, LV_STATE_DEFAULT);
        lv_obj_set_style_pad_top(app_ctx->bottom_card, 5, LV_STATE_DEFAULT);

        lv_obj_t* bottom_text_container = lv_obj_create(app_ctx->bottom_card);
        lv_obj_set_size(bottom_text_container, lv_pct(60), lv_pct(100));
        lv_obj_set_style_bg_opa(bottom_text_container, LV_OPA_TRANSP, LV_STATE_DEFAULT);
        lv_obj_set_layout(bottom_text_container, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(bottom_text_container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(bottom_text_container, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_scrollbar_mode(bottom_text_container, LV_SCROLLBAR_MODE_OFF);

        app_ctx->hourly_temp_lbl = lv_label_create(bottom_text_container);
        lv_label_set_text(app_ctx->hourly_temp_lbl, "0\u00B0C");
        lv_obj_set_style_text_font(app_ctx->hourly_temp_lbl, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(app_ctx->hourly_temp_lbl, COLOR_THEME_PRIMARY, 0);

        app_ctx->hourly_humidity_lbl = lv_label_create(bottom_text_container);
        lv_label_set_text_fmt(app_ctx->hourly_humidity_lbl, "H: 0%%");
        lv_obj_set_style_text_font(app_ctx->hourly_humidity_lbl, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(app_ctx->hourly_humidity_lbl, COLOR_THEME_PRIMARY, 0);

        app_ctx->hourly_wind_lbl = lv_label_create(bottom_text_container);
        lv_label_set_text(app_ctx->hourly_wind_lbl, "W: 0km/h");
        lv_obj_set_style_text_font(app_ctx->hourly_wind_lbl, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(app_ctx->hourly_wind_lbl, COLOR_THEME_PRIMARY, 0);

        lv_obj_t* bottom_icon_container = lv_obj_create(app_ctx->bottom_card);
        lv_obj_set_size(bottom_icon_container, lv_pct(40), lv_pct(100));
        lv_obj_set_style_bg_opa(bottom_icon_container, LV_OPA_TRANSP, LV_STATE_DEFAULT);
        lv_obj_set_scrollbar_mode(bottom_icon_container, LV_SCROLLBAR_MODE_OFF);

        app_ctx->hourly_state_icon = lv_img_create(bottom_icon_container);
        lv_img_set_src(app_ctx->hourly_state_icon, &sunny);
        lv_obj_align(app_ctx->hourly_state_icon, LV_ALIGN_CENTER, 0, 0);
        lv_img_set_zoom(app_ctx->hourly_state_icon, ZOOM_SCALE_HR_ICON);

        lv_obj_t* hour_container = lv_obj_create(app_ctx->base_obj);
        lv_obj_add_flag(hour_container, LV_OBJ_FLAG_FLOATING);
        lv_obj_set_size(hour_container, LV_SIZE_CONTENT, DATE_CARD_SIZE_PX);
        lv_obj_set_style_pad_hor(hour_container, 8, LV_STATE_DEFAULT);
        lv_obj_set_style_radius(hour_container, 10, LV_STATE_DEFAULT);
        lv_obj_set_size(hour_container, LV_SIZE_CONTENT, DATE_CARD_SIZE_PX);
        lv_obj_align(hour_container, LV_ALIGN_TOP_LEFT, 10, TOP_CARD_SIZE_PX - (1*DATE_CARD_SIZE_PX)/2);
        lv_obj_set_style_bg_color(hour_container, COLOR_THEME_PRIMARY, LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(hour_container, COLOR_THEME_TEXT_HIGLIGHT, LV_STATE_FOCUSED);

        app_ctx->hourly_lbl = lv_label_create(hour_container);
        lv_label_set_text(app_ctx->hourly_lbl, "1 AM");
        lv_obj_align(app_ctx->hourly_lbl, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_text_font(app_ctx->hourly_lbl, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(app_ctx->hourly_lbl, COLOR_THEME_TEXT_PRIMARY, 0);
        lv_obj_set_style_text_color(app_ctx->hourly_lbl, COLOR_THEME_TEXT_ERROR, LV_STATE_FOCUSED);

        lv_obj_t* date_container = lv_obj_create(app_ctx->base_obj);
        lv_obj_add_flag(date_container, LV_OBJ_FLAG_FLOATING);
        lv_obj_set_style_pad_hor(date_container, 10, LV_STATE_DEFAULT);
        lv_obj_set_style_radius(date_container, 10, LV_STATE_DEFAULT);
        lv_obj_set_size(date_container, LV_SIZE_CONTENT, DATE_CARD_SIZE_PX);
        lv_obj_align(date_container, LV_ALIGN_TOP_RIGHT, -10, TOP_CARD_SIZE_PX - (1*DATE_CARD_SIZE_PX)/2);
        lv_obj_set_style_bg_color(date_container, COLOR_THEME_PRIMARY, LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(date_container, COLOR_THEME_TEXT_HIGLIGHT, LV_STATE_FOCUSED);

        app_ctx->date_lbl = lv_label_create(date_container);
        lv_obj_align(app_ctx->date_lbl, LV_ALIGN_CENTER, 0, 0);
        lv_label_set_text(app_ctx->date_lbl, "26 Aug");
        lv_obj_set_style_text_font(app_ctx->date_lbl, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(app_ctx->date_lbl, COLOR_THEME_TEXT_PRIMARY, LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(app_ctx->date_lbl, COLOR_THEME_TEXT_ERROR, LV_STATE_FOCUSED);

        make_obj_navigable(date_container);
        make_obj_navigable(hour_container);
        make_obj_navigable(app_ctx->hourly_lbl);
        make_obj_navigable(app_ctx->date_lbl);

        lv_obj_add_event_cb(app_ctx->hourly_lbl, handle_label_key_event, LV_EVENT_KEY, NULL);
        lv_obj_add_event_cb(app_ctx->date_lbl, handle_label_key_event, LV_EVENT_KEY, NULL);
        lv_obj_add_event_cb(app_ctx->date_lbl, handle_date_unfocused_event, LV_EVENT_DEFOCUSED, NULL);
    }
    initialize_weather_app();
}

static void receive_event(const app_update_t* update) {
    if(update==NULL || app_ctx == NULL) return;
    switch(update->req) {
        case DATED_WEATHER_REQUEST :
            memcpy(app_ctx->data.weather_data, update->data, sizeof(hourly_weather_t) * TOTAL_HOURS);
            exit_loading_state();
            break;
        default:
            ESP_LOGW(TAG, "Unhandled weather app update opcode: %d", update->req);
            break;
    }
}

static void delete_app_ui() {
    WITH_UI_LOCK() {
        if(app_ctx != NULL && app_ctx->base_obj != NULL) {
            lv_obj_delete(app_ctx->base_obj);
        }
        if(app_ctx) {
            free(app_ctx);
            app_ctx = NULL;
        }
    }
}

static application_t weather_app = {
    .name = "Weather",
    .app_perms = APP_PERM_BLE,
    .ico = &cloudy,
    .draw_app = draw_app_ui,
    .close_app = delete_app_ui,
    .handle_event = receive_event
};

application_t* get_weather_app(void) {
    return &weather_app;
}
