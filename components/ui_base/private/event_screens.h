#ifndef EVENT_SCREENS_H
#define EVENT_SCREENS_H

#include <lvgl.h>
#include <common_types.h>
#include <ui_base_types.h>

void show_incoming_call_page(lv_obj_t* parent, const char* number, const char* name, on_finish_event_callback_t cb);
void show_incoming_notification_toast(lv_obj_t* parent, const char* title, on_finish_event_callback_t cb);
void show_alarm_page(lv_obj_t* parent, date_time_t *alarm_date, on_finish_event_callback_t cb);

#endif /* EVENT_SCREENS_H */
