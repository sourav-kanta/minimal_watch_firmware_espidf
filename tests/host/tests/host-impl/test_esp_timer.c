#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static volatile int callback_count;
static volatile void *callback_arg_received;

static void sleep_us(uint64_t duration_us) {
    struct timespec remaining = {
        .tv_sec = duration_us / 1000000ULL,
        .tv_nsec = (duration_us % 1000000ULL) * 1000ULL,
    };

    while (nanosleep(&remaining, &remaining) != 0) {
        if (errno != EINTR) {
            abort();
        }
    }
}

static void test_callback(void *arg) {
    callback_count++;
    callback_arg_received = arg;
}

static void reset_callback_state(void) {
    callback_count = 0;
    callback_arg_received = NULL;
}

/*
 * Test: ESP timer time measurement.
 *
 * Verifies esp_timer_get_time() returns a monotonic value and measures
 * elapsed time within the expected range.
 */
static void test_get_time(void) {
    fprintf(stderr, "[TEST] esp_timer_get_time\n");

    int64_t start = esp_timer_get_time();

    sleep_us(10000);

    int64_t end = esp_timer_get_time();

    assert(end > start);

    int64_t elapsed = end - start;
    fprintf(stderr, "[TEST] esp_timer elapsed: %lld us\n",
        (long long)elapsed);
    assert(elapsed >= 5000);
    assert(elapsed < 1000000);
}

/*
 * Test: ESP timer creation and deletion.
 *
 * Verifies a timer can be created with callback arguments, starts inactive,
 * and can be deleted successfully.
 */
static void test_create(void) {
    fprintf(stderr, "[TEST] esp_timer_create\n");

    reset_callback_state();

    int marker = 1234;
    esp_timer_create_args_t args = {
        .callback = test_callback,
        .arg = &marker,
        .name = "test",
    };

    esp_timer_handle_t timer = NULL;

    assert(esp_timer_create(&args, &timer) == 0);
    assert(timer != NULL);
    assert(!esp_timer_is_active(timer));
    assert(esp_timer_delete(timer) == 0);
}

/*
 * Test: ESP timer one-shot operation.
 *
 * Verifies a one-shot timer expires once, invokes its callback with the
 * correct argument, becomes inactive, and does not fire again.
 */
static void test_one_shot(void) {
    fprintf(stderr, "[TEST] esp_timer_start_once\n");

    reset_callback_state();

    int marker = 5678;
    esp_timer_create_args_t args = {
        .callback = test_callback,
        .arg = &marker,
        .name = "one-shot",
    };
    esp_timer_handle_t timer = NULL;

    assert(esp_timer_create(&args, &timer) == 0);
    assert(esp_timer_start_once(timer, 20000) == 0);
    assert(esp_timer_is_active(timer));

    sleep_us(5000);

    assert(callback_count == 0);
    assert(esp_timer_is_active(timer));

    sleep_us(30000);

    assert(callback_count == 1);
    assert(callback_arg_received == &marker);
    assert(!esp_timer_is_active(timer));

    sleep_us(30000);

    assert(callback_count == 1);
    assert(esp_timer_delete(timer) == 0);
}

/*
 * Test: ESP timer periodic operation.
 *
 * Verifies a periodic timer invokes its callback repeatedly, remains active,
 * and stops invoking callbacks after it is stopped.
 */
static void test_periodic(void) {
    fprintf(stderr, "[TEST] esp_timer_start_periodic\n");

    reset_callback_state();

    esp_timer_create_args_t args = {
        .callback = test_callback,
        .arg = NULL,
        .name = "periodic",
    };
    esp_timer_handle_t timer = NULL;

    assert(esp_timer_create(&args, &timer) == 0);
    assert(esp_timer_start_periodic(timer, 10000) == 0);
    assert(esp_timer_is_active(timer));

    sleep_us(55000);

    assert(callback_count >= 3);
    assert(callback_count <= 8);
    assert(esp_timer_is_active(timer));

    int count_before_stop = callback_count;

    assert(esp_timer_stop(timer) == 0);
    assert(!esp_timer_is_active(timer));

    sleep_us(30000);

    assert(callback_count == count_before_stop);
    assert(esp_timer_delete(timer) == 0);
}

/*
 * Test: ESP timer stopping a one-shot timer.
 *
 * Verifies stopping an active one-shot timer prevents its callback from
 * executing after the scheduled expiry.
 */
static void test_stop_one_shot(void) {
    fprintf(stderr, "[TEST] esp_timer_stop one-shot\n");

    reset_callback_state();

    esp_timer_create_args_t args = {
        .callback = test_callback,
        .arg = NULL,
        .name = "stop-one-shot",
    };
    esp_timer_handle_t timer = NULL;

    assert(esp_timer_create(&args, &timer) == 0);
    assert(esp_timer_start_once(timer, 30000) == 0);
    assert(esp_timer_is_active(timer));

    sleep_us(5000);

    assert(esp_timer_stop(timer) == 0);
    assert(!esp_timer_is_active(timer));

    sleep_us(50000);

    assert(callback_count == 0);
    assert(esp_timer_delete(timer) == 0);
}

/*
 * Test: ESP timer one-shot restart.
 *
 * Verifies restarting an active one-shot timer replaces its previous expiry
 * rather than allowing the original expiry to trigger a callback.
 */
static void test_restart_one_shot(void) {
    fprintf(stderr, "[TEST] esp_timer restart one-shot\n");

    reset_callback_state();

    esp_timer_create_args_t args = {
        .callback = test_callback,
        .arg = NULL,
        .name = "restart",
    };
    esp_timer_handle_t timer = NULL;

    assert(esp_timer_create(&args, &timer) == 0);
    assert(esp_timer_start_once(timer, 50000) == 0);

    sleep_us(5000);

    /*
     * Restart it before the original expiry.
     */
    assert(esp_timer_start_once(timer, 30000) == 0);

    sleep_us(45000);

    assert(callback_count == 1);
    assert(!esp_timer_is_active(timer));
    assert(esp_timer_delete(timer) == 0);
}

static esp_timer_handle_t callback_restart_timer;
static int callback_restart_count;

static void callback_restart_handler(void *arg) {
    (void)arg;

    callback_count++;
    callback_restart_count++;

    if (callback_restart_count == 1) {
        assert(esp_timer_start_once(callback_restart_timer, 10000) == 0);
    }
}

/*
 * Test: ESP timer callback restart.
 *
 * Verifies a timer callback can safely restart its own one-shot timer and
 * that the restarted timer executes exactly once more.
 */
static void test_callback_can_restart_timer(void) {
    fprintf(stderr, "[TEST] esp_timer callback restart\n");

    reset_callback_state();

    callback_restart_count = 0;
    callback_restart_timer = NULL;
    esp_timer_create_args_t args = {
        .callback = callback_restart_handler,
        .arg = NULL,
        .name = "callback-restart",
    };

    assert(esp_timer_create(&args, &callback_restart_timer) == 0);
    assert(esp_timer_start_once(callback_restart_timer, 10000) == 0);

    sleep_us(50000);

    assert(callback_count == 2);
    assert(!esp_timer_is_active(callback_restart_timer));
    assert(esp_timer_delete(callback_restart_timer) == 0);

    callback_restart_timer = NULL;
}

static void host_test_task(void *arg) {
    (void)arg;

    test_get_time();
    test_create();
    test_one_shot();
    test_periodic();
    test_stop_one_shot();
    test_restart_one_shot();
    test_callback_can_restart_timer();

    fprintf(stderr, "[TEST] all esp_timer tests passed\n");

    vTaskEndScheduler();
    vTaskDelete(NULL);
}

int main(void) {
    assert(xTaskCreate(host_test_task,
                       "host_test",
                       4096,
                       NULL,
                       1,
                       NULL
           ) == pdPASS);
    vTaskStartScheduler();
    return 0;
}
