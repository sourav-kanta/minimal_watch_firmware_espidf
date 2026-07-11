#ifndef GLOBAL_LOCKS_H
#define GLOBAL_LOCKS_H

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_log.h>
#include <stdbool.h>

void init_locks(void);
bool acquire_lock_ui(void);
bool relinquish_ui_lock(void);

static inline bool _log_ui_timeout(const char* func, int line) {
    ESP_LOGE("UI_LOCK", "Timeout! Could not acquire UI lock at %s:%d", func, line);
    return false; 
}

#define WITH_UI_LOCK() \
    for (bool _locked = acquire_lock_ui(), _run = true; \
         (_locked && _run) || (!_locked && _log_ui_timeout(__FUNCTION__, __LINE__)); \
         _run = false, relinquish_ui_lock())

#endif /* GLOBAL_LOCKS_H */
