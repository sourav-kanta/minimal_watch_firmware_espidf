#ifndef LISTVIEW_H
#define LISTVIEW_H

#include <lvgl.h>

typedef struct {
    const char *title;
    const char *value;
    void *data;
    lv_obj_t* value_label;
    void (*title_click_cb) (void* data);
} listview_t;

void draw_listview_item(lv_obj_t* base_obj, listview_t* item);

#endif /* LISTVIEW_H */
