#ifndef STATE_MANAGER_H
#define STATE_MANAGER_H

#include <stdint.h>
#include <common_types.h>

void state_manager_init(void);
void state_manager_deinit(void);
void state_manager_check_validity(void);
uint32_t get_epoch_time(void);
const hourly_weather_t* get_weather_today(void);

#endif /* STATE_MANAGER_H */
