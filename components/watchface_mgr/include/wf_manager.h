#ifndef WF_MANAGER_H
#define WF_MANAGER_H

#include <ui_types.h>
#include <lvgl.h>

bool watchface_manager_select_wf(uint8_t idx);
const char* watchface_manager_get_selected_wf_name(void);
size_t watchface_manager_get_all_wf_names(const char** names);
size_t watchface_manager_get_total_wfs(void);
void watchface_manager_start_wf(lv_obj_t* parent);
void watchface_manager_stop_wf(void);
void watchface_manager_init(void);
void watchface_manager_deinit(void);
void watchface_manager_suspend(void);
void watchface_manager_resume(void);

void watchface_manager_register_wf(watchface_t* wf);

#endif /* WF_MANAGER_H */
