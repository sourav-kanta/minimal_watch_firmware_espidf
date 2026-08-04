#include <common_apis.h>
#include <state_manager.h>
#include <event_manager.h>
#include <time.h>
#include <string.h>
#include <common_types.h>
#include <app_utils.h>
#include <app_types.h>
#include <esp_log.h>
#include <gpio_manager.h>
#include <lvgl_bridge.h>
#include <wf_manager.h>

static const char* TAG = "Common API";

void get_date_time(date_time_t* time) {
    assert(time);
    time_t time_val = (time_t) get_epoch_time();
    struct tm curr_time;
    if (gmtime_r(&time_val, &curr_time) != NULL) {
        time->day = curr_time.tm_mday;
        time->month = curr_time.tm_mon + 1;
        time->year = curr_time.tm_year + 1900;
        time->hr = curr_time.tm_hour;
        time->min = curr_time.tm_min;
        time->sec = curr_time.tm_sec;
        time->d_week = curr_time.tm_wday;
    }
}

void get_weather_day(hourly_weather_t* day_weather) {
    assert(day_weather);
    const hourly_weather_t *weather = get_weather_today();
    assert(weather);
    memcpy(day_weather, weather, sizeof(hourly_weather_t)*24);
}

bool request_ble_resource(const application_t* req_app, app_ble_req_t req, void* data) {
    if(!req_app) return false;
    if(!check_app_permission(req_app, APP_PERM_BLE)) {
        ESP_LOGE(TAG, "BLE app permission not set. Failed!");
        return false;
    }
    uint8_t app_id = get_system_app_id(req_app);
    if(req == DATED_WEATHER_REQUEST) {
        assert(data);
        date_time_t* req_date = (date_time_t*) data;
        uint8_t payload[4] = { 0xFF & req_date->day,
                               0xFF & req_date->month,
                               0xFF & (req_date->year >> 8),
                               0XFF & (req_date->year) };
        ble_req_t ble_req = {
            .req_code = DATED_WEATHER_QUERY,
            .req_data_len = sizeof(payload),
            .req_data = payload,
            .app_id = app_id
        };
        event_t ev = {
            .ev = EVENT_BLE_REQUEST,
            .payload_len = sizeof(ble_req_t),
            .data = &ble_req
        };
        // Need to update BLE module to memcpy the payload
        return event_publish(&ev);      
    }
    return false;
}

int get_system_brightness(void) {
    return gpio_manager_backlight_get_brightness();    
}

bool set_system_brightness(const application_t* req_app, int percent) {
    // Check app permission of system app
    if(!check_app_permission(req_app, APP_PERM_SYSTEM)) {
        ESP_LOGE(TAG, "System app reserved API. Failed!");
        return false;
    }
    return gpio_manager_backlight_set_brightness(percent);
}

size_t get_system_watchface_names(const char** names) {
    return watchface_manager_get_all_wf_names(names);
}

bool set_system_watchface(const application_t* req_app,int index) {
    // Check app permission of system app
    if(!check_app_permission(req_app, APP_PERM_SYSTEM)) {
        ESP_LOGE(TAG, "System app reserved API. Failed!");
        return false;
    }
    return watchface_manager_select_wf(index);
}

const char* get_system_selected_watchface_name(void) {
    return watchface_manager_get_selected_wf_name();
}

uint32_t get_ui_inactivity_timeout(void) {
    return lvgl_bridge_get_inactivity_timeout();
}

bool set_ui_inactivity_timeout(const application_t* req_app, uint32_t timeout) {
    // Check system app permission
    if(!check_app_permission(req_app, APP_PERM_SYSTEM)) {
        ESP_LOGE(TAG, "System app reserved API. Failed!");
        return false;
    }
    return lvgl_bridge_update_inactivity_timeout(timeout);
}
