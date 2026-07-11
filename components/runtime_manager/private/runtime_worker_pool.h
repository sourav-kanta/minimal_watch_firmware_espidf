#ifndef RUNTIME_WORKER_POOL_H
#define RUNTIME_WORKER_POOL_H

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <worker_pool_types.h>

void worker_pool_init(QueueHandle_t user_work, QueueHandle_t system_work);
void worker_pool_deinit(void);
void worker_pool_resume_all(void);
void worker_pool_suspend_all(void);
bool worker_pool_has_work(void);
void worker_pool_recover_stalled_worker(worker_registry_t*, bool resume);

#endif /* RUNTIME_WORKER_POOL_H */
