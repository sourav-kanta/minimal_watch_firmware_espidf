#ifndef APP_MANAGER_H 
#define APP_MANAGER_H

#include <lvgl.h>
#include <common_types.h>
#include <app_types.h> 

void app_manager_init(void);
void app_manager_deinit(void);
void show_app_picker_ui(lv_obj_t* parent);
void del_app_picker_ui(void);
void open_app(const application_t*, lv_obj_t* parent);
void close_curr_app(void);
bool check_if_app_running(void);
const application_t* get_current_app(void);

#endif /* APP_MANAGER_H */
