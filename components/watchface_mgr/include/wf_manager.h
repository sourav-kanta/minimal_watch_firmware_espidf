#ifndef WF_MANAGER_H
#define WF_MANAGER_H

#include <ui_types.h>
#include <lvgl.h>

bool watchface_manager_select_wf(uint8_t idx);
const watchface_t* watchface_manager_get_selected_wf(void);
void watchface_manager_start_wf(lv_obj_t* parent);
void watchface_manager_stop_wf(void);
void watchface_manager_init(void);
void watchface_manager_deinit(void);
void watchface_manager_suspend(void);
void watchface_manager_resume(void);

#endif /* WF_MANAGER_H */
