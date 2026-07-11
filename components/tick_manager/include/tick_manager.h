#ifndef TICK_MANAGER_H
#define TICK_MANAGER_H

#include <common_types.h>

typedef enum {
    TICK_WATCHFACE,
    TICK_WORK
} tick_type_t;


void tick_manager_init(void);
void tick_manager_deinit(void);

void tick_manager_generate_tick(tick_type_t);
void tick_manager_stop_tick(tick_type_t);

#endif /* TICK_MANAGER_H */
