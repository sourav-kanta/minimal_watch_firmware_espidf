#ifndef DEPENDENT_TYPES_H
#define DEPENDENT_TYPES_H

#include <event_manager.h>

typedef enum {
    DEPENDENT_TICK_SENSOR,
    DEPENDENT_TICK_INVALID,
} dependent_tick_type_t;

typedef struct {
    unsigned int interval_work_ticks;
    event_id_t event_mapping;
} dependent_tick_registry_t;

#endif /* DEPENDENT_TYPES_H */
