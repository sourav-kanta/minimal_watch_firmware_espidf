#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <runtime_consts.h>
#include <runtime_types.h>
#include <runtime_watchdog.h>
#include <runtime_worker_pool.h>
#include <worker_pool_types.h>

static atomic_int settlement_reset_count;
static atomic_int early_curfew_count;

static atomic_int concurrent_workers;
static atomic_int max_concurrent_workers;
static atomic_int completed_workers;

static atomic_bool system_handler_started;
static atomic_bool system_handler_finished;

static atomic_bool handler_started;
static atomic_bool handler_finished;
static atomic_bool handler_saw_abort_clear;
static atomic_bool handler_saw_working;

static atomic_bool blocking_handler_release;
static atomic_int blocking_handlers_started;
static atomic_int blocking_handlers_finished;

static atomic_int stalled_handlers_started;

static atomic_bool recovery_handler_release;
static atomic_int recovery_handlers_started;
static atomic_int recovery_test_completed;

static atomic_int stalled_system_handlers_started;
static atomic_int suspended_recovery_handlers_started;
static atomic_int suspended_recovery_handlers_finished;

static void reset_parallel_state(void) {
    atomic_store(&concurrent_workers, 0);
    atomic_store(&max_concurrent_workers, 0);
    atomic_store(&completed_workers, 0);
}

static void reset_handler_state(void) {
    atomic_store(&handler_started, false);
    atomic_store(&handler_finished, false);
    atomic_store(&handler_saw_abort_clear, false);
    atomic_store(&handler_saw_working, false);
}

static void wait_for_atomic_true(atomic_bool *value,
                                 uint32_t timeout_ms,
                                 const char *name) {
    for (uint32_t elapsed = 0; elapsed < timeout_ms; elapsed++) {
        if (atomic_load(value)) {
            return;
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }

    fprintf(stderr,
            "Timed out waiting for %s after %u ms\n",
            name,
            timeout_ms);

    assert(atomic_load(value));
}

static void wait_for_condition(atomic_bool *condition,
                               uint32_t timeout_ms) {
    uint32_t elapsed = 0;

    while (!atomic_load(condition)) {
        vTaskDelay(pdMS_TO_TICKS(1));

        elapsed++;

        assert(elapsed <= timeout_ms);
    }
}

static void create_queues(QueueHandle_t *user_queue,
                          QueueHandle_t *system_queue) {
    *user_queue = xQueueCreate(
        MAX_USER_WORK_PER_WINDOW,
        sizeof(runtime_work_item_t)
    );

    *system_queue = xQueueCreate(
        MAX_SYSTEM_WORK_PER_WINDOW,
        sizeof(runtime_work_item_t)
    );

    assert(*user_queue != NULL);
    assert(*system_queue != NULL);
}

static void delete_queues(QueueHandle_t user_queue,
                          QueueHandle_t system_queue) {
    vQueueDelete(user_queue);
    vQueueDelete(system_queue);
}

void runtime_manager_reset_settlement_timer(void) {
    atomic_fetch_add(&settlement_reset_count, 1);
}

void runtime_manager_evaluate_early_curfew(void) {
    atomic_fetch_add(&early_curfew_count, 1);
}

static void isolation_system_handler(void *arg,
                                     runtime_abort_flag_t *abort_flag) {
    (void)arg;

    assert(abort_flag != NULL);

    atomic_store(&system_handler_started, true);

    /*
     * Wait until both dedicated user workers are occupied.
     *
     * This happens inside the system worker, so the host test task
     * does not have to wake up within the user's 30 ms watchdog window.
     */
    printf("  waiting for both user handlers to start...\n");

    uint32_t elapsed = 0;

    while (atomic_load(&blocking_handlers_started) < 2) {
        vTaskDelay(pdMS_TO_TICKS(1));

        elapsed++;
        assert(elapsed <= 15);
    }

    printf("  both user handlers started\n");

    atomic_store(&handler_saw_working, true);
    atomic_store(&blocking_handler_release, true);
    atomic_store(&system_handler_finished, true);
}

static void test_handler(void *arg,
                         runtime_abort_flag_t *abort_flag) {
    (void)arg;

    atomic_store(&handler_started, true);

    if (!atomic_load(abort_flag)) {
        atomic_store(&handler_saw_abort_clear, true);
    }

    vTaskDelay(pdMS_TO_TICKS(20));

    atomic_store(&handler_finished, true);
}

static void blocking_handler(void *arg,
                             runtime_abort_flag_t *abort_flag) {
    (void)arg;

    assert(abort_flag != NULL);

    atomic_fetch_add(&blocking_handlers_started, 1);
    atomic_store(&handler_started, true);

    if (!atomic_load(abort_flag)) {
        atomic_store(&handler_saw_abort_clear, true);
    }

    while (!atomic_load(&blocking_handler_release)) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    atomic_fetch_add(&blocking_handlers_finished, 1);
    atomic_store(&handler_finished, true);
}

static void parallel_handler(void *arg,
                             runtime_abort_flag_t *abort_flag) {
    (void)arg;
    (void)abort_flag;

    int current =
        atomic_fetch_add(&concurrent_workers, 1) + 1;

    int previous_max =
        atomic_load(&max_concurrent_workers);

    while (current > previous_max &&
           !atomic_compare_exchange_weak(
               &max_concurrent_workers,
               &previous_max,
               current)) {
    }

    vTaskDelay(pdMS_TO_TICKS(10));

    atomic_fetch_sub(&concurrent_workers, 1);
    atomic_fetch_add(&completed_workers, 1);
}

typedef struct {
    atomic_int executions[10];
} job_execution_state_t;

static job_execution_state_t job_state;

static void queued_job_handler(void *arg,
                               runtime_abort_flag_t *abort_flag) {
    (void)abort_flag;

    uint8_t job_id = *(uint8_t *)arg;

    assert(job_id < 10);

    atomic_fetch_add(&job_state.executions[job_id], 1);

    vTaskDelay(pdMS_TO_TICKS(2));
}

/*
 * This handler intentionally never returns.
 *
 * The watchdog must first set abort_flag during the soft timeout and
 * then forcibly recover the worker during the hard timeout.
 */
static void stalled_handler(void *arg,
                            runtime_abort_flag_t *abort_flag) {
    (void)arg;
    (void)abort_flag;

    atomic_fetch_add(&stalled_handlers_started, 1);

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

/*
 * This handler blocks until the test releases it.
 *
 * Two recovery jobs are submitted so both recovered workers must accept
 * a job instead of one worker consuming both jobs sequentially.
 */
static void recovery_test_handler(void *arg,
                                  runtime_abort_flag_t *abort_flag) {
    (void)arg;

    assert(abort_flag != NULL);

    atomic_fetch_add(&recovery_handlers_started, 1);

    while (!atomic_load(&recovery_handler_release)) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    atomic_fetch_add(&recovery_test_completed, 1);
}

/*
 * This handler intentionally never returns.
 *
 * It represents a system worker that has exceeded the runtime manager's
 * grace period and therefore must be recovered by mandatory abort.
 */
static void stalled_system_handler(void *arg,
                                   runtime_abort_flag_t *abort_flag) {
    (void)arg;
    (void)abort_flag;

    atomic_fetch_add(&stalled_system_handlers_started, 1);

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

static void suspended_recovery_handler(void *arg,
                                       runtime_abort_flag_t *abort_flag) {
    (void)arg;

    assert(abort_flag != NULL);

    atomic_fetch_add(&suspended_recovery_handlers_started, 1);

    vTaskDelay(pdMS_TO_TICKS(5));

    atomic_fetch_add(&suspended_recovery_handlers_finished, 1);
}

/*
 * Test: worker pool initialization and suspension.
 *
 * Verifies workers start suspended and queued work remains pending.
 */
static void test_worker_pool_init_and_suspend(void) {
    printf("test_worker_pool_init_and_suspend...\n");

    QueueHandle_t user_queue;
    QueueHandle_t system_queue;

    create_queues(&user_queue, &system_queue);

    reset_handler_state();

    atomic_store(&settlement_reset_count, 0);
    atomic_store(&early_curfew_count, 0);

    worker_pool_init(user_queue, system_queue);

    runtime_work_item_t item = {
        .handler = test_handler,
        .type = WORK_TYPE_USER,
        .priority = 0,
    };

    assert(xQueueSend(user_queue, &item, 0) == pdPASS);

    vTaskDelay(pdMS_TO_TICKS(20));

    assert(!atomic_load(&handler_started));
    assert(worker_pool_has_work());

    worker_pool_deinit();

    delete_queues(user_queue, system_queue);

    printf("  PASS\n");
}

/*
 * Test: user work execution.
 *
 * Verifies a user worker executes queued work with a clear abort flag
 * and triggers the expected runtime-manager hooks.
 */
static void test_user_work_execution(void) {
    printf("test_user_work_execution...\n");

    QueueHandle_t user_queue;
    QueueHandle_t system_queue;

    create_queues(&user_queue, &system_queue);

    reset_handler_state();

    atomic_store(&settlement_reset_count, 0);
    atomic_store(&early_curfew_count, 0);

    worker_pool_init(user_queue, system_queue);

    runtime_work_item_t item = {
        .handler = test_handler,
        .type = WORK_TYPE_USER,
        .priority = 0,
    };

    assert(xQueueSend(user_queue, &item, 0) == pdPASS);

    worker_pool_resume_all();

    wait_for_condition(
        &handler_started,
        100
    );

    assert(atomic_load(&handler_saw_abort_clear));

    wait_for_condition(
        &handler_finished,
        100
    );

    vTaskDelay(pdMS_TO_TICKS(10));

    assert(!worker_pool_has_work());
    assert(atomic_load(&settlement_reset_count) >= 1);
    assert(atomic_load(&early_curfew_count) >= 1);

    worker_pool_deinit();

    delete_queues(user_queue, system_queue);

    printf("  PASS\n");
}

/*
 * Test: system work execution.
 *
 * Verifies a system worker executes queued system work successfully.
 */
static void test_system_work_execution(void) {
    printf("test_system_work_execution...\n");

    QueueHandle_t user_queue;
    QueueHandle_t system_queue;

    create_queues(&user_queue, &system_queue);

    reset_handler_state();

    atomic_store(&settlement_reset_count, 0);
    atomic_store(&early_curfew_count, 0);

    worker_pool_init(user_queue, system_queue);

    runtime_work_item_t item = {
        .handler = test_handler,
        .type = WORK_TYPE_SYSTEM,
        .priority = 0,
    };

    assert(xQueueSend(system_queue, &item, 0) == pdPASS);

    worker_pool_resume_all();

    wait_for_condition(
        &handler_started,
        100
    );

    wait_for_condition(
        &handler_finished,
        100
    );

    vTaskDelay(pdMS_TO_TICKS(10));

    assert(!worker_pool_has_work());

    worker_pool_deinit();
    delete_queues(user_queue, system_queue);

    printf("  PASS\n");
}

/*
 * Test: queued work detection.
 *
 * Verifies worker_pool_has_work() reports queued and executing work,
 * then returns false after the work completes.
 */
static void test_worker_pool_has_queued_work(void) {
    printf("test_worker_pool_has_queued_work...\n");

    QueueHandle_t user_queue;
    QueueHandle_t system_queue;

    create_queues(&user_queue, &system_queue);

    reset_handler_state();

    worker_pool_init(user_queue, system_queue);
    assert(!worker_pool_has_work());

    runtime_work_item_t item = {
        .handler = blocking_handler,
        .type = WORK_TYPE_USER,
        .priority = 0,
    };

    atomic_store(&blocking_handlers_started, 0);
    atomic_store(&blocking_handlers_finished, 0);
    atomic_store(&blocking_handler_release, false);

    assert(xQueueSend(user_queue, &item, 0) == pdPASS);
    assert(worker_pool_has_work());

    worker_pool_resume_all();

    wait_for_condition(
        &handler_started,
        100
    );

    assert(worker_pool_has_work());

    atomic_store(&blocking_handler_release, true);

    wait_for_atomic_true(
        &handler_finished,
        100,
        "blocking handler to finish"
    );

    vTaskDelay(pdMS_TO_TICKS(10));

    assert(!worker_pool_has_work());

    worker_pool_deinit();
    delete_queues(user_queue, system_queue);

    printf("  PASS\n");
}

/*
 * Test: parallel worker execution.
 *
 * Verifies multiple workers can execute user work concurrently.
 */
static void test_parallel_worker_execution(void) {
    printf("test_parallel_worker_execution...\n");

    QueueHandle_t user_queue;
    QueueHandle_t system_queue;

    create_queues(&user_queue, &system_queue);

    reset_parallel_state();

    worker_pool_init(user_queue, system_queue);

    runtime_work_item_t item = {
        .handler = parallel_handler,
        .type = WORK_TYPE_USER,
        .priority = 0,
    };

    for (int i = 0; i < WORKER_POOL_SIZE; i++) {
        assert(xQueueSend(user_queue, &item, 0) == pdPASS);
    }

    worker_pool_resume_all();

    uint32_t elapsed = 0;

    while (atomic_load(&completed_workers) < WORKER_POOL_SIZE) {
        vTaskDelay(pdMS_TO_TICKS(1));

        elapsed++;
        assert(elapsed <= 200);
    }

    elapsed = 0;

    while (worker_pool_has_work()) {
        vTaskDelay(pdMS_TO_TICKS(1));

        elapsed++;
        assert(elapsed <= 200);
    }

    assert(atomic_load(&max_concurrent_workers) >= 2);
    assert(atomic_load(&concurrent_workers) == 0);
    assert(atomic_load(&completed_workers) == WORKER_POOL_SIZE);

    worker_pool_deinit();
    delete_queues(user_queue, system_queue);

    printf("  PASS\n");
}

/*
 * Test: worker pool suspend and resume.
 *
 * Verifies work remains queued while suspended, executes after resume,
 * and remains queued when the pool is suspended again.
 */
static void test_worker_pool_suspend_resume(void) {
    printf("test_worker_pool_suspend_resume...\n");

    QueueHandle_t user_queue;
    QueueHandle_t system_queue;

    create_queues(&user_queue, &system_queue);

    reset_handler_state();

    worker_pool_init(user_queue, system_queue);

    runtime_work_item_t item = {
        .handler = test_handler,
        .type = WORK_TYPE_USER,
        .priority = 0,
    };

    assert(xQueueSend(user_queue, &item, 0) == pdPASS);

    vTaskDelay(pdMS_TO_TICKS(20));

    assert(!atomic_load(&handler_started));
    assert(!atomic_load(&handler_finished));
    assert(worker_pool_has_work());

    worker_pool_resume_all();

    wait_for_atomic_true(
        &handler_started,
        100,
        "handler to start"
    );
    wait_for_atomic_true(
        &handler_finished,
        100,
        "handler to finish"
    );

    uint32_t elapsed = 0;

    while (worker_pool_has_work()) {
        vTaskDelay(pdMS_TO_TICKS(1));

        elapsed++;
        assert(elapsed <= 100);
    }

    worker_pool_suspend_all();

    atomic_store(&handler_started, false);
    atomic_store(&handler_finished, false);

    assert(xQueueSend(user_queue, &item, 0) == pdPASS);

    vTaskDelay(pdMS_TO_TICKS(20));

    assert(!atomic_load(&handler_started));
    assert(!atomic_load(&handler_finished));
    assert(worker_pool_has_work());

    worker_pool_deinit();
    delete_queues(user_queue, system_queue);

    printf("  PASS\n");
}

/*
 * Test: multiple queued jobs.
 *
 * Verifies ten distinct queued jobs are each executed exactly once.
 */
static void test_multiple_queued_jobs(void) {
    printf("test_multiple_queued_jobs...\n");

    QueueHandle_t user_queue;
    QueueHandle_t system_queue;

    create_queues(&user_queue, &system_queue);

    for (int i = 0; i < 10; i++) {
        atomic_store(&job_state.executions[i], 0);
    }

    worker_pool_init(user_queue, system_queue);

    for (uint8_t i = 0; i < 10; i++) {
        runtime_work_item_t item = {
            .handler = queued_job_handler,
            .type = WORK_TYPE_USER,
            .priority = 0,
        };

        item.arg_payload[0] = i;

        assert(xQueueSend(user_queue, &item, 0) == pdPASS);
    }

    assert(worker_pool_has_work());

    worker_pool_resume_all();

    uint32_t elapsed = 0;

    while (true) {
        bool all_complete = true;

        for (int i = 0; i < 10; i++) {
            if (atomic_load(&job_state.executions[i]) != 1) {
                all_complete = false;
                break;
            }
        }

        if (all_complete) {
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(1));

        elapsed++;
        assert(elapsed <= 200);
    }

    elapsed = 0;

    while (worker_pool_has_work()) {
        vTaskDelay(pdMS_TO_TICKS(1));

        elapsed++;
        assert(elapsed <= 100);
    }

    for (int i = 0; i < 10; i++) {
        assert(atomic_load(&job_state.executions[i]) == 1);
    }

    worker_pool_deinit();
    delete_queues(user_queue, system_queue);

    printf("  PASS\n");
}

/*
 * Test: user/system worker isolation.
 *
 * Verifies system work can execute while both dedicated user workers
 * are occupied.
 */
static void test_user_system_worker_isolation(void) {
    printf("test_user_system_worker_isolation...\n");

    QueueHandle_t user_queue;
    QueueHandle_t system_queue;

    create_queues(&user_queue, &system_queue);

    worker_pool_init(user_queue, system_queue);

    reset_handler_state();

    atomic_store(&system_handler_started, false);
    atomic_store(&system_handler_finished, false);
    atomic_store(&blocking_handlers_started, 0);
    atomic_store(&blocking_handlers_finished, 0);
    atomic_store(&blocking_handler_release, false);

    runtime_work_item_t user_item = {
        .handler = blocking_handler,
        .type = WORK_TYPE_USER,
    };

    runtime_work_item_t system_item = {
        .handler = isolation_system_handler,
        .type = WORK_TYPE_SYSTEM,
    };

    assert(xQueueSend(user_queue, &user_item, 0) == pdPASS);
    assert(xQueueSend(user_queue, &user_item, 0) == pdPASS);
    assert(xQueueSend(system_queue, &system_item, 0) == pdPASS);

    worker_pool_resume_all();

    printf("  waiting for system handler...\n");

    wait_for_atomic_true(
        &system_handler_started,
        50,
        "system handler to start"
    );

    printf("  system handler started\n");

    uint32_t elapsed = 0;

    while (atomic_load(&blocking_handlers_finished) < 2) {
        vTaskDelay(pdMS_TO_TICKS(1));

        elapsed++;
        assert(elapsed <= 100);
    }

    elapsed = 0;

    while (worker_pool_has_work()) {
        vTaskDelay(pdMS_TO_TICKS(1));

        elapsed++;
        assert(elapsed <= 100);
    }

    assert(atomic_load(&system_handler_finished));
    assert(atomic_load(&blocking_handlers_started) == 2);
    assert(atomic_load(&blocking_handlers_finished) == 2);
    assert(atomic_load(&blocking_handler_release));
    assert(atomic_load(&handler_saw_working));

    worker_pool_deinit();
    delete_queues(user_queue, system_queue);

    printf("  PASS\n");
}

/*
 * Test: watchdog worker recovery.
 *
 * Verifies stalled user workers are recovered by the watchdog and
 * replacement workers can execute subsequent work.
 */
static void test_watchdog_worker_recovery(void) {
    printf("test_watchdog_worker_recovery...\n");

    atomic_store(&stalled_handlers_started, 0);
    atomic_store(&recovery_handlers_started, 0);
    atomic_store(&recovery_test_completed, 0);
    atomic_store(&recovery_handler_release, false);

    QueueHandle_t user_queue;
    QueueHandle_t system_queue;

    create_queues(&user_queue, &system_queue);

    worker_pool_init(user_queue, system_queue);

    runtime_work_item_t stalled_item = {
        .handler = stalled_handler,
        .type = WORK_TYPE_USER,
        .priority = 0,
    };

    assert(xQueueSend(user_queue, &stalled_item, 0) == pdPASS);
    assert(xQueueSend(user_queue, &stalled_item, 0) == pdPASS);

    worker_pool_resume_all();

    printf("  waiting for both stalled handlers...\n");

    uint32_t elapsed = 0;

    while (atomic_load(&stalled_handlers_started) < 2) {
        vTaskDelay(pdMS_TO_TICKS(1));

        elapsed++;
        assert(elapsed <= 100);
    }

    printf("  both stalled handlers started\n");

    assert(atomic_load(&stalled_handlers_started) == 2);

    /*
     * Allow both workers to pass through the soft and hard watchdog
     * timeouts and complete recovery.
     */
    vTaskDelay(pdMS_TO_TICKS(80));

    runtime_work_item_t recovery_item = {
        .handler = recovery_test_handler,
        .type = WORK_TYPE_USER,
        .priority = 0,
    };

    assert(xQueueSend(user_queue, &recovery_item, 0) == pdPASS);
    assert(xQueueSend(user_queue, &recovery_item, 0) == pdPASS);

    printf("  waiting for both replacement handlers...\n");

    elapsed = 0;

    while (atomic_load(&recovery_handlers_started) < 2) {
        vTaskDelay(pdMS_TO_TICKS(1));

        elapsed++;
        assert(elapsed <= 200);
    }

    printf("  both replacement handlers started\n");

    assert(atomic_load(&recovery_handlers_started) == 2);

    atomic_store(&recovery_handler_release, true);

    elapsed = 0;

    while (atomic_load(&recovery_test_completed) < 2) {
        vTaskDelay(pdMS_TO_TICKS(1));

        elapsed++;
        assert(elapsed <= 100);
    }

    assert(atomic_load(&recovery_test_completed) == 2);

    elapsed = 0;

    while (worker_pool_has_work()) {
        vTaskDelay(pdMS_TO_TICKS(1));

        elapsed++;
        assert(elapsed <= 100);
    }

    worker_pool_deinit();
    delete_queues(user_queue, system_queue);

    printf("  PASS\n");
}

/*
 * Test: mandatory abort suspends recovered system workers.
 *
 * Verifies mandatory recovery recreates system workers in the suspended
 * state and they execute queued work only after an explicit resume.
 */
static void test_mandatory_abort_suspends_system_recovery(void) {
    printf("test_mandatory_abort_suspends_system_recovery...\n");

    atomic_store(&stalled_system_handlers_started, 0);
    atomic_store(&suspended_recovery_handlers_started, 0);
    atomic_store(&suspended_recovery_handlers_finished, 0);

    QueueHandle_t user_queue;
    QueueHandle_t system_queue;

    create_queues(&user_queue, &system_queue);

    worker_pool_init(user_queue, system_queue);

    runtime_work_item_t stalled_item = {
        .handler = stalled_system_handler,
        .type = WORK_TYPE_SYSTEM,
        .priority = 0,
    };

    for (int i = 0; i < WORKER_POOL_SYSTEM_ALLOCATION; i++) {
        assert(xQueueSend(system_queue, &stalled_item, 0) == pdPASS);
    }

    worker_pool_resume_all();

    printf("  waiting for all stalled system handlers...\n");

    uint32_t elapsed = 0;

    while (atomic_load(&stalled_system_handlers_started) <
           WORKER_POOL_SYSTEM_ALLOCATION) {
        vTaskDelay(pdMS_TO_TICKS(1));

        elapsed++;
        assert(elapsed <= 100);
    }

    assert(atomic_load(&stalled_system_handlers_started) ==
           WORKER_POOL_SYSTEM_ALLOCATION);

    printf("  all stalled system handlers started\n");

    /*
     * Recover the stalled workers as if the runtime manager had reached
     * the mandatory-abort point after the grace period.
     */
    watchdog_force_all_mandatory_abort();

    vTaskDelay(pdMS_TO_TICKS(20));

    runtime_work_item_t recovery_item = {
        .handler = suspended_recovery_handler,
        .type = WORK_TYPE_SYSTEM,
        .priority = 0,
    };

    for (int i = 0; i < WORKER_POOL_SYSTEM_ALLOCATION; i++) {
        assert(xQueueSend(system_queue, &recovery_item, 0) == pdPASS);
    }

    vTaskDelay(pdMS_TO_TICKS(20));

    /*
     * Recovered workers must remain suspended until the pool is resumed.
     */
    assert(atomic_load(&suspended_recovery_handlers_started) == 0);
    assert(atomic_load(&suspended_recovery_handlers_finished) == 0);
    assert(worker_pool_has_work());

    worker_pool_resume_all();

    elapsed = 0;

    while (atomic_load(&suspended_recovery_handlers_started) <
           WORKER_POOL_SYSTEM_ALLOCATION) {
        vTaskDelay(pdMS_TO_TICKS(1));

        elapsed++;
        assert(elapsed <= 100);
    }

    assert(atomic_load(&suspended_recovery_handlers_started) ==
           WORKER_POOL_SYSTEM_ALLOCATION);

    elapsed = 0;

    while (atomic_load(&suspended_recovery_handlers_finished) <
           WORKER_POOL_SYSTEM_ALLOCATION) {
        vTaskDelay(pdMS_TO_TICKS(1));

        elapsed++;
        assert(elapsed <= 100);
    }

    assert(atomic_load(&suspended_recovery_handlers_finished) ==
           WORKER_POOL_SYSTEM_ALLOCATION);

    elapsed = 0;

    while (worker_pool_has_work()) {
        vTaskDelay(pdMS_TO_TICKS(1));

        elapsed++;
        assert(elapsed <= 100);
    }

    worker_pool_deinit();
    delete_queues(user_queue, system_queue);

    printf("  PASS\n");
}

static void host_test_task(void *arg) {
    (void)arg;

    test_worker_pool_init_and_suspend();
    test_user_work_execution();
    test_system_work_execution();
    test_worker_pool_has_queued_work();
    test_parallel_worker_execution();
    test_worker_pool_suspend_resume();
    test_multiple_queued_jobs();
    test_user_system_worker_isolation();
    test_watchdog_worker_recovery();
    test_mandatory_abort_suspends_system_recovery();

    printf("[TEST] all runtime worker pool tests passed\n");

    exit(0);
}

int main(void) {
    BaseType_t result = xTaskCreate(
        host_test_task,
        "Host test",
        4096,
        NULL,
        RUNTIME_BASELINE_PRIORITY,
        NULL
    );

    assert(result == pdPASS);

    vTaskStartScheduler();
    return 0;
}
