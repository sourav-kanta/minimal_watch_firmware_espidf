#ifndef COMMON_APIS_H
#define COMMON_APIS_H

#include <common_types.h>

void get_date_time(date_time_t*);
void get_weather_day(hourly_weather_t*);
void request_ble_resource(app_ble_req_t, void* data, uint8_t app_id);

#endif /* COMMON_APIS_H */
