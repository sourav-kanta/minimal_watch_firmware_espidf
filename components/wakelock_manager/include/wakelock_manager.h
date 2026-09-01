#ifndef WAKELOCK_MANAGER_H
#define WAKELOCK_MANAGER_H

void wakelock_manager_init(void);
void wakelock_manager_deinit(void);

void wakelock_manager_acquire_wakelock(void);
void wakelock_manager_release_wakelock(void);
bool wakelock_manager_is_wake_locked(void);

#endif /* WAKELOCK_MANAGER_H */
