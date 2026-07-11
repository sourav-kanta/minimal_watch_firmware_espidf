#ifndef PICKER_UI_H
#define PICKER_UI_H

#include <lvgl.h>
#include <app_types.h>

void draw_app_picker_ui(lv_obj_t*, application_t**, uint8_t);
void del_app_picker_ui(void);

#endif /* PICKER_UI_H */
