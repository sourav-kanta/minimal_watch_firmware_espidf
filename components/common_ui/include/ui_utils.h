#ifndef LVGL_HELPER_H
#define LVGL_HELPER_H

#include <lvgl.h>

typedef struct {
    const char *title;
    const char *value;
    void *data;
    lv_obj_t* value_label;
    void (*title_click_cb) (void* data);
} listview_t;

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

void remove_shadow_and_outline(lv_obj_t*);
void make_obj_navigable(lv_obj_t*);
void draw_horizoantal_divider(lv_obj_t* parent);

void draw_listview_item(lv_obj_t* base_obj, listview_t* item);
void create_popup_listview(lv_obj_t* parent_container, popup_registry_t *popup_registry, 
                           listview_t *list_items, int item_count, void (*select_cb)(int index));

#endif /* LVGL_HELPER_H */
