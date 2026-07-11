#ifndef RUNTIME_MANAGER_H
#define RUNTIME_MANAGER_H

#include <runtime_types.h>
#include <stdint.h>

bool schedule_user_work(const runtime_work_item_t *item);
bool schedule_system_work(const runtime_work_item_t *item);
bool runtime_manager_register_hook(curfew_hook_t);
bool runtime_manager_unregister_hook(curfew_hook_t);
void runtime_manager_set_active_state(bool ui_active);

void runtime_manager_init(void);
void runtime_manager_deinit(void);
runtime_state_t runtime_get_state(void);

#endif /* RUNTIME_MANAGER_H */
