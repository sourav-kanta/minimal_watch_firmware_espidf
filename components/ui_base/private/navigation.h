#ifndef NAVIGATION_H
#define NAVIGATION_H

#include <lvgl.h>

lv_obj_t* find_first_focusable_child_dfs(lv_obj_t*);
lv_obj_t* find_first_focusable_parent_dfs(lv_obj_t*);
lv_obj_t* find_next_focusable_sibling(lv_obj_t*);
lv_obj_t* find_prev_focusable_sibling(lv_obj_t*);

#endif /* NAVIGATION_H */
