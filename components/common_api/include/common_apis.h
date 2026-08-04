#ifndef COMMON_APIS_H
#define COMMON_APIS_H

#include <common_types.h>
#include <app_types.h>

void get_date_time(date_time_t*);
void get_weather_day(hourly_weather_t*);

bool request_ble_resource(const application_t* app, app_ble_req_t, void* data);
bool set_ui_inactivity_timeout(const application_t* req_app, uint32_t timeout);
bool set_system_watchface(const application_t* req_app,int index);

int get_system_brightness(void);
bool set_system_brightness(const application_t* req_app, int percent);
size_t get_system_watchface_names(const char** names);
const char* get_system_selected_watchface_name(void);
uint32_t get_ui_inactivity_timeout(void);

#endif /* COMMON_APIS_H */
