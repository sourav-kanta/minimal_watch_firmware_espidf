#ifndef LVGL_INPUT_TRANSLATOR_H
#define LVGL_INPUT_TRANSLATOR_H

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <lvgl.h>

void setup_keyboard(QueueHandle_t);
void delete_keyboard(void);
void suspend_keyboard(void);
void resume_keyboard(void);
lv_group_t* get_key_group(void);
const lv_indev_t* get_keypad_indev(void);

#endif /* LVGL_INPUT_TRANSLATOR_H */
