#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

void power_manager_init(void);
void power_manager_deinit(void);

void power_manager_prevent_sleep(void);
void power_manager_allow_sleep(void);

#endif /* POWER_MANAGER_H */
