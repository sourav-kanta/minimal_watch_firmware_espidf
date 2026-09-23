#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <runtime_consts.h>
#include <runtime_watchdog.h>
#include <worker_pool_types.h>
#include <runtime_utils.h>

static atomic_int recovery_count;
static worker_registry_t *last_recovered_worker;
static bool last_recovery_resume;

void worker_pool_recover_stalled_worker(worker_registry_t *worker,
                                        bool resume) {
    assert(worker != NULL);

    last_recovered_worker = worker;
    last_recovery_resume = resume;

    atomic_fetch_add(&recovery_count, 1);
}

static void sleep_us(uint64_t duration_us) {
    struct timespec remaining = {
        .tv_sec = duration_us / 1000000ULL,
        .tv_nsec = (duration_us % 1000000ULL) * 1000ULL,
    };

    while (nanosleep(&remaining, &remaining) != 0) {
        assert(errno == EINTR);
    }
}

static void reset_recovery_state(void) {
    atomic_store(&recovery_count, 0);
    last_recovered_worker = NULL;
    last_recovery_resume = false;
}

static void initialize_workers(worker_registry_t *workers) {
    for (int i = 0; i < WORKER_POOL_SIZE; i++) {
        workers[i].worker_id = (uint8_t)i;
        atomic_init(&workers[i].working, false);
        workers[i].type =
            (i < WORKER_POOL_USER_ALLOCATION)
                ? WORK_TYPE_USER
                : WORK_TYPE_SYSTEM;
        workers[i].task_handle = NULL;
        atomic_init(&workers[i].abort_flag, false);
        workers[i].soft_close = NULL;
        workers[i].hard_close = NULL;
    }
}

static void assert_user_timers_initialized(worker_registry_t *workers) {
    for (int i = 0; i < WORKER_POOL_USER_ALLOCATION; i++) {
        assert(workers[i].soft_close != NULL);
        assert(workers[i].hard_close != NULL);
        assert(!esp_timer_is_active(workers[i].soft_close));
        assert(!esp_timer_is_active(workers[i].hard_close));
    }
}

static void assert_user_timers_cleaned(worker_registry_t *workers) {
    for (int i = 0; i < WORKER_POOL_USER_ALLOCATION; i++) {
        assert(workers[i].soft_close == NULL);
        assert(workers[i].hard_close == NULL);
    }
}

/*
 * Test: watchdog initialization.
 *
 * Verifies user workers receive inactive soft and hard watchdog timers,
 * system workers receive no watchdog timers, and deinitialization cleans
 * up the user timers.
 */
static void test_watchdog_init(void) {
    printf("test_watchdog_init...\n");

    worker_registry_t workers[WORKER_POOL_SIZE];

    initialize_workers(workers);
    reset_recovery_state();

    watchdog_manager_init(workers);

    assert_user_timers_initialized(workers);

    for (int i = WORKER_POOL_USER_ALLOCATION;
         i < WORKER_POOL_SIZE;
         i++) {
        assert(workers[i].soft_close == NULL);
        assert(workers[i].hard_close == NULL);
    }

    watchdog_manager_deinit();

    assert_user_timers_cleaned(workers);

    printf("  PASS\n");
}

/*
 * Test: watchdog start and stop.
 *
 * Verifies starting user work activates only the soft watchdog and stopping
 * the work deactivates both watchdog timers without triggering recovery.
 */
static void test_watchdog_start_stop(void) {
    printf("test_watchdog_start_stop...\n");

    worker_registry_t workers[WORKER_POOL_SIZE];

    initialize_workers(workers);
    reset_recovery_state();

    watchdog_manager_init(workers);

    worker_registry_t *worker = &workers[0];

    watchdog_start_user_work(worker);

    assert(esp_timer_is_active(worker->soft_close));
    assert(!esp_timer_is_active(worker->hard_close));

    watchdog_stop_user_work(worker);

    assert(!esp_timer_is_active(worker->soft_close));
    assert(!esp_timer_is_active(worker->hard_close));

    sleep_us((TIMEOUT_USER_WORK_SOFT_MS + 20) * 1000ULL);

    assert(atomic_load(&worker->abort_flag) == false);
    assert(atomic_load(&recovery_count) == 0);

    watchdog_manager_deinit();

    printf("  PASS\n");
}

/*
 * Test: soft timeout starts hard timeout.
 *
 * Verifies the soft timeout sets the worker's abort flag and transitions
 * from the soft watchdog to the hard watchdog. The hard timeout then
 * recovers the worker with resume enabled.
 */
static void test_soft_timeout_starts_hard_timeout(void) {
    printf("test_soft_timeout_starts_hard_timeout...\n");

    worker_registry_t workers[WORKER_POOL_SIZE];

    initialize_workers(workers);
    reset_recovery_state();

    watchdog_manager_init(workers);

    worker_registry_t *worker = &workers[0];

    watchdog_start_user_work(worker);

    sleep_us((TIMEOUT_USER_WORK_SOFT_MS + 5) * 1000ULL);

    assert(atomic_load(&worker->abort_flag) == true);
    assert(!esp_timer_is_active(worker->soft_close));
    assert(esp_timer_is_active(worker->hard_close));
    assert(atomic_load(&recovery_count) == 0);

    sleep_us((TIMEOUT_USER_WORK_HARD_MS + 20) * 1000ULL);

    assert(atomic_load(&recovery_count) == 1);
    assert(last_recovered_worker == worker);
    assert(last_recovery_resume == true);
    assert(!esp_timer_is_active(worker->hard_close));

    watchdog_manager_deinit();

    printf("  PASS\n");
}

/*
 * Test: stopping work prevents timeout.
 *
 * Verifies stopping user work before the soft timeout prevents both
 * watchdog stages from firing and prevents worker recovery.
 */
static void test_watchdog_stop_prevents_timeout(void) {
    printf("test_watchdog_stop_prevents_timeout...\n");

    worker_registry_t workers[WORKER_POOL_SIZE];

    initialize_workers(workers);
    reset_recovery_state();

    watchdog_manager_init(workers);

    worker_registry_t *worker = &workers[1];

    watchdog_start_user_work(worker);

    sleep_us(5000);

    watchdog_stop_user_work(worker);

    sleep_us((TIMEOUT_USER_WORK_SOFT_MS +
              TIMEOUT_USER_WORK_HARD_MS +
              20) * 1000ULL);

    assert(atomic_load(&worker->abort_flag) == false);
    assert(atomic_load(&recovery_count) == 0);
    assert(!esp_timer_is_active(worker->soft_close));
    assert(!esp_timer_is_active(worker->hard_close));

    watchdog_manager_deinit();

    printf("  PASS\n");
}

/*
 * Test: stalled worker initialization.
 *
 * Verifies a worker that is initialized after the watchdog manager is
 * already running receives the required inactive watchdog timers and
 * those timers can be started, stopped, and cleaned up normally.
 */
static void test_stalled_worker_initialization(void) {
    printf("test_stalled_worker_initialization...\n");

    worker_registry_t worker;

    worker.worker_id = 42;
    atomic_init(&worker.working, false);
    worker.type = WORK_TYPE_USER;
    worker.task_handle = NULL;
    atomic_init(&worker.abort_flag, false);
    worker.soft_close = NULL;
    worker.hard_close = NULL;

    watchdog_manager_init_stalled_worker(&worker);

    assert(worker.soft_close != NULL);
    assert(worker.hard_close != NULL);
    assert(!esp_timer_is_active(worker.soft_close));
    assert(!esp_timer_is_active(worker.hard_close));

    watchdog_start_user_work(&worker);
    watchdog_stop_user_work(&worker);

    safe_timer_cleanup(&worker.soft_close);
    safe_timer_cleanup(&worker.hard_close);

    assert(worker.soft_close == NULL);
    assert(worker.hard_close == NULL);

    printf("  PASS\n");
}

/*
 * Test: cooperative abort all workers.
 *
 * Verifies a cooperative abort request sets the abort flag for every
 * registered worker without triggering immediate worker recovery.
 */
static void test_cooperative_abort_all_workers(void) {
    printf("test_cooperative_abort_all_workers...\n");

    worker_registry_t workers[WORKER_POOL_SIZE];

    initialize_workers(workers);
    reset_recovery_state();

    watchdog_manager_init(workers);

    for (int i = 0; i < WORKER_POOL_SIZE; i++) {
        assert(atomic_load(&workers[i].abort_flag) == false);
    }

    watchdog_force_all_cooperative_abort();

    for (int i = 0; i < WORKER_POOL_SIZE; i++) {
        assert(atomic_load(&workers[i].abort_flag) == true);
    }

    assert(atomic_load(&recovery_count) == 0);

    watchdog_manager_deinit();

    printf("  PASS\n");
}

/*
 * Test: mandatory abort running workers.
 *
 * Verifies mandatory abort recovers only workers currently marked as
 * working, uses resume disabled, and stops any active watchdog timers.
 */
static void test_mandatory_abort_running_workers(void) {
    printf("test_mandatory_abort_running_workers...\n");

    worker_registry_t workers[WORKER_POOL_SIZE];

    initialize_workers(workers);
    reset_recovery_state();

    watchdog_manager_init(workers);

    atomic_store(&workers[0].working, true);
    atomic_store(&workers[1].working, false);
    atomic_store(&workers[2].working, true);
    atomic_store(&workers[3].working, false);
    atomic_store(&workers[4].working, true);
    atomic_store(&workers[5].working, false);

    watchdog_start_user_work(&workers[0]);

    assert(esp_timer_is_active(workers[0].soft_close));

    watchdog_force_all_mandatory_abort();

    assert(atomic_load(&recovery_count) == 3);
    assert(last_recovery_resume == false);
    assert(last_recovered_worker == &workers[4]);
    assert(!esp_timer_is_active(workers[0].soft_close));
    assert(!esp_timer_is_active(workers[0].hard_close));

    watchdog_manager_deinit();

    printf("  PASS\n");
}

/*
 * Test: NULL stalled worker initialization.
 *
 * Verifies stalled-worker initialization safely ignores a NULL worker
 * instead of dereferencing it.
 */
static void test_stalled_worker_null_initialization(void) {
    printf("test_stalled_worker_null_initialization...\n");

    watchdog_manager_init_stalled_worker(NULL);

    printf("  PASS\n");
}

static void host_test_task(void *arg) {
    (void)arg;

    test_watchdog_init();
    test_watchdog_start_stop();
    test_soft_timeout_starts_hard_timeout();
    test_watchdog_stop_prevents_timeout();
    test_stalled_worker_initialization();
    test_cooperative_abort_all_workers();
    test_mandatory_abort_running_workers();
    test_stalled_worker_null_initialization();

    printf("[TEST] all runtime watchdog tests passed\n");
    exit(0);
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
