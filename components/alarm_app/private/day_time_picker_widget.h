#ifndef DAY_TIME_PICKER_WIDGET_H
#define DAY_TIME_PICKER_WIDGET_H

#include <lvgl.h>

typedef struct {
    const lv_font_t* font;
    lv_obj_t* parent;
    const char* pattern;
} picker_params_t;

lv_obj_t* day_time_picker_widget_draw(picker_params_t* params, lv_obj_t** out_labels); 
void day_time_picker_widget_get_selected_value(const char* pattern, lv_obj_t** labels,  char* out_val);

#endif /* DAY_TIME_PICKER_WIDGET_H */
