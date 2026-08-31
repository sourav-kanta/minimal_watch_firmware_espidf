#ifndef ALARM_APP_H
#define ALARM_APP_H

#include <app_types.h>

application_t* get_alarm_app(void);
void invalidate_and_repopulate_alarm_list(void);

#endif /* ALARM_APP_H */
