#ifndef WEATHER_UTILS_H
#define WEATHER_UTILS_H

#include <stdint.h>
#include <lvgl.h>
#include <assert.h>
#include <common_types.h>
#include <common_apis.h>

#define SECONDS_PER_DAY             86400
#define MAX_DATE_OFFSET_DIFF        60

const lv_img_dsc_t* get_icon_from_code(uint8_t code);
void get_date_time_hr_string_fmt(uint8_t hour, int hr_offset, char* out);
void get_date_time_date_string_fmt(const date_time_t* date, int day_offset, char* out);

static inline bool validate_hour(int hr) {
    return hr >= 0 && hr < 24;
}

static inline bool validate_epoch(int64_t epoch) {
    return epoch >= 0 && epoch <= UINT32_MAX;
}

static inline uint8_t calculate_hr_with_offset(const date_time_t* time, int hr_offset) {
    assert(time);
    int proposed_hr = (int)time->hr + hr_offset;
    assert(validate_hour(proposed_hr));
    return (uint8_t) proposed_hr;
}

static inline bool validate_hr_offset_increment(const date_time_t* time, int hr_offset) {
    assert(time);
    int proposed_hr = (int)time->hr + hr_offset + 1;
    return validate_hour(proposed_hr);
}

static inline bool validate_hr_offset_decrement(const date_time_t* time, int hr_offset) {
    assert(time);
    int proposed_hr = (int)time->hr + hr_offset - 1;
    return validate_hour(proposed_hr);
}

static inline void calculate_date_with_offset(const date_time_t* time, int date_offset, date_time_t* out) {
    assert(time);
    assert(out);
    assert(date_offset >= (-1)*MAX_DATE_OFFSET_DIFF && date_offset <= MAX_DATE_OFFSET_DIFF);
    int64_t proposed_epoch = (int64_t)get_epoch_from_date_time(time) + (date_offset) * SECONDS_PER_DAY;
    assert(validate_epoch(proposed_epoch));
    get_date_time_from_epoch((uint32_t)proposed_epoch, out);
}

static inline bool validate_date_offset_increment(const date_time_t* time, int date_offset) {
    assert(time);
    if(date_offset >= MAX_DATE_OFFSET_DIFF) return false;
    int64_t proposed_epoch = (int64_t)get_epoch_from_date_time(time) + (date_offset + 1) * SECONDS_PER_DAY;
    return validate_epoch(proposed_epoch);
}

static inline bool validate_date_offset_decrement(const date_time_t* time, int date_offset) {
    assert(time);
    if(date_offset <= (-1) * MAX_DATE_OFFSET_DIFF) return false;
    int64_t proposed_epoch = (int64_t)get_epoch_from_date_time(time) + (date_offset - 1) * SECONDS_PER_DAY;
    return validate_epoch(proposed_epoch);
}

#endif /* WEATHER_UTILS_H */
