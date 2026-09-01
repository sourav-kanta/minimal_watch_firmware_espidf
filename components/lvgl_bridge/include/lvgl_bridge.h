#ifndef LVGL_BRIDGE_H
#define LVGL_BRIDGE_H

#include <stdint.h>

void init_lvgl(void);
void deinit_lvgl(void);
void suspend_lvgl(void);
void resume_lvgl(void);
bool lvgl_bridge_update_inactivity_timeout(uint32_t timeout);
uint32_t lvgl_bridge_get_inactivity_timeout(void);

#endif /* LVGL_BRIDGE_H */
