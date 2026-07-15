#ifndef NOTIFICATION_MANAGER_H
#define NOTIFICATION_MANAGER_H

#include <common_types.h>
#include <lvgl.h>

void notification_manager_init(void);
void notification_manager_deinit(void);
const notification_t* notification_manager_retreive_notification(unsigned int index);
const notification_t* notification_manager_retreive_all_notification(unsigned int* count);
void notification_manager_dismiss_notification(unsigned int index);
unsigned int notification_manager_get_notification_count(void);
void notification_manager_draw_notification_page(lv_obj_t* parent);
void notification_manager_delete_ui(void);

#endif /* NOTIFICATION_MANAGER_H */
