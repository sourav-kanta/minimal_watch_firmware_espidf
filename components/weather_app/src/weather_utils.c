#include <weather_utils.h>

#include <lvgl.h>
#include <stdio.h>
#include <assert.h>
#include <common_apis.h>

#include "assets/cloudy.h"
#include "assets/rainy.h"
#include "assets/sunny.h"
#include "assets/cloud.h"

static const char months_str[][4] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

const lv_img_dsc_t* get_icon_from_code(uint8_t code) {
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

void get_date_time_hr_string_fmt(uint8_t hour, int hr_offset, char* out) {
    assert(out);
    int sanitized_hr = (int)hour + hr_offset;
    assert(validate_hour(sanitized_hr));
    const char* post_str = sanitized_hr >= 12 ? "PM" : "AM";
    sanitized_hr = sanitized_hr > 12 ? sanitized_hr % 12 : sanitized_hr;
    sanitized_hr = sanitized_hr == 0 ? 12 : sanitized_hr;
    snprintf(out, 6, "%u %s", sanitized_hr, post_str);
}

void get_date_time_date_string_fmt(const date_time_t* date, int day_offset, char* out) {
    assert(out);
    date_time_t offseted_date;
    uint32_t epoch = get_epoch_from_date_time(date);
    int64_t proposed_epoch = (int64_t)epoch + day_offset*SECONDS_PER_DAY;
    assert(validate_epoch(proposed_epoch));
    get_date_time_from_epoch((uint32_t) proposed_epoch, &offseted_date);
    uint8_t sanitized_month = offseted_date.month == 0 || offseted_date.month > 12 ? 0 :
                              offseted_date.month - 1;
    snprintf(out, 7, "%u %s", offseted_date.day, months_str[sanitized_month]);
}
