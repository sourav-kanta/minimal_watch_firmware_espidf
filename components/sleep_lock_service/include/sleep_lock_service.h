#ifndef SLEEP_LOCK_SERVICE_H
#define SLEEP_LOCK_SERVICE_H

#include <esp_log.h>
#include <stdbool.h>

typedef struct {
    void (*sleep_lock_acquire)(void);
    void (*sleep_lock_release)(void);
} sleep_lock_interface_t;

void sleep_lock_service_init(sleep_lock_interface_t* interface);
void sleep_lock_service_deinit(void);
void sleep_lock_service_acquire_lock(void);
void sleep_lock_service_release_lock(void);

static inline void _log_sleep_lock_request(const char* func, int line) {
    ESP_LOGD("Sleep lock", "Lock requested at %s:%d", func, line);
}

#define WITH_SLEEP_LOCK() \
    for (bool _run = (sleep_lock_service_acquire_lock(), _log_sleep_lock_request(__FUNCTION__, __LINE__), true); \
         _run; \
         sleep_lock_service_release_lock(), _run = false)

#endif /* SLEEP_LOCK_SERVICE_H */
