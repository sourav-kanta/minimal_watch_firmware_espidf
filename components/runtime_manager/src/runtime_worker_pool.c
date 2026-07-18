#include <runtime_worker_pool.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <runtime_consts.h>
#include <runtime_types.h>
#include <esp_log.h>
#include <stdatomic.h>
#include <worker_pool_types.h>
#include <runtime_manager.h>
#include <string.h>
#include <runtime_manager_internal.h>
#include <runtime_utils.h>
#include <runtime_watchdog.h>

static const char* TAG = "Worker Pool";
static QueueHandle_t user_work = NULL;
static QueueHandle_t system_work = NULL;
static worker_registry_t workers[WORKER_POOL_SIZE];
static atomic_int active_workers = 0;

static void pool_worker_entry(void *pvParameters) {
    worker_registry_t* worker_metadata = (worker_registry_t*) pvParameters;
    vTaskPrioritySet(NULL, RUNTIME_BASELINE_PRIORITY); 
    QueueHandle_t queue = worker_metadata->type == WORK_TYPE_USER ?
                          user_work : system_work; 
    runtime_work_item_t item;
    while(1) {
        assert(queue);
        ESP_LOGD(TAG, "Inside  worker task %d", worker_metadata->worker_id);
        BaseType_t result = xQueueReceive(queue, &item, portMAX_DELAY);
        if(result == pdPASS) {
            ESP_LOGD(TAG, "Stack remaining = %u words", uxTaskGetStackHighWaterMark(NULL));
            runtime_manager_reset_settlement_timer();
            atomic_fetch_add(&active_workers, 1);
            bool is_user_task = item.type == WORK_TYPE_USER;
            BaseType_t priority = is_user_task? RUNTIME_USER_PRIORITY : RUNTIME_SYSTEM_PRIORITY;
            vTaskPrioritySet(NULL, priority);
            atomic_store(&worker_metadata->abort_flag, false);
            if(is_user_task) {
                // Start watchdog for task
                watchdog_start_user_work(worker_metadata);
            }
            atomic_store(&worker_metadata->working, true);
            item.handler(item.arg_payload, &worker_metadata->abort_flag);
            atomic_store(&worker_metadata->working, false);
            if(is_user_task) {
                // Stop watchdog timer for task
                watchdog_stop_user_work(worker_metadata);
            }
            atomic_store(&worker_metadata->abort_flag, true);
            vTaskPrioritySet(NULL, RUNTIME_BASELINE_PRIORITY);
            if(atomic_fetch_sub(&active_workers, 1) == 1) {
                // Evaluate early curfew
                runtime_manager_evaluate_early_curfew();
            }
        }
    }
}

void worker_pool_resume_all(void) {
    for(int i=0; i<WORKER_POOL_SIZE; i++) {
        if(workers[i].task_handle) {
            vTaskResume(workers[i].task_handle);
        }
    }
}

void worker_pool_suspend_all(void) {
    for(int i=0; i<WORKER_POOL_SIZE; i++) {
        if(workers[i].task_handle) {
            vTaskSuspend(workers[i].task_handle);
        }
    }
}

bool worker_pool_has_work(void) { 
    assert(user_work && system_work);
    return (atomic_load(&active_workers) > 0) || 
            uxQueueMessagesWaiting(system_work) || uxQueueMessagesWaiting(user_work);
}

void worker_pool_recover_stalled_worker(worker_registry_t* worker, bool resume) {
    assert(worker);
    if (!atomic_exchange(&worker->working, false)) {
        return;
    }
    worker_registry_t temp = *worker;
    if(temp.task_handle) {
        vTaskDelete(temp.task_handle);
        atomic_fetch_sub(&active_workers, 1);
    }
    memset(worker, 0, sizeof(worker_registry_t));
    worker->worker_id = temp.worker_id;
    worker->type = temp.type;
    if(temp.type == WORK_TYPE_USER) {
        // Reinitialize the watchdog timers
        watchdog_manager_init_stalled_worker(worker);
    }
    BaseType_t result = xTaskCreate(pool_worker_entry, "Pool worker", WORKER_STACK_SIZE_BYTES,
                                    worker, RUNTIME_BASELINE_PRIORITY, &worker->task_handle);
    atomic_store(&worker->abort_flag, false);
    atomic_store(&worker->working, false);
    if(result != pdPASS) {
        ESP_LOGE(TAG, "Failed to spawn worker %d", temp.worker_id);
        worker->task_handle = NULL;
    }
    if(!resume) {
        if(worker->task_handle)
            vTaskSuspend(worker->task_handle);
    }
}

void worker_pool_init(QueueHandle_t user_queue, QueueHandle_t system_queue) {
    user_work = user_queue;
    system_work = system_queue;
    int idx = 0;
    for(int i=0; i<WORKER_POOL_USER_ALLOCATION; i++) {
        memset(&workers[idx], 0, sizeof(worker_registry_t));
        workers[idx].worker_id = idx;
        workers[idx].type = WORK_TYPE_USER;
        atomic_store(&workers[idx].abort_flag, false);
        atomic_store(&workers[idx].working, false);
        BaseType_t result = xTaskCreate(pool_worker_entry, "Pool worker", WORKER_STACK_SIZE_BYTES,
                                        &workers[idx], RUNTIME_BASELINE_PRIORITY, &workers[idx].task_handle);
        if(result != pdPASS) {
            ESP_LOGE(TAG, "Failed to spawn worker %d", idx);
            workers[idx].task_handle = NULL;
        }
        ESP_LOGI(TAG, "Runtime worker created %d", idx);
        idx++;
    }
    for(int i=0; i<WORKER_POOL_SYSTEM_ALLOCATION; i++) {
        memset(&workers[idx], 0, sizeof(worker_registry_t));
        workers[idx].worker_id = idx;
        workers[idx].type = WORK_TYPE_SYSTEM;
        atomic_store(&workers[idx].abort_flag, false);
        BaseType_t result = xTaskCreate(pool_worker_entry, "Pool worker", WORKER_STACK_SIZE_BYTES,
                                        &workers[idx], RUNTIME_BASELINE_PRIORITY, &workers[idx].task_handle);
        if(result != pdPASS) {
            ESP_LOGE(TAG, "Failed to spawn worker %d", idx);
            workers[idx].task_handle = NULL;
        }
        ESP_LOGI(TAG, "Runtime worker created %d", idx);
        idx++; 
    }
    ESP_LOGI(TAG, "Runtime worker pool initialized");
    watchdog_manager_init(workers);
    vTaskDelay(pdMS_TO_TICKS(10));
    worker_pool_suspend_all();
}

void worker_pool_deinit(void) {
    watchdog_manager_deinit();
    worker_pool_suspend_all();
    for(int i=0; i<WORKER_POOL_SIZE; i++) {
        if (workers[i].task_handle) {
            vTaskDelete(workers[i].task_handle);
        }
        memset(&workers[i], 0, sizeof(worker_registry_t));
    }
    active_workers = 0;
    user_work = NULL;
    system_work = NULL;
}

