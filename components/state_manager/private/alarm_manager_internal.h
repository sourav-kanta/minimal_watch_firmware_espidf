#ifndef ALARM_MANAGER_INTERNAL_H
#define ALARM_MANAGER_INTERNAL_H

#include <common_types.h>

void alarm_manager_init(alarm_sync_t* alarms);
void alarm_manager_deinit(void);

bool alarm_manager_revalidate(void);

#endif /* ALARM_MANAGER_INTERNAL_H */
