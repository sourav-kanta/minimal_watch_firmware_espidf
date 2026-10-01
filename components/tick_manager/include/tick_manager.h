#ifndef TICK_MANAGER_H
#define TICK_MANAGER_H

#include <tick_types.h>
#include <stdbool.h>

void tick_manager_init(void);
void tick_manager_deinit(void);

void tick_manager_generate_tick(tick_type_t);
void tick_manager_stop_tick(tick_type_t);
bool tick_manager_shift_work_tick(unsigned int delta_us);
unsigned int tick_manager_get_tick_interval_ms(tick_type_t);

#endif /* TICK_MANAGER_H */
