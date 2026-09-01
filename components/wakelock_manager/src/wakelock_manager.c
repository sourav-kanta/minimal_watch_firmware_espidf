#include <wakelock_manager.h>
#include <esp_log.h>
#include <stdatomic.h>

static const char* TAG = "Wakelock manager";
static atomic_int wakelock_counter = 0;

void wakelock_manager_acquire_wakelock(void) {
    atomic_fetch_add(&wakelock_counter, 1);
    ESP_LOGD(TAG, "Acquired wakelock");
}

void wakelock_manager_release_wakelock(void) {
    if(atomic_fetch_add(&wakelock_counter, -1) <= 0) {
        ESP_LOGE(TAG, "Invalid wakelock release. Panic!!!!!");
        atomic_store(&wakelock_counter, 0);
    }
    else {
        ESP_LOGD(TAG, "Released wakelock");
    }
}

bool wakelock_manager_is_wake_locked(void) {
    return atomic_load(&wakelock_counter) > 0;
}

void wakelock_manager_init(void) {
    atomic_store(&wakelock_counter, 0);
}

void wakelock_manager_deinit(void) {
    assert(!wakelock_manager_is_wake_locked());
    atomic_store(&wakelock_counter, 0);
}

