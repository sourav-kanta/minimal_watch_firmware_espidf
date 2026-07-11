#ifndef WORKER_POOL_TYPES_H
#define WORKER_POOL_TYPES_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <runtime_types.h>
#include <stdint.h>
#include <esp_timer.h>
#include <stdatomic.h>

typedef struct {
    uint8_t worker_id;
    atomic_bool working;
    runtime_work_type_t type;
    TaskHandle_t task_handle;
    runtime_abort_flag_t abort_flag;
    esp_timer_handle_t soft_close;
    esp_timer_handle_t hard_close;
} worker_registry_t;


#endif /* WORKER_POOL_TYPES_H */
