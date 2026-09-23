#ifndef HOST_ESP_TIMER_H
#define HOST_ESP_TIMER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>

typedef int esp_err_t;

#define ESP_OK 0

#define ESP_ERROR_CHECK(x) \
    do { \
        esp_err_t __err_rc = (x); \
        assert(__err_rc == ESP_OK); \
    } while (0)

typedef struct esp_timer *esp_timer_handle_t;

typedef void (*esp_timer_cb_t)(void *arg);

typedef enum {
    ESP_TIMER_TASK,
    ESP_TIMER_ISR
} esp_timer_dispatch_t;

typedef struct {
    esp_timer_cb_t callback;
    void *arg;
    const char *name;
    esp_timer_dispatch_t dispatch_method;
    bool skip_unhandled_events;
} esp_timer_create_args_t;

int64_t esp_timer_get_time(void);

int esp_timer_create(const esp_timer_create_args_t *create_args,
                     esp_timer_handle_t *out_handle);

int esp_timer_delete(esp_timer_handle_t timer);

int esp_timer_start_once(esp_timer_handle_t timer,
                         uint64_t timeout_us);

int esp_timer_start_periodic(esp_timer_handle_t timer,
                             uint64_t period_us);

int esp_timer_stop(esp_timer_handle_t timer);

bool esp_timer_is_active(esp_timer_handle_t timer);

#endif
