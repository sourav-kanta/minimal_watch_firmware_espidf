#include "esp_timer.h"

#include <pthread.h>
#include <stdlib.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define HOST_ESP_TIMER_TASK_PRIORITY    4
#define HOST_ESP_TIMER_QUEUE_LENGTH     32
#define HOST_ESP_TIMER_TASK_STACK       2048

struct esp_timer {
    esp_timer_cb_t callback;
    void *arg;
    const char *name;

    bool active;
    bool periodic;

    uint64_t period_us;
    int64_t expiry_us;

    struct esp_timer *next;
};

typedef struct {
    esp_timer_cb_t callback;
    void *arg;
} timer_callback_item_t;

static pthread_mutex_t timer_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t timer_service_once = PTHREAD_ONCE_INIT;

static struct esp_timer *timer_list = NULL;

static QueueHandle_t callback_queue = NULL;
static TaskHandle_t callback_task = NULL;
static TaskHandle_t timer_service_task_handle = NULL;

/* ------------------------------------------------------------------------- */
/* Time                                                                      */
/* ------------------------------------------------------------------------- */

static int64_t monotonic_time_us(void) {
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        abort();
    }

    return ((int64_t)ts.tv_sec * 1000000LL) +
           ((int64_t)ts.tv_nsec / 1000LL);
}

int64_t esp_timer_get_time(void) {
    return monotonic_time_us();
}

/* ------------------------------------------------------------------------- */
/* Timer list                                                                */
/* ------------------------------------------------------------------------- */

static void add_timer(struct esp_timer *timer) {
    timer->next = timer_list;
    timer_list = timer;
}

static void remove_timer(struct esp_timer *timer) {
    struct esp_timer **current = &timer_list;

    while (*current != NULL) {
        if (*current == timer) {
            *current = timer->next;
            timer->next = NULL;
            return;
        }

        current = &(*current)->next;
    }
}

static struct esp_timer *find_next_expired_timer(int64_t now_us) {
    struct esp_timer *timer = timer_list;

    while (timer != NULL) {
        if (timer->active && timer->expiry_us <= now_us) {
            return timer;
        }

        timer = timer->next;
    }

    return NULL;
}

static struct esp_timer *find_next_active_timer(void) {
    struct esp_timer *timer = timer_list;
    struct esp_timer *next = NULL;

    while (timer != NULL) {
        if (timer->active &&
            (next == NULL || timer->expiry_us < next->expiry_us)) {
            next = timer;
        }

        timer = timer->next;
    }

    return next;
}

/* ------------------------------------------------------------------------- */
/* Callback dispatch                                                         */
/* ------------------------------------------------------------------------- */

static void timer_callback_task(void *arg) {
    (void)arg;

    timer_callback_item_t item;

    while (true) {
        if (xQueueReceive(callback_queue, &item, portMAX_DELAY) == pdPASS &&
            item.callback != NULL) {
            item.callback(item.arg);
        }
    }
}

/* ------------------------------------------------------------------------- */
/* Timer service                                                             */
/* ------------------------------------------------------------------------- */

static void timer_service_task(void *arg) {
    (void)arg;

    while (true) {
        pthread_mutex_lock(&timer_lock);

        int64_t now_us = monotonic_time_us();
        struct esp_timer *timer =
            find_next_expired_timer(now_us);

        if (timer != NULL) {
            if (timer->periodic) {
                timer->expiry_us += (int64_t)timer->period_us;
            }
            else {
                timer->active = false;
            }

            timer_callback_item_t callback_item = {
                .callback = timer->callback,
                .arg = timer->arg,
            };

            pthread_mutex_unlock(&timer_lock);

            /*
             * The callback is dispatched from the dedicated FreeRTOS
             * callback task rather than from the timer service task.
             */
            (void)xQueueSend(
                callback_queue,
                &callback_item,
                portMAX_DELAY
            );

            continue;
        }

        timer = find_next_active_timer();
        TickType_t wait_ticks = portMAX_DELAY;

        if (timer != NULL) {
            int64_t delay_us = timer->expiry_us - now_us;
            if (delay_us <= 0) {
                pthread_mutex_unlock(&timer_lock);
                continue;
            }
            uint64_t delay_ms =
                ((uint64_t)delay_us + 999ULL) / 1000ULL;
            wait_ticks = pdMS_TO_TICKS(delay_ms);
            if (wait_ticks == 0) {
                wait_ticks = 1;
            }
        }

        pthread_mutex_unlock(&timer_lock);

        /*
         * Sleep until the next timer expires or a timer operation wakes
         * us to recalculate the next expiry.
         */
        (void)ulTaskNotifyTake(
            pdTRUE,
            wait_ticks
        );
    }
}

/* ------------------------------------------------------------------------- */
/* Initialization                                                            */
/* ------------------------------------------------------------------------- */

static void initialize_timer_service(void) {
    callback_queue = xQueueCreate(
        HOST_ESP_TIMER_QUEUE_LENGTH,
        sizeof(timer_callback_item_t)
    );

    if (callback_queue == NULL) {
        abort();
    }

    BaseType_t result = xTaskCreate(
        timer_callback_task,
        "esp_timer_cb",
        HOST_ESP_TIMER_TASK_STACK,
        NULL,
        HOST_ESP_TIMER_TASK_PRIORITY,
        &callback_task
    );

    if (result != pdPASS) {
        abort();
    }

    result = xTaskCreate(
        timer_service_task,
        "esp_timer",
        HOST_ESP_TIMER_TASK_STACK,
        NULL,
        HOST_ESP_TIMER_TASK_PRIORITY,
        &timer_service_task_handle
    );

    if (result != pdPASS) {
        abort();
    }
}

static void start_timer_service(void) {
    initialize_timer_service();
}

static void ensure_timer_service_started(void) {
    pthread_once(
        &timer_service_once,
        start_timer_service
    );
}

static void wake_timer_service(void) {
    if (timer_service_task_handle != NULL) {
        xTaskNotifyGive(timer_service_task_handle);
    }
}

/* ------------------------------------------------------------------------- */
/* Public API                                                                */
/* ------------------------------------------------------------------------- */

int esp_timer_create(const esp_timer_create_args_t *create_args,
                     esp_timer_handle_t *out_handle) {
    if (create_args == NULL ||
        create_args->callback == NULL ||
        out_handle == NULL) {
        return -1;
    }

    ensure_timer_service_started();

    struct esp_timer *timer =
        calloc(1, sizeof(struct esp_timer));

    if (timer == NULL) {
        return -1;
    }

    timer->callback = create_args->callback;
    timer->arg = create_args->arg;
    timer->name = create_args->name;

    pthread_mutex_lock(&timer_lock);

    add_timer(timer);

    pthread_mutex_unlock(&timer_lock);

    *out_handle = timer;

    wake_timer_service();

    return 0;
}

int esp_timer_delete(esp_timer_handle_t timer) {
    if (timer == NULL) {
        return -1;
    }

    pthread_mutex_lock(&timer_lock);

    timer->active = false;
    remove_timer(timer);

    pthread_mutex_unlock(&timer_lock);

    wake_timer_service();

    free(timer);

    return 0;
}

int esp_timer_start_once(esp_timer_handle_t timer,
                         uint64_t timeout_us) {
    if (timer == NULL) {
        return -1;
    }

    ensure_timer_service_started();

    pthread_mutex_lock(&timer_lock);

    timer->periodic = false;
    timer->period_us = 0;
    timer->expiry_us =
        monotonic_time_us() + (int64_t)timeout_us;
    timer->active = true;

    pthread_mutex_unlock(&timer_lock);

    wake_timer_service();

    return 0;
}

int esp_timer_start_periodic(esp_timer_handle_t timer,
                             uint64_t period_us) {
    if (timer == NULL || period_us == 0) {
        return -1;
    }

    ensure_timer_service_started();

    pthread_mutex_lock(&timer_lock);

    timer->periodic = true;
    timer->period_us = period_us;
    timer->expiry_us =
        monotonic_time_us() + (int64_t)period_us;
    timer->active = true;

    pthread_mutex_unlock(&timer_lock);

    wake_timer_service();

    return 0;
}

int esp_timer_stop(esp_timer_handle_t timer) {
    if (timer == NULL) {
        return -1;
    }

    pthread_mutex_lock(&timer_lock);

    timer->active = false;

    pthread_mutex_unlock(&timer_lock);

    wake_timer_service();

    return 0;
}

bool esp_timer_is_active(esp_timer_handle_t timer) {
    if (timer == NULL) {
        return false;
    }

    pthread_mutex_lock(&timer_lock);

    bool active = timer->active;

    pthread_mutex_unlock(&timer_lock);

    return active;
}
