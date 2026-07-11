#ifndef RUNTIME_CONSTS_H
#define RUNTIME_CONSTS_H

#define MAX_USER_WORK_PER_WINDOW            10
#define MAX_SYSTEM_WORK_PER_WINDOW          10

#define WORKER_POOL_SIZE                    4
#define WORKER_POOL_SYSTEM_ALLOCATION       2
#define WORKER_POOL_USER_ALLOCATION         (WORKER_POOL_SIZE - WORKER_POOL_SYSTEM_ALLOCATION)
#define WORKER_STACK_SIZE_BYTES             3072
#define RUNTIME_BASELINE_PRIORITY           1
#define RUNTIME_USER_PRIORITY               2
#define RUNTIME_SYSTEM_PRIORITY             3

#define TIMEOUT_USER_WORK_SOFT_MS           30
#define TIMEOUT_USER_WORK_HARD_MS           10
#define WINDOW_BACKGROUND_MAX_MS            160
#define WINDOW_GRACE_PERIOD_MS              200
#define WINDOW_UI_MAX_MS                    700
#define WINDOW_IDLE_DEBOUNCE_INTERVAL_MS    10

#define MAX_CURFEW_HOOKS                    4

#endif /* RUNTIME_CONSTS_H */
