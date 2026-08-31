#ifndef UI_BASE_H
#define UI_BASE_H

#include <common_types.h>
#include <ui_base_types.h>

void draw_base_screen(void);
void clean_base_screen(void);
void suspend_base_screen(void);
void resume_base_screen(void);
void handle_base_screen_event(ui_base_screen_event_t* ui_event);

#endif /* UI_BASE_H */
