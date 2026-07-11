#include <runtime_manager.h>
#include <runtime_manager_internal.h>
#include <runtime_consts.h>
#include <runtime_locks.h>
#include <runtime_worker_pool.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <semaphore.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <string.h>
#include <runtime_utils.h>
#include <runtime_watchdog.h>
#include <common_types.h>
#include <event_manager.h>
#include <gpio_manager.h>

typedef struct {
    int64_t window_start_time;
    int64_t grace_start_time;
    esp_timer_handle_t total_window_timer;
    esp_timer_handle_t grace_period_timer;
    esp_timer_handle_t curfew_settlement_timer;
} window_ctx_t;

static const char* TAG = "Runtime Manager";
static QueueHandle_t user_work = NULL;
static QueueHandle_t system_work = NULL;
static QueueHandle_t pending_user_work = NULL;
static QueueHandle_t pending_system_work = NULL;
static SemaphoreHandle_t runtime_lock = NULL;
static runtime_state_t runtime_state = RUNTIME_STATE_SLEEP;
static curfew_hook_t window_hooks[MAX_CURFEW_HOOKS];
static uint8_t active_hook_count = 0;
static bool initialized = false;
static const int runtime_lock_timeout_ms = 50;
static window_ctx_t window_ctx;
static runtime_state_t work_state = RUNTIME_STATE_UI_ACTIVE;

// Needs to be called from locked state
static void drain_pending_queue(QueueHandle_t queue_src, QueueHandle_t queue_dest) {
    assert(queue_src);
    assert(queue_dest);
    if(runtime_state == RUNTIME_STATE_GRACE_PERIOD) {
        ESP_LOGE(TAG, "Invalid drain during grace"); 
    }
    else {
        runtime_work_item_t work_item;
        while(xQueueReceive(queue_src, &work_item, 0) == pdTRUE) {
            BaseType_t result = xQueueSend(queue_dest, &work_item, 0);
            if(result != pdTRUE) {
                ESP_LOGE(TAG, "Unable to push task, skipping");
            }
        }
    }
}

static void set_runtime_state(runtime_state_t target_state) {
    WITH_RUNTIME_LOCK() {
        if(runtime_state == target_state) {
            ESP_LOGE(TAG, "Invalid transition, skip");
        }
        else if(target_state == RUNTIME_STATE_UI_ACTIVE || 
                target_state == RUNTIME_STATE_BACKGROUND_ACTIVE) {
            if(runtime_state == RUNTIME_STATE_SLEEP) {
                runtime_state = target_state;
                ESP_LOGD(TAG, "Worker window start %s", 
                         runtime_state == RUNTIME_STATE_UI_ACTIVE ? "UI active" : "Background");
                window_ctx.window_start_time = esp_timer_get_time();
                int64_t window_duration = runtime_state == RUNTIME_STATE_UI_ACTIVE ? 
                                          WINDOW_UI_MAX_MS*1000 : WINDOW_BACKGROUND_MAX_MS*1000;
                assert(window_ctx.window_start_time);
                gpio_manager_debug_led_on();
                esp_timer_start_once(window_ctx.total_window_timer, window_duration);
                // Resume worker pools
                worker_pool_resume_all();
                runtime_manager_evaluate_early_curfew();
            }
            else {
                ESP_LOGE(TAG, "Invalid runtime state transition to active from non sleep state, skipping");
            }
        }
        else if(target_state == RUNTIME_STATE_SLEEP) {
            esp_timer_stop(window_ctx.total_window_timer);
            esp_timer_stop(window_ctx.grace_period_timer);
            esp_timer_stop(window_ctx.curfew_settlement_timer);
            runtime_state = target_state;
            drain_pending_queue(pending_user_work, user_work);
            drain_pending_queue(pending_system_work, system_work);
            // Watchdog abort and suspend worker pools
            watchdog_force_all_mandatory_abort();
            worker_pool_suspend_all();
            gpio_manager_debug_led_off();
            ESP_LOGD(TAG, "Worker window stopped, sleeping");
        }
    }
}
                                  
static void curfew_hook_adapter(void* arg1, runtime_abort_flag_t* flag) {
    curfew_hook_t hook = (curfew_hook_t) arg1;
    if(hook) {
        ESP_LOGD(TAG, "Executing hooked task");
        hook();
    }
}

// Needs to be called from inside a lock
static void runtime_manager_begin_curfew_unsafe(void) {
    esp_timer_stop(window_ctx.total_window_timer);
    esp_timer_stop(window_ctx.curfew_settlement_timer);
    if(active_hook_count == 0 && !worker_pool_has_work()) {
        set_runtime_state(RUNTIME_STATE_SLEEP);
        ESP_LOGD(TAG, "No work to be done dropping to sleep");
    }
    else {
        runtime_state = RUNTIME_STATE_GRACE_PERIOD;
        assert(window_ctx.grace_period_timer);
        esp_timer_start_once(window_ctx.grace_period_timer, WINDOW_GRACE_PERIOD_MS*1000);
        window_ctx.grace_start_time = esp_timer_get_time();
        for(int i=0; i<active_hook_count; i++) {
            runtime_work_item_t work_item = {
                .type = WORK_TYPE_SYSTEM,
                .priority = RUNTIME_SYSTEM_PRIORITY,
                .handler = curfew_hook_adapter,
                .arg1 = (void*)window_hooks[i]
            };
            if(pdPASS != xQueueSend(system_work, &work_item, 0)) {
                ESP_LOGE(TAG, "Unable to submit hook, skipping");
            }
        }
        ESP_LOGD(TAG, "Work window finished, switching to grace");
    }
}

static void work_window_expiry_cb(void* arg) {
    WITH_RUNTIME_LOCK() {
        if(runtime_state == RUNTIME_STATE_BACKGROUND_ACTIVE || runtime_state == RUNTIME_STATE_UI_ACTIVE) {
            ESP_LOGD(TAG, "Work window expired");
            runtime_manager_begin_curfew_unsafe();
        }
    }
}

static void grace_period_expiry_cb(void* arg) {
    WITH_RUNTIME_LOCK() {
        if(runtime_state == RUNTIME_STATE_GRACE_PERIOD) {
            ESP_LOGD(TAG, "Grace period expired");
            esp_timer_stop(window_ctx.grace_period_timer);
            // Watchdog force cooperative abort
            watchdog_force_all_cooperative_abort();
            xQueueReset(user_work);
            set_runtime_state(RUNTIME_STATE_SLEEP);
        }
    }
}

static inline bool is_system_completely_idle(void) { 
    return !worker_pool_has_work();
}

static void curfew_settlement_expiry_cb(void* arg) {
    WITH_RUNTIME_LOCK() {
        if(runtime_state == RUNTIME_STATE_BACKGROUND_ACTIVE || runtime_state == RUNTIME_STATE_UI_ACTIVE) {
            if(is_system_completely_idle()) {
                ESP_LOGD(TAG, "No work to be done, entering early curfew");
                runtime_manager_begin_curfew_unsafe();
            }
        }
        else if(runtime_state == RUNTIME_STATE_GRACE_PERIOD) {
            if(is_system_completely_idle()) {
                ESP_LOGD(TAG, "No work to be done in grace period, drop to sleep");
                set_runtime_state(RUNTIME_STATE_SLEEP);
            }
        }
    }
}

void runtime_manager_evaluate_early_curfew(void) {
    WITH_RUNTIME_LOCK() {
        if(runtime_state == RUNTIME_STATE_UI_ACTIVE || 
           runtime_state == RUNTIME_STATE_BACKGROUND_ACTIVE) {
            if(is_system_completely_idle()) {
                if(esp_timer_is_active(window_ctx.curfew_settlement_timer)){
                    ESP_LOGD(TAG, "Settlement timer armed, wait till expiry");
                }
                else {
                    int64_t elapsed_now = esp_timer_get_time() - window_ctx.window_start_time;
                    int64_t limit_ms = (runtime_state == RUNTIME_STATE_UI_ACTIVE) ?
                                    WINDOW_UI_MAX_MS : WINDOW_BACKGROUND_MAX_MS;
                    int64_t remaining = limit_ms*1000 - elapsed_now;
                    if(remaining > 0) {
                        int64_t debounce_duration = (remaining < WINDOW_IDLE_DEBOUNCE_INTERVAL_MS*1000) ?
                                                   remaining : WINDOW_IDLE_DEBOUNCE_INTERVAL_MS*1000;
                        esp_timer_start_once(window_ctx.curfew_settlement_timer, debounce_duration);
                    }
                    // Total work window expiry will handle grace transition
                }
            }
        }
        else if(runtime_state == RUNTIME_STATE_GRACE_PERIOD) {
            if(is_system_completely_idle()) {
                if(esp_timer_is_active(window_ctx.curfew_settlement_timer)){
                    ESP_LOGD(TAG, "Settlement timer armed, wait till expiry");
                }
                else {
                    int64_t elapsed_now = esp_timer_get_time() - window_ctx.grace_start_time;
                    int64_t remaining = WINDOW_GRACE_PERIOD_MS*1000 - elapsed_now;
                    if(remaining > 0) {
                        int64_t debounce_duration = (remaining < WINDOW_IDLE_DEBOUNCE_INTERVAL_MS*1000) ?
                                                   remaining : WINDOW_IDLE_DEBOUNCE_INTERVAL_MS*1000;
                        esp_timer_start_once(window_ctx.curfew_settlement_timer, debounce_duration);
                    }
                    // grace timer expiry will handle transition to sleep
                }
            }
        }
    }
}

void runtime_manager_reset_settlement_timer(void) {
    WITH_RUNTIME_LOCK() {
        esp_timer_stop(window_ctx.curfew_settlement_timer);
    }
}

static void start_work_cb(const event_t* event) {
    if(initialized) {
        set_runtime_state(work_state);
    } 
}

void runtime_manager_init(void) {
    if(initialized) return;
    user_work = xQueueCreate(MAX_USER_WORK_PER_WINDOW, sizeof(runtime_work_item_t));
    if(user_work == NULL) {
        ESP_LOGE(TAG, "Unable to allocate user work queue");
        esp_system_abort("User work queue unallocated");
    }
    system_work = xQueueCreate(MAX_SYSTEM_WORK_PER_WINDOW, sizeof(runtime_work_item_t));
    if(system_work == NULL) {
        ESP_LOGE(TAG, "Unable to allocate system work queue");
        esp_system_abort("System work queue unallocated");
    }
    pending_user_work = xQueueCreate(MAX_USER_WORK_PER_WINDOW/2, sizeof(runtime_work_item_t));
    if(pending_user_work == NULL) {
        ESP_LOGE(TAG, "Unable to allocate user work queue");
        esp_system_abort("User work queue unallocated");
    }
    pending_system_work = xQueueCreate(MAX_SYSTEM_WORK_PER_WINDOW/2, sizeof(runtime_work_item_t));
    if(pending_system_work == NULL) {
        ESP_LOGE(TAG, "Unable to allocate system work queue");
        esp_system_abort("System work queue unallocated");
    }
    runtime_lock = xSemaphoreCreateRecursiveMutex();
    if(runtime_lock == NULL) {
        ESP_LOGE(TAG, "Fatal: Failed to allocate runtime mutex memory");
        esp_system_abort("Runtime Mutex Allocation Failure");

    }
    active_hook_count = 0;
    runtime_state = RUNTIME_STATE_SLEEP;
    memset(&window_ctx, 0, sizeof(window_ctx_t));
    ESP_LOGI(TAG, "Runtime manager initialized");
    worker_pool_init(user_work, system_work);
    init_timer(&window_ctx.total_window_timer, work_window_expiry_cb, "Work timer", NULL);
    init_timer(&window_ctx.grace_period_timer, grace_period_expiry_cb, "Grace timer", NULL);
    init_timer(&window_ctx.curfew_settlement_timer, curfew_settlement_expiry_cb, "Curfew timer", NULL);
    initialized = true;
    work_state = RUNTIME_STATE_UI_ACTIVE;
    event_subscribe(EVENT_WORK_TICK, start_work_cb);
}

void runtime_manager_deinit(void) {
    set_runtime_state(RUNTIME_STATE_SLEEP);
    event_unsubscribe(EVENT_WORK_TICK, start_work_cb);
    safe_timer_cleanup(&window_ctx.total_window_timer);
    safe_timer_cleanup(&window_ctx.grace_period_timer);
    safe_timer_cleanup(&window_ctx.curfew_settlement_timer);
    worker_pool_deinit();
    if(user_work) {
        vQueueDelete(user_work);
        user_work = NULL;
    }
    if(system_work) {
        vQueueDelete(system_work);
        system_work = NULL;
    }   
    if(pending_user_work) {
        vQueueDelete(pending_user_work);
        pending_user_work = NULL;
    }
    if(pending_system_work) {
        vQueueDelete(pending_system_work);
        pending_system_work = NULL;
    }   
    if(runtime_lock) {
        vSemaphoreDelete(runtime_lock);
        runtime_lock = NULL;
    }
    active_hook_count = 0;
    initialized = false;
}

void runtime_manager_set_active_state(bool ui_active_state) {
    if(ui_active_state)
        work_state = RUNTIME_STATE_UI_ACTIVE;
    else
        work_state = RUNTIME_STATE_BACKGROUND_ACTIVE;
}

bool acquire_lock_runtime(void) {
    if(runtime_lock == NULL) {
        ESP_LOGE(TAG, "Runtime lock is null");
        return false;
    }
    const TickType_t timeout_ticks = pdMS_TO_TICKS(runtime_lock_timeout_ms);
    return (xSemaphoreTakeRecursive(runtime_lock, timeout_ticks) == pdTRUE);
}

bool relinquish_lock_runtime() {
    if (runtime_lock == NULL) {
        ESP_LOGE(TAG, "Error: Attempted to release lock on an uninitialized mutex");
        return false;
    }
    return (xSemaphoreGiveRecursive(runtime_lock) == pdTRUE);
}

runtime_state_t runtime_get_state(void) {
    runtime_state_t state = RUNTIME_STATE_SLEEP;
    WITH_RUNTIME_LOCK() {
        state = runtime_state;
    }
    return state;
}

bool schedule_user_work(const runtime_work_item_t *item) {
    if(!initialized || !user_work) return false;
    if(item == NULL || item->handler == NULL) {
        ESP_LOGW(TAG, "Invalid user work, skipping");
        return false;
    }
    BaseType_t result = pdPASS;
    WITH_RUNTIME_LOCK() {
        if(runtime_state == RUNTIME_STATE_GRACE_PERIOD) {
            result = xQueueSend(pending_user_work, item, 0);
        }
        else {
            result = xQueueSend(user_work, item, 0);
        }
    }
    return (result == pdPASS);
}

bool schedule_system_work(const runtime_work_item_t *item) {
    if(!initialized || !system_work) return false;
    if(item == NULL || item->handler == NULL) {
        ESP_LOGW(TAG, "Invalid user work, skipping");
        return false;
    }
    BaseType_t result = pdPASS;
    WITH_RUNTIME_LOCK() {
        if(runtime_state == RUNTIME_STATE_GRACE_PERIOD) {
            result = xQueueSend(pending_system_work, item, 0);
        }
        else {
            result = xQueueSend(system_work, item, 0);
        }
    }
    return (result== pdPASS);
}

bool runtime_manager_register_hook(curfew_hook_t hook) {
    if(!initialized || !hook) return false;
    bool success = true;
    WITH_RUNTIME_LOCK() {
        if(active_hook_count >= MAX_CURFEW_HOOKS) {
            ESP_LOGE(TAG, "Reached max curfew hooks, skipping");
            success = false;
        }
        else {
            success = true;
            window_hooks[active_hook_count++] = hook;
        }
    }
    if(success)
        ESP_LOGI(TAG, "Hook registration successful");
    return success;
}

bool runtime_manager_unregister_hook(curfew_hook_t hook) {
    if(!initialized || !hook) return false;
    bool success = false;
    WITH_RUNTIME_LOCK() {
        for(int i=0; i<active_hook_count; i++) {
            if(window_hooks[i] == hook) {
                success = true;
            }
            if(success && i!=(active_hook_count-1)) {
                window_hooks[i] = window_hooks[i+1];
            } 
        }
        if(success) {
            active_hook_count--;
            window_hooks[active_hook_count] = NULL;
            ESP_LOGI(TAG, "Hook unregistration successful");
        }
    }
    return success;
}
