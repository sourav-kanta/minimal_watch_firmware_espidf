#ifndef LISTVIEW_H
#define LISTVIEW_H

#include <lvgl.h>

typedef struct {
    char *title;
    char *value;
    lv_obj_t* value_label;
    void (*title_click_cb) (void);
} listview_t;

void draw_listview_item(lv_obj_t* base_obj, listview_t* item);

#endif /* LISTVIEW_H */
