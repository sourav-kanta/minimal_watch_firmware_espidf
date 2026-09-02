#ifndef APP_MANAGER_H 
#define APP_MANAGER_H

#include <lvgl.h>
#include <common_types.h>
#include <app_types.h> 

void app_manager_init(void);
void app_manager_deinit(void);
void app_manager_show_app_picker_ui(lv_obj_t* parent);
void app_manager_del_app_picker_ui(void);

void app_manager_add_app(application_t* app);

#endif /* APP_MANAGER_H */
