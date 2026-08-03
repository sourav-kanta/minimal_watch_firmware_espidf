#ifndef POPUP_LISTVIEW_H
#define POPUP_LISTVIEW_H

#include <lvgl.h>

typedef struct popup_registry_t popup_registry_t;

typedef struct {
    popup_registry_t *popup_registry;
    int index;
} popup_item_data_t;

struct popup_registry_t {
    lv_obj_t* popup;
    void (*select_cb) (int index);
    popup_item_data_t *item_data_array;
    int item_count;
};

void create_popup_listview(lv_obj_t* parent_container, popup_registry_t *popup_registry, 
                           listview_t *list_items, int item_count, void (*select_cb)(int index));

#endif /* POPUP_LISTVIEW_H */
