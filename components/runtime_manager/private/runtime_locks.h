#ifndef RUNTIME_LOCKS_H
#define RUNTIME_LOCKS_H

#include <runtime_manager.h>
#include <esp_log.h>

bool acquire_lock_runtime(void);
bool relinquish_lock_runtime(void);

static inline bool _log_runtime_timeout(const char* func, int line) {
    ESP_LOGE("Runtime Lock", "Timeout! Could not acquire runtime lock at %s:%d", func, line);
    return false; 
}

#define WITH_RUNTIME_LOCK() \
    for (bool _locked = acquire_lock_runtime(), _run = true; \
         (_locked && _run) || (!_locked && _log_runtime_timeout(__FUNCTION__, __LINE__)); \
         _run = false, relinquish_lock_runtime())


#endif /* RUNTIME_LOCKS_H */
