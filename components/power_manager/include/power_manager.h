#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

typedef uint32_t shutdown_reason_t;
#define POWER_MANAGER_SHUTDOWN_REASON_DEEP_SLEEP    0x1
#define POWER_MANAGER_SHUTDOWN_REASON_DFU           0x2

void power_manager_init(TaskHandle_t main);
void power_manager_deinit(void);

void power_manager_prevent_sleep(void);
void power_manager_allow_sleep(void);

void power_manager_engage_deep_sleep(void);

#endif /* POWER_MANAGER_H */
