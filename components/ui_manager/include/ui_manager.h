#ifndef UI_MANAGER_H
#define UI_MANAGER_H

void ui_manager_init(void);
void ui_manager_deinit(void);
void ui_off(void);
void ui_on(void);
void ui_sleep(void);
void ui_resume(void);

void ui_manager_acquire_wakelock(void);
void ui_manager_release_wakelock(void);
bool ui_manager_is_wake_locked(void);

#endif /* UI_MANAGER_H */
