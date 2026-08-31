#ifndef COMMON_APIS_H
#define COMMON_APIS_H

#include <common_types.h>
#include <app_types.h>

uint32_t get_epoch_time_now(void);
void get_date_time(date_time_t*);
void get_date_time_from_epoch(uint32_t epoch, date_time_t* out_dt);
uint32_t get_epoch_from_date_time(const date_time_t* dt);
bool validate_date_time(const date_time_t* dt);
void get_weather_day(hourly_weather_t*);

uint8_t get_all_alarms(alarm_t* out_alarms);
bool create_new_alarm(const application_t* req_app, alarm_t* alarm);
bool edit_alarm_by_index(const application_t* req_app, int idx, alarm_t* new_alarm);
bool delete_alarm_by_index(const application_t* req_app, int idx);

bool request_ble_resource(const application_t* app, app_ble_req_t, void* data);
bool set_ui_inactivity_timeout(const application_t* req_app, uint32_t timeout);
bool set_system_watchface(const application_t* req_app,int index);

int get_system_brightness(void);
bool set_system_brightness(const application_t* req_app, int percent);
size_t get_system_watchface_names(const char** names);
const char* get_system_selected_watchface_name(void);
uint32_t get_ui_inactivity_timeout(void);

#endif /* COMMON_APIS_H */
