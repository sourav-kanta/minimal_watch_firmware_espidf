#include <runtime_watchdog.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <runtime_worker_pool.h>
#include <worker_pool_types.h>
#include <runtime_utils.h>
#include <runtime_consts.h>
#include <worker_pool_types.h>

static const char* TAG = "Runtime watchdog";
static worker_registry_t* workers = NULL;

static void worker_soft_abort_cb(void* data) {
    assert(data);
    worker_registry_t* work = (worker_registry_t*) data;
    ESP_LOGW(TAG, "Worker %d exceeded soft timeout", work->worker_id);
    atomic_store(&work->abort_flag, true);
    assert(work->hard_close);
    esp_timer_stop(work->soft_close);
    esp_timer_stop(work->hard_close);
    esp_timer_start_once(work->hard_close, TIMEOUT_USER_WORK_HARD_MS*1000);
}

static void worker_hard_abort_cb(void* data) {
    assert(data);
    worker_registry_t* work = (worker_registry_t*) data;
    ESP_LOGE(TAG, "Worker %d exceeded hard timeout", work->worker_id);
    atomic_store(&work->abort_flag, true);
    assert(work->hard_close);
    esp_timer_stop(work->hard_close);
    // Recover the stalled worker 
    worker_pool_recover_stalled_worker(work, true);
}

void watchdog_start_user_work(worker_registry_t* worker) {
    assert(worker);
    assert(worker->soft_close);
    ESP_LOGD(TAG, "Started watchdog for worker %d", worker->worker_id);
    esp_timer_stop(worker->soft_close);
    esp_timer_start_once(worker->soft_close, TIMEOUT_USER_WORK_SOFT_MS*1000); 
}

void watchdog_stop_user_work(worker_registry_t* worker) {
    assert(worker);
    assert(worker->soft_close);
    assert(worker->hard_close);
    ESP_LOGD(TAG, "Stopped watchdog for worker %d", worker->worker_id);
    esp_timer_stop(worker->soft_close);
    esp_timer_stop(worker->hard_close);
}

void watchdog_force_all_mandatory_abort(void) {
    assert(workers);
    for(int i=0;i<WORKER_POOL_SIZE; i++) {
        if(atomic_load(&workers[i].working)) {
            if(workers[i].type == WORK_TYPE_USER) { 
                esp_timer_stop(workers[i].soft_close);
                esp_timer_stop(workers[i].hard_close);
            }
            worker_pool_recover_stalled_worker(&workers[i], false);
        }
    }
}

void watchdog_force_all_cooperative_abort(void) {
    assert(workers);
    for(int i=0; i<WORKER_POOL_SIZE; i++) {
        atomic_store(&workers[i].abort_flag, true);
    }
}

void watchdog_manager_init_stalled_worker(worker_registry_t* worker) {
    if(!worker) {
        ESP_LOGE(TAG, "Trying to init a NULL worker");
        return;
    }
    init_timer(&worker->soft_close, worker_soft_abort_cb, "Soft expiry timer", worker);
    init_timer(&worker->hard_close, worker_hard_abort_cb, "Hard expiry timer", worker);
}

void watchdog_manager_init(worker_registry_t* registry) {
    assert(registry);
    workers = registry;
    for(int i=0; i<WORKER_POOL_USER_ALLOCATION; i++) {
        init_timer(&workers[i].soft_close, worker_soft_abort_cb, "Soft expiry timer", &workers[i]);
        init_timer(&workers[i].hard_close, worker_hard_abort_cb, "Hard expiry timer", &workers[i]);
    }
}

void watchdog_manager_deinit(void) {
    for(int i=0; i<WORKER_POOL_USER_ALLOCATION; i++) {
        safe_timer_cleanup(&workers[i].soft_close); 
        safe_timer_cleanup(&workers[i].hard_close); 
    }
    workers = NULL;
}
