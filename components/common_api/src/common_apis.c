#include <common_apis.h>
#include <state_manager.h>
#include <alarm_manager.h>
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
#include <wakelock_manager.h>

static const char* TAG = "Common API";

uint32_t get_epoch_time_now(void) {
    return get_epoch_time();
}

void get_date_time_from_epoch(uint32_t epoch, date_time_t* out_dt) {
    assert(out_dt);
    time_t time_val = (time_t) epoch;
    struct tm curr_time;
    if (gmtime_r(&time_val, &curr_time) != NULL) {
        out_dt->day = curr_time.tm_mday;
        out_dt->month = curr_time.tm_mon + 1;
        out_dt->year = curr_time.tm_year + 1900;
        out_dt->hr = curr_time.tm_hour;
        out_dt->min = curr_time.tm_min;
        out_dt->sec = curr_time.tm_sec;
        out_dt->d_week = curr_time.tm_wday;
    }
}

uint32_t get_epoch_from_date_time(const date_time_t* dt) {
    assert(dt);
    struct tm temp_tm = {
        .tm_sec   = dt->sec,
        .tm_min   = dt->min,
        .tm_hour  = dt->hr,
        .tm_mday  = dt->day,
        .tm_mon   = dt->month - 1,
        .tm_year  = dt->year - 1900,
        .tm_isdst = 0                  
    };
    time_t t = timegm(&temp_tm);
    return (uint32_t)t;
}

void get_date_time(date_time_t* time) {
    assert(time);
    get_date_time_from_epoch(get_epoch_time(), time);
}

void get_weather_day(hourly_weather_t* day_weather) {
    assert(day_weather);
    const hourly_weather_t *weather = get_weather_today();
    assert(weather);
    memcpy(day_weather, weather, sizeof(hourly_weather_t)*24);
}

bool validate_date_time(const date_time_t* dt) {
    assert(dt);
    struct tm temp_tm = {
        .tm_sec   = dt->sec,
        .tm_min   = dt->min,
        .tm_hour  = dt->hr,
        .tm_mday  = dt->day,
        .tm_mon   = dt->month - 1,
        .tm_year  = dt->year - 1900,
        .tm_isdst = 0                  
    };
    struct tm check_tm = temp_tm;
    time_t t = timegm(&check_tm);
    if (t == (time_t)(-1)) {
        return false; 
    }
    return (temp_tm.tm_sec  == check_tm.tm_sec  &&
            temp_tm.tm_min  == check_tm.tm_min  &&
            temp_tm.tm_hour == check_tm.tm_hour &&
            temp_tm.tm_mday == check_tm.tm_mday &&
            temp_tm.tm_mon  == check_tm.tm_mon  &&
            temp_tm.tm_year == check_tm.tm_year);
}

uint8_t get_all_alarms(alarm_t* out_alarms) {
    return alarm_manager_get_all_alarms(out_alarms);
}

bool create_new_alarm(const application_t* req_app, alarm_t* alarm) {
    if(!req_app) return false;
    if(!check_app_permission(req_app, APP_PERM_SYSTEM)) {
        ESP_LOGE(TAG, "System app reserved API. Failed!");
        return false;
    }

    return alarm_manager_create_alarm(alarm);
}

bool edit_alarm_by_index(const application_t* req_app, int idx, alarm_t* new_alarm) {
    if(!req_app) return false;
    if(!check_app_permission(req_app, APP_PERM_SYSTEM)) {
        ESP_LOGE(TAG, "System app reserved API. Failed!");
        return false;
    }

    return alarm_manager_edit_alarm(idx, new_alarm);
}

bool delete_alarm_by_index(const application_t* req_app, int idx) {
    if(!req_app) return false;
    if(!check_app_permission(req_app, APP_PERM_SYSTEM)) {
        ESP_LOGE(TAG, "System app reserved API. Failed!");
        return false;
    }

    return alarm_manager_delete_alarm(idx);
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
    if(!req_app) return false;
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
    if(!req_app) return false;
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
    if(!req_app) return false;
    // Check system app permission
    if(!check_app_permission(req_app, APP_PERM_SYSTEM)) {
        ESP_LOGE(TAG, "System app reserved API. Failed!");
        return false;
    }
    return lvgl_bridge_update_inactivity_timeout(timeout);
}

bool acquire_wakelock(const application_t* req_app) {
    if(!req_app) return false;
    if(!check_app_permission(req_app, APP_PERM_WAKELOCK)) {
        ESP_LOGE(TAG, "Wakelock permission missing in app. Failed!");
        return false;
    }
    wakelock_manager_acquire_wakelock();
    return true;
}

bool release_wakelock(const application_t* req_app) {
    if(!req_app) return false;
    if(!check_app_permission(req_app, APP_PERM_WAKELOCK)) {
        ESP_LOGE(TAG, "Wakelock permission missing in app. Failed!");
        return false;
    }
    wakelock_manager_release_wakelock();
    return true;
}
