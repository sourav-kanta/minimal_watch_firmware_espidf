#include <common_apis.h>
#include <state_manager.h>
#include <event_manager.h>
#include <time.h>
#include <string.h>
#include <common_types.h>

void get_date_time(date_time_t* time) {
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
    const hourly_weather_t *weather = get_weather_today();
    memcpy(day_weather, weather, sizeof(hourly_weather_t)*24);
}

void request_ble_resource(app_ble_req_t req, void* data, uint8_t app_id) {
    if(req == DATED_WEATHER_REQUEST) {
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
        event_publish(&ev);      
    }
}
