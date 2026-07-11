#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_system.h>
#include <esp_log.h>

#include "global_locks.h"

static const char* TAG = "GLOBAL_LOCKS";
static const int ui_lock_timeout_ms = 250;
static SemaphoreHandle_t ui_mutex = NULL;

void init_locks(void) {
    if (ui_mutex == NULL) {
        ui_mutex = xSemaphoreCreateRecursiveMutex();
        if (ui_mutex == NULL) {
            ESP_LOGE(TAG, "Fatal: Failed to allocate global UI mutex memory");
            esp_system_abort("UI Mutex Allocation Failure");
        }

        ESP_LOGI(TAG, "Global system locks initialized successfully");
    } else {
        ESP_LOGW(TAG, "Global locks already initialized. Skipping.");
    }
}

bool acquire_lock_ui(void) {
    if (ui_mutex == NULL) {
        ESP_LOGE(TAG, "Error: Attempted to acquire lock before calling init_locks()");
        return false;
    }
    const TickType_t timeout_ticks = pdMS_TO_TICKS(ui_lock_timeout_ms);
    return (xSemaphoreTakeRecursive(ui_mutex, timeout_ticks) == pdTRUE);
}

bool relinquish_ui_lock(void) {
    if (ui_mutex == NULL) {
        ESP_LOGE(TAG, "Error: Attempted to release lock on an uninitialized mutex");
        return false;
    }

    return (xSemaphoreGiveRecursive(ui_mutex) == pdTRUE);
}
