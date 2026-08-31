#ifndef ALARM_MANAGER_H
#define ALARM_MANAGER_H

#include <common_types.h>

uint8_t alarm_manager_get_all_alarms(alarm_t *all_alarms);
bool alarm_manager_create_alarm(alarm_t* alarm);
bool alarm_manager_edit_alarm(int index, alarm_t* alarm);
bool alarm_manager_delete_alarm(int index);

#endif /* ALARM_MANAGER_H */
