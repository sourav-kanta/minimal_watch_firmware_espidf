#ifndef LVGL_BRIDGE_TYPES_H
#define LVGL_BRIDGE_TYPES_H

#include <stdint.h>

typedef bool (*wakelock_check_func_t) (void);
typedef struct {
    wakelock_check_func_t wakelock_check_func;
} lvgl_params_t;

#endif /* LVGL_BRIDGE_TYPES_H */
