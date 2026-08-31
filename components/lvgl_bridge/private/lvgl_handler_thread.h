#ifndef LVGL_HANDLER_THREAD_H
#define LVGL_HANDLER_THREAD_H

#include <stdint.h>
#include <lvgl_bridge_types.h>

void start_lvgl_thread(wakelock_check_func_t func);
void stop_lvgl_thread(void);
bool lvgl_thread_exists(void);
bool update_input_timeout(uint32_t timeout);
uint32_t get_input_timeout(void);

#endif /* LVGL_HANDLER_THREAD_H */
