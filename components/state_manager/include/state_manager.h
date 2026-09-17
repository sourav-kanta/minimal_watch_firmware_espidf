#ifndef STATE_MANAGER_H
#define STATE_MANAGER_H

#include <stdint.h>
#include <common_types.h>

void state_manager_init(void);
void state_manager_deinit(void);
void state_manager_check_validity(void);
uint32_t state_manager_get_epoch_time(void);
const hourly_weather_t* state_manager_get_weather_today(void);
uint32_t state_manager_get_step_count(void);

#endif /* STATE_MANAGER_H */
