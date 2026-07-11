#ifndef RUNTIME_TYPES_H
#define RUNTIME_TYPES_H

#include <stdint.h>
#include <stdatomic.h>

typedef atomic_bool runtime_abort_flag_t;
#define MAX_WORKER_ARG_PAYLOAD              255

typedef enum {
    WORK_TYPE_SYSTEM,
    WORK_TYPE_USER
} runtime_work_type_t;

typedef enum {
    RUNTIME_STATE_SLEEP,
    RUNTIME_STATE_BACKGROUND_ACTIVE,
    RUNTIME_STATE_GRACE_PERIOD,
    RUNTIME_STATE_UI_ACTIVE
} runtime_state_t;

typedef struct {
    void (*handler)(void *arg1, runtime_abort_flag_t *abort_flag);
    runtime_work_type_t type; 
    uint8_t priority;
    void *arg1;
    uint8_t arg_payload[MAX_WORKER_ARG_PAYLOAD];
} runtime_work_item_t;

typedef void (*curfew_hook_t)(void);

#endif /* RUNTIME_TYPES_H */
