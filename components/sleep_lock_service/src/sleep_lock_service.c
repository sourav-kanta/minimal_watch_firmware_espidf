#include <sleep_lock_service.h>

#include <stddef.h>
#include <esp_log.h>

static const char* TAG = "Sleep Lock";
static sleep_lock_interface_t* lock_impl = NULL;

void sleep_lock_service_init(sleep_lock_interface_t* apis) {
    lock_impl = apis;
}

void sleep_lock_service_deinit(void) {
    lock_impl = NULL;
}

void sleep_lock_service_acquire_lock(void) {
    if (lock_impl && lock_impl->sleep_lock_acquire) {
        lock_impl->sleep_lock_acquire();
    }
    else {
        ESP_LOGE(TAG, "Failed to acquire sleep lock. Service uninitialized");
    }
}

void sleep_lock_service_release_lock(void) {
    if (lock_impl && lock_impl->sleep_lock_release) {
        lock_impl->sleep_lock_release();
    }
    else {
        ESP_LOGE(TAG, "Failed to release sleep lock. Service uninitialized");
    }
}
