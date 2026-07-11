#ifndef RUNTIME_WATCHDOG_H
#define RUNTIME_WATCHDOG_H

#include <worker_pool_types.h>
#include <stdint.h>

void watchdog_manager_init(worker_registry_t*);
void watchdog_manager_init_stalled_worker(worker_registry_t*);
void watchdog_manager_deinit(void);
void watchdog_start_user_work(worker_registry_t*);
void watchdog_stop_user_work(worker_registry_t*);
void watchdog_force_all_cooperative_abort(void);
void watchdog_force_all_mandatory_abort(void); 

#endif /* RUNTIME_WATCHDOG_H */
