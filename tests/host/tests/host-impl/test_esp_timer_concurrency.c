#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define CONTROL_THREAD_COUNT        4
#define CONTROL_TEST_DURATION_MS    100
#define DELETE_STRESS_ITERATIONS    100

#define TEST_WAIT_TIMEOUT_MS        1000

static void sleep_us(uint64_t duration_us) {
    struct timespec remaining = {
        .tv_sec = duration_us / 1000000ULL,
        .tv_nsec = (duration_us % 1000000ULL) * 1000ULL,
    };

    while (nanosleep(&remaining, &remaining) != 0) {
        assert(errno == EINTR);
    }
}

static void wait_for_atomic_int(atomic_int *value,
                                int expected,
                                uint32_t timeout_ms,
                                const char *name) {
    uint32_t elapsed = 0;

    while (atomic_load(value) != expected) {
        vTaskDelay(pdMS_TO_TICKS(1));
        elapsed++;
        if (elapsed >= timeout_ms) {
            fprintf(stderr,
                    "[FAIL] timeout waiting for %s: expected %d, got %d\n",
                    name,
                    expected,
                    atomic_load(value));
            assert(false);
        }
    }
}

static void wait_for_atomic_bool(atomic_bool *value,
                                 uint32_t timeout_ms,
                                 const char *name) {
    uint32_t elapsed = 0;

    while (!atomic_load(value)) {
        vTaskDelay(pdMS_TO_TICKS(1));
        elapsed++;
        if (elapsed >= timeout_ms) {
            fprintf(stderr,
                    "[FAIL] timeout waiting for %s\n",
                    name);
            assert(false);
        }
    }
}

static atomic_int concurrent_callback_count;

static void concurrent_callback(void *arg) {
    (void)arg;
    atomic_fetch_add(&concurrent_callback_count, 1);
    sleep_us(5000);
}

/*
 * Test: concurrent timer expiry.
 *
 * Verifies that 8 independent timers can expire around the same time
 * and all callbacks get dispatched.
 */
static void test_multiple_timers_concurrently(void) {
    fprintf(stderr, "[TEST] multiple timers concurrently\n");

    atomic_store(&concurrent_callback_count, 0);
    esp_timer_handle_t timers[8];
    esp_timer_create_args_t args = {
        .callback = concurrent_callback,
        .arg = NULL,
        .name = "concurrent",
    };

    for (size_t i = 0; i < 8; i++) {
        assert(esp_timer_create(&args, &timers[i]) == 0);
        assert(esp_timer_start_once(timers[i], 10000) == 0);
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    assert(atomic_load(&concurrent_callback_count) == 8);

    for (size_t i = 0; i < 8; i++) {
        assert(!esp_timer_is_active(timers[i]));
        assert(esp_timer_delete(timers[i]) == 0);
    }
}

struct timer_control_context {
    esp_timer_handle_t timer;
    atomic_bool stop;
    atomic_int finished_tasks;
};

static void timer_control_task(void *arg) {
    struct timer_control_context *context = arg;

    while (!atomic_load(&context->stop)) {
        assert(esp_timer_start_once(context->timer, 1000) == 0);

        (void)esp_timer_is_active(context->timer);

        assert(esp_timer_stop(context->timer) == 0);

        taskYIELD();
    }

    atomic_fetch_add(&context->finished_tasks, 1);
    vTaskDelete(NULL);
}

/*
 * Test: concurrent timer control.
 *
 * Verifies that multiple tasks can concurrently start, query, and stop
 * the same timer without corrupting timer state or causing API failures.
 */
static void test_concurrent_timer_control(void) {
    fprintf(stderr, "[TEST] concurrent start/stop/is_active\n");

    esp_timer_create_args_t args = {
        .callback = concurrent_callback,
        .arg = NULL,
        .name = "control",
    };
    esp_timer_handle_t timer = NULL;

    assert(esp_timer_create(&args, &timer) == 0);

    struct timer_control_context context = {
        .timer = timer,
    };
    atomic_init(&context.stop, false);
    atomic_init(&context.finished_tasks, 0);

    TaskHandle_t tasks[CONTROL_THREAD_COUNT];

    for (size_t i = 0; i < CONTROL_THREAD_COUNT; i++) {
        assert(xTaskCreate(
            timer_control_task,
            "timer_ctrl",
            2048,
            &context,
            2,
            &tasks[i]
        ) == pdPASS);
    }

    vTaskDelay(pdMS_TO_TICKS(CONTROL_TEST_DURATION_MS));

    atomic_store(&context.stop, true);

    wait_for_atomic_int(
        &context.finished_tasks,
        CONTROL_THREAD_COUNT,
        TEST_WAIT_TIMEOUT_MS,
        "timer control tasks"
    );

    assert(esp_timer_stop(timer) == 0);
    assert(!esp_timer_is_active(timer));
    assert(esp_timer_delete(timer) == 0);
}

static atomic_bool delete_callback_entered;
static atomic_bool delete_callback_finished;
static atomic_bool delete_task_finished;

static void delete_callback(void *arg) {
    (void)arg;

    atomic_store(&delete_callback_entered, true);

    sleep_us(20000);

    atomic_store(&delete_callback_finished, true);
}

static void delete_callback_task(void *arg) {
    esp_timer_handle_t timer = arg;

    uint32_t elapsed = 0;

    while (!atomic_load(&delete_callback_entered)) {
        vTaskDelay(pdMS_TO_TICKS(1));
        elapsed++;
        if (elapsed >= TEST_WAIT_TIMEOUT_MS) {
            fprintf(stderr,
                    "[FAIL] timeout waiting for delete callback to enter\n");
            assert(false);
        }
    }

    assert(esp_timer_delete(timer) == 0);

    atomic_store(&delete_task_finished, true);
    vTaskDelete(NULL);
}

/*
 * Test: deleting a timer during its callback.
 *
 * Verifies that a timer can be safely deleted while its callback is
 * executing and that the callback is allowed to finish without accessing
 * freed timer state.
 */
static void test_delete_during_callback(void) {
    fprintf(stderr, "[TEST] delete during callback\n");

    atomic_store(&delete_callback_entered, false);
    atomic_store(&delete_callback_finished, false);
    atomic_store(&delete_task_finished, false);

    esp_timer_create_args_t args = {
        .callback = delete_callback,
        .arg = NULL,
        .name = "delete-race",
    };
    esp_timer_handle_t timer = NULL;

    assert(esp_timer_create(&args, &timer) == 0);

    TaskHandle_t task;

    assert(xTaskCreate(
        delete_callback_task,
        "timer_delete",
        2048,
        timer,
        2,
        &task
    ) == pdPASS);

    assert(esp_timer_start_once(timer, 1000) == 0);

    wait_for_atomic_bool(
        &delete_task_finished,
        TEST_WAIT_TIMEOUT_MS,
        "timer delete task"
    );

    vTaskDelay(pdMS_TO_TICKS(30));

    assert(atomic_load(&delete_callback_finished));
}

static atomic_int stress_callback_count;

static void stress_callback(void *arg) {
    (void)arg;

    atomic_fetch_add(&stress_callback_count, 1);
}

struct delete_stress_context {
    atomic_int finished_tasks;
};

static void delete_stress_task(void *arg) {
    struct delete_stress_context *context = arg;

    for (int i = 0; i < DELETE_STRESS_ITERATIONS; i++) {
        esp_timer_create_args_t args = {
            .callback = stress_callback,
            .arg = NULL,
            .name = "delete-stress",
        };

        esp_timer_handle_t timer = NULL;

        assert(esp_timer_create(&args, &timer) == 0);
        assert(esp_timer_start_once(timer, 1000) == 0);

        vTaskDelay(pdMS_TO_TICKS(1));

        assert(esp_timer_delete(timer) == 0);
    }

    atomic_fetch_add(&context->finished_tasks, 1);
    vTaskDelete(NULL);
}

/*
 * Test: repeated concurrent timer lifecycle.
 *
 * Verifies that timers can be repeatedly created, started, and deleted
 * from multiple tasks without leaking or corrupting timer state.
 */
static void test_repeated_create_start_delete(void) {
    fprintf(stderr, "[TEST] repeated create/start/delete\n");

    atomic_store(&stress_callback_count, 0);

    struct delete_stress_context context;
    atomic_init(&context.finished_tasks, 0);
    TaskHandle_t tasks[CONTROL_THREAD_COUNT];

    for (size_t i = 0; i < CONTROL_THREAD_COUNT; i++) {
        assert(xTaskCreate(
            delete_stress_task,
            "timer_stress",
            2048,
            &context,
            2,
            &tasks[i]
        ) == pdPASS);
    }

    wait_for_atomic_int(
        &context.finished_tasks,
        CONTROL_THREAD_COUNT,
        TEST_WAIT_TIMEOUT_MS,
        "timer stress tasks"
    );
}

static void host_test_task(void *arg) {
    (void)arg;

    test_multiple_timers_concurrently();
    test_concurrent_timer_control();
    test_delete_during_callback();
    test_repeated_create_start_delete();

    fprintf(stderr, "[TEST] all esp_timer concurrency tests passed\n");

    vTaskEndScheduler();
    vTaskDelete(NULL);
}

int main(void) {
    assert(xTaskCreate(
        host_test_task,
        "host_test",
        4096,
        NULL,
        3,
        NULL
    ) == pdPASS);

    vTaskStartScheduler();
    return 0;
}
