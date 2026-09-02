#ifndef DRAW_TAB_PAGE_H
#define DRAW_TAB_PAGE_H

#include <lvgl.h>
#include <ui_base_types.h>

void draw_tab_page(lv_obj_t*, ui_tab_handlers_t* tab);
void clean_tab_page(ui_tab_handlers_t* tab);

#endif /* DRAW_TAB_PAGE_H */
