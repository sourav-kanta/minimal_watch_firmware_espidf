#ifndef UI_BASE_H
#define UI_BASE_H

#include <common_types.h>
#include <ui_base_types.h>
#include <lvgl.h>

void ui_base_draw_base_screen(void);
void ui_base_clean_base_screen(void);
void ui_base_suspend_base_screen(void);
void ui_base_resume_base_screen(void);
void ui_base_handle_base_screen_event(ui_base_screen_event_t* ui_event);

void ui_base_register_tab(ui_state_t page, ui_tab_handlers_t* tab_handler);

#endif /* UI_BASE_H */
