#ifndef POWER_TYPES_H
#define POWER_TYPES_H

typedef enum {
    POWER_CMD_UI_SLEEP,
    POWER_CMD_UI_WAKE,
    POWER_CMD_SHUTDOWN,
} power_cmd_t;

typedef enum {
    POWER_STATE_UI_ACTIVE,
    POWER_STATE_BACKGROUND,
    POWER_STATE_SLEEP
} power_state_t;

#endif /* POWER_TYPES_H */
