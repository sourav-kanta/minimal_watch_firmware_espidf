#include <assert.h>
#include <stdio.h>
#include <stdatomic.h>
#include <stdint.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <event_manager.h>
#include <runtime_manager.h>
#include <runtime_types.h>
#include <runtime_consts.h>
#include <tick_manager.h>
#include <tick_consts.h>


#define TICK_TIMEOUT_MS                     3000
#define WORK_START_TIMEOUT_MS               1000
#define WORK_DURATION_MS                    10

#define PACKED_WORK_DURATION_MS             30
#define SYSTEM_PACKED_WORK_ITEM_COUNT       12
#define SYSTEM_PACKED_WORK_DURATION_MS      80

#define SYSTEM_GRACE_ABORT_WORK_DURATION_MS 1000
#define SYSTEM_GRACE_ABORT_WORKER_COUNT     4

#define DEINIT_EXISTING_WORK_COUNT          4
#define DEINIT_WORK_DURATION_MS             5

#define SCHEDULING_WORK_DURATION_MS         2


static atomic_bool work_started;
static atomic_bool work_finished;
static atomic_bool user_work_finished;
static atomic_bool system_work_finished;
static atomic_bool curfew_hook_finished;
static atomic_bool grace_user_work_finished;
static atomic_bool grace_system_work_finished;

static atomic_int packed_work_started;
static atomic_int packed_work_finished;
static atomic_int packed_work_active;
static atomic_int packed_work_max_active;

static atomic_int system_packed_work_started;
static atomic_int system_packed_work_finished;
static atomic_int system_packed_work_active;
static atomic_int system_packed_work_max_active;

static atomic_int test_work_tick_count;

static atomic_int system_grace_abort_started;
static atomic_int system_grace_abort_finished;
static atomic_int system_grace_abort_active;

static atomic_int scheduling_user_started;
static atomic_int scheduling_user_finished;
static atomic_int scheduling_system_started;
static atomic_int scheduling_system_finished;
static atomic_int scheduling_user_priority;
static atomic_int scheduling_system_priority;

static atomic_int deinit_user_started;
static atomic_int deinit_user_finished;
static atomic_int deinit_system_started;
static atomic_int deinit_system_finished;

static atomic_int hook_a_count;
static atomic_int hook_b_count;
static atomic_int hook_c_count;

static SemaphoreHandle_t test_leak_sem = NULL;

static void leaking_orphaned_worker_handler(void *arg, runtime_abort_flag_t *abort_flag) {
    (void)arg;
    (void)abort_flag;

    atomic_store(&work_started, true);
    
    xSemaphoreTake(test_leak_sem, portMAX_DELAY);

    while(1) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    xSemaphoreGive(test_leak_sem);
    atomic_store(&work_finished, true);
}

static void test_grace_user_work_handler(void *arg,
                                         runtime_abort_flag_t *abort_flag) {
    (void)arg;

    assert(abort_flag != NULL);

    atomic_store(&grace_user_work_finished, true);
}


static void test_grace_system_work_handler(void *arg,
                                           runtime_abort_flag_t *abort_flag) {
    (void)arg;

    assert(abort_flag != NULL);

    atomic_store(&grace_system_work_finished, true);
}


static void test_user_work_handler(void *arg,
                                   runtime_abort_flag_t *abort_flag) {
    (void)arg;

    assert(abort_flag != NULL);

    atomic_store(&user_work_finished, true);
}


static void test_system_work_handler(void *arg,
                                     runtime_abort_flag_t *abort_flag) {
    (void)arg;

    assert(abort_flag != NULL);

    atomic_store(&system_work_finished, true);
}


static void test_curfew_hook(void) {
    atomic_store(&curfew_hook_finished, true);
}


static void test_curfew_hook_schedules_work(void) {
    runtime_work_item_t user_work = {
        .handler = test_grace_user_work_handler,
        .type = WORK_TYPE_USER,
        .priority = RUNTIME_USER_PRIORITY,
    };

    runtime_work_item_t system_work = {
        .handler = test_grace_system_work_handler,
        .type = WORK_TYPE_SYSTEM,
        .priority = RUNTIME_SYSTEM_PRIORITY,
    };

    assert(runtime_get_state() == RUNTIME_STATE_GRACE_PERIOD);

    assert(schedule_user_work(&user_work));
    assert(schedule_system_work(&system_work));

    atomic_store(&curfew_hook_finished, true);
}

static void queue_flooding_curfew_hook(void) {
    runtime_work_item_t user_work = {
        .handler = test_grace_user_work_handler,
        .type = WORK_TYPE_USER,
        .priority = RUNTIME_USER_PRIORITY,
    };

    int success_count = 0;
    int failure_count = 0;

    // Attempt to flood the pending_user_work queue (which only holds MAX/2)
    for(int i = 0; i < MAX_USER_WORK_PER_WINDOW; i++) {
        if(schedule_user_work(&user_work)) {
            success_count++;
        } else {
            failure_count++;
        }
    }

    // Handle odd numbers safely (e.g., if MAX_USER_WORK_PER_WINDOW is 15, capacity is 7)
    int expected_success = MAX_USER_WORK_PER_WINDOW / 2;
    int expected_failure = MAX_USER_WORK_PER_WINDOW - expected_success;

    assert(success_count == expected_success);
    assert(failure_count == expected_failure);

    atomic_store(&curfew_hook_finished, true);
}

static void test_work_handler(void *arg,
                              runtime_abort_flag_t *abort_flag) {
    (void)arg;

    assert(abort_flag != NULL);

    atomic_store(&work_started, true);

    vTaskDelay(pdMS_TO_TICKS(WORK_DURATION_MS));

    atomic_store(&work_finished, true);
}


static void scheduling_user_work_handler(
        void *arg,
        runtime_abort_flag_t *abort_flag) {
    (void)arg;

    assert(abort_flag != NULL);

    atomic_fetch_add(&scheduling_user_started, 1);

    /*
     * Record the priority assigned by the production worker.
     */
    atomic_store(
        &scheduling_user_priority,
        uxTaskPriorityGet(NULL));

    vTaskDelay(pdMS_TO_TICKS(SCHEDULING_WORK_DURATION_MS));

    atomic_fetch_add(&scheduling_user_finished, 1);
}


static void scheduling_system_work_handler(
        void *arg,
        runtime_abort_flag_t *abort_flag) {
    (void)arg;

    assert(abort_flag != NULL);

    atomic_fetch_add(&scheduling_system_started, 1);

    /*
     * Record the priority assigned by the production worker.
     */
    atomic_store(
        &scheduling_system_priority,
        uxTaskPriorityGet(NULL));

    vTaskDelay(pdMS_TO_TICKS(SCHEDULING_WORK_DURATION_MS));

    atomic_fetch_add(&scheduling_system_finished, 1);
}


static void packed_work_handler(void *arg,
                                runtime_abort_flag_t *abort_flag) {
    (void)arg;

    assert(abort_flag != NULL);

    int active =
        atomic_fetch_add(&packed_work_active, 1) + 1;

    atomic_fetch_add(&packed_work_started, 1);

    int observed_max =
        atomic_load(&packed_work_max_active);

    while (active > observed_max) {
        if (atomic_compare_exchange_weak(
                &packed_work_max_active,
                &observed_max,
                active)) {
            break;
        }
    }

    vTaskDelay(pdMS_TO_TICKS(PACKED_WORK_DURATION_MS));

    atomic_fetch_add(&packed_work_finished, 1);
    atomic_fetch_sub(&packed_work_active, 1);
}


static void system_packed_work_handler(void *arg,
                                       runtime_abort_flag_t *abort_flag) {
    (void)arg;

    assert(abort_flag != NULL);

    int active =
        atomic_fetch_add(&system_packed_work_active, 1) + 1;

    atomic_fetch_add(&system_packed_work_started, 1);

    int observed_max =
        atomic_load(&system_packed_work_max_active);

    while (active > observed_max) {
        if (atomic_compare_exchange_weak(
                &system_packed_work_max_active,
                &observed_max,
                active)) {
            break;
        }
    }

    vTaskDelay(pdMS_TO_TICKS(SYSTEM_PACKED_WORK_DURATION_MS));

    atomic_fetch_add(&system_packed_work_finished, 1);

    atomic_fetch_sub(&system_packed_work_active, 1);
}


static void system_grace_abort_work_handler(
        void *arg,
        runtime_abort_flag_t *abort_flag) {
    (void)arg;

    assert(abort_flag != NULL);

    int active =
        atomic_fetch_add(&system_grace_abort_active, 1) + 1;

    atomic_fetch_add(&system_grace_abort_started, 1);

    (void)active;

    vTaskDelay(
        pdMS_TO_TICKS(
            SYSTEM_GRACE_ABORT_WORK_DURATION_MS));

    atomic_fetch_add(&system_grace_abort_finished, 1);

    atomic_fetch_sub(&system_grace_abort_active, 1);
}


static void deinit_user_work_handler(void *arg,
                                     runtime_abort_flag_t *abort_flag) {
    (void)arg;

    assert(abort_flag != NULL);

    atomic_fetch_add(&deinit_user_started, 1);

    vTaskDelay(pdMS_TO_TICKS(DEINIT_WORK_DURATION_MS));

    atomic_fetch_add(&deinit_user_finished, 1);
}


static void deinit_system_work_handler(void *arg,
                                       runtime_abort_flag_t *abort_flag) {
    (void)arg;

    assert(abort_flag != NULL);

    atomic_fetch_add(&deinit_system_started, 1);

    vTaskDelay(pdMS_TO_TICKS(DEINIT_WORK_DURATION_MS));

    atomic_fetch_add(&deinit_system_finished, 1);
}


static void test_hook_a(void) {
    atomic_fetch_add(&hook_a_count, 1);
}


static void test_hook_b(void) {
    atomic_fetch_add(&hook_b_count, 1);
}


static void test_hook_c(void) {
    atomic_fetch_add(&hook_c_count, 1);
}


static void reset_scheduling_counters(void) {
    atomic_store(&scheduling_user_started, 0);
    atomic_store(&scheduling_user_finished, 0);
    atomic_store(&scheduling_system_started, 0);
    atomic_store(&scheduling_system_finished, 0);

    /*
     * -1 means no worker has recorded its priority yet.
     */
    atomic_store(&scheduling_user_priority, -1);
    atomic_store(&scheduling_system_priority, -1);
}


static void test_work_tick_observer(const event_t *event) {
    assert(event != NULL);
    assert(event->ev == EVENT_WORK_TICK);

    atomic_fetch_add(&test_work_tick_count, 1);
}


static void wait_for_atomic_bool(atomic_bool *value,
                                 uint32_t timeout_ms,
                                 const char *name) {
    uint32_t elapsed_ms = 0;

    while (!atomic_load(value)) {
        vTaskDelay(pdMS_TO_TICKS(1));
        elapsed_ms++;

        if (elapsed_ms >= timeout_ms) {
            fprintf(stderr,
                    "[FAIL] timeout waiting for %s\n",
                    name);
            assert(false);
        }
    }
}


static void wait_for_runtime_state(runtime_state_t state,
                                   uint32_t timeout_ms,
                                   const char *name) {
    uint32_t elapsed_ms = 0;

    while (runtime_get_state() != state) {
        vTaskDelay(pdMS_TO_TICKS(10));
        elapsed_ms += 10;

        if (elapsed_ms >= timeout_ms) {
            fprintf(stderr,
                    "[FAIL] timeout waiting for runtime state %s\n",
                    name);
            assert(false);
        }
    }
}


static void wait_for_packed_work_finished(int target,
                                          uint32_t timeout_ms,
                                          const char *name) {
    uint32_t elapsed_ms = 0;

    while (atomic_load(&packed_work_finished) < target) {
        vTaskDelay(pdMS_TO_TICKS(1));
        elapsed_ms++;

        if (elapsed_ms >= timeout_ms) {
            fprintf(stderr,
                    "[FAIL] timeout waiting for %s: finished=%d target=%d\n",
                    name,
                    atomic_load(&packed_work_finished),
                    target);
            assert(false);
        }
    }
}


static void wait_for_system_packed_work_finished(
        int target,
        uint32_t timeout_ms,
        const char *name) {
    uint32_t elapsed_ms = 0;

    while (atomic_load(&system_packed_work_finished) < target) {
        vTaskDelay(pdMS_TO_TICKS(1));
        elapsed_ms++;

        if (elapsed_ms >= timeout_ms) {
            fprintf(stderr,
                    "[FAIL] timeout waiting for %s: finished=%d target=%d\n",
                    name,
                    atomic_load(&system_packed_work_finished),
                    target);
            assert(false);
        }
    }
}


static void wait_for_work_ticks(int target,
                                uint32_t timeout_ms,
                                const char *name) {
    uint32_t elapsed_ms = 0;

    while (atomic_load(&test_work_tick_count) < target) {
        vTaskDelay(pdMS_TO_TICKS(10));
        elapsed_ms += 10;

        if (elapsed_ms >= timeout_ms) {
            fprintf(stderr,
                    "[FAIL] timeout waiting for %s: ticks=%d target=%d\n",
                    name,
                    atomic_load(&test_work_tick_count),
                    target);
            assert(false);
        }
    }
}


static void wait_for_system_grace_abort_started(
        int target,
        uint32_t timeout_ms,
        const char *name) {
    uint32_t elapsed_ms = 0;

    while (atomic_load(&system_grace_abort_started) < target) {
        vTaskDelay(pdMS_TO_TICKS(1));
        elapsed_ms++;

        if (elapsed_ms >= timeout_ms) {
            fprintf(stderr,
                    "[FAIL] timeout waiting for %s: started=%d target=%d\n",
                    name,
                    atomic_load(&system_grace_abort_started),
                    target);
            assert(false);
        }
    }
}


static void wait_for_deinit_work_started(
        uint32_t timeout_ms,
        const char *name) {
    uint32_t elapsed_ms = 0;

    while (atomic_load(&deinit_user_started) == 0 ||
           atomic_load(&deinit_system_started) == 0) {

        vTaskDelay(pdMS_TO_TICKS(1));
        elapsed_ms++;

        if (elapsed_ms >= timeout_ms) {
            fprintf(stderr,
                    "[FAIL] timeout waiting for %s: "
                    "user_started=%d system_started=%d\n",
                    name,
                    atomic_load(&deinit_user_started),
                    atomic_load(&deinit_system_started));
            assert(false);
        }
    }
}


static void reset_packed_work_counters(void) {
    atomic_store(&packed_work_started, 0);
    atomic_store(&packed_work_finished, 0);
    atomic_store(&packed_work_active, 0);
    atomic_store(&packed_work_max_active, 0);
}


static void reset_system_packed_work_counters(void) {
    atomic_store(&system_packed_work_started, 0);
    atomic_store(&system_packed_work_finished, 0);
    atomic_store(&system_packed_work_active, 0);
    atomic_store(&system_packed_work_max_active, 0);
}


static void reset_system_grace_abort_counters(void) {
    atomic_store(&system_grace_abort_started, 0);
    atomic_store(&system_grace_abort_finished, 0);
    atomic_store(&system_grace_abort_active, 0);
}


static void reset_deinit_work_counters(void) {
    atomic_store(&deinit_user_started, 0);
    atomic_store(&deinit_user_finished, 0);
    atomic_store(&deinit_system_started, 0);
    atomic_store(&deinit_system_finished, 0);
}


/*
 * Test: first tick activates UI.
 *
 * Verifies the first work tick transitions the runtime from sleep to
 * UI active and accepted work is drained during deinitialization.
 */
static void test_first_tick_activates_ui(void) {
    atomic_store(&work_started, false);
    atomic_store(&work_finished, false);

    runtime_manager_init();

    assert(runtime_get_state() == RUNTIME_STATE_SLEEP);

    wait_for_work_ticks(
        atomic_load(&test_work_tick_count) + 1,
        TICK_TIMEOUT_MS,
        "first work tick");

    assert(runtime_get_state() == RUNTIME_STATE_UI_ACTIVE);

    runtime_work_item_t work = {
        .handler = test_work_handler,
        .type = WORK_TYPE_USER,
        .priority = RUNTIME_USER_PRIORITY,
    };

    assert(schedule_user_work(&work));

    wait_for_atomic_bool(
        &work_started,
        WORK_START_TIMEOUT_MS,
        "test work start");

    runtime_manager_deinit();

    assert(atomic_load(&work_finished));
}


/*
 * Test: second tick activates background.
 *
 * Verifies an inactive-state request followed by a work tick enters
 * background active state.
 */
static void test_second_tick_activates_background(void) {
    runtime_manager_init();

    assert(runtime_get_state() == RUNTIME_STATE_SLEEP);

    runtime_manager_set_active_state(false);

    wait_for_work_ticks(
        atomic_load(&test_work_tick_count) + 1,
        TICK_TIMEOUT_MS,
        "background work tick");

    assert(runtime_get_state() == RUNTIME_STATE_BACKGROUND_ACTIVE);

    runtime_manager_deinit();
}


/*
 * Test: window expiry returns to sleep.
 *
 * Verifies an active UI window eventually passes through grace and
 * returns the runtime to sleep.
 */
static void test_window_expiry_returns_to_sleep(void) {
    runtime_manager_init();

    assert(runtime_get_state() == RUNTIME_STATE_SLEEP);

    runtime_manager_set_active_state(true);

    wait_for_work_ticks(
        atomic_load(&test_work_tick_count) + 1,
        TICK_TIMEOUT_MS,
        "UI work tick");

    assert(runtime_get_state() == RUNTIME_STATE_UI_ACTIVE);

    wait_for_runtime_state(
        RUNTIME_STATE_SLEEP,
        WINDOW_UI_MAX_MS + WINDOW_GRACE_PERIOD_MS + 300,
        "window expiry");

    runtime_manager_deinit();
}


/*
 * Test: schedule user and system work.
 *
 * Verifies both worker classes execute work submitted during an active
 * UI window.
 */
static void test_schedule_user_and_system_work(void) {
    atomic_store(&user_work_finished, false);
    atomic_store(&system_work_finished, false);

    runtime_manager_init();

    runtime_manager_set_active_state(true);

    wait_for_runtime_state(
        RUNTIME_STATE_UI_ACTIVE,
        TICK_TIMEOUT_MS,
        "UI_ACTIVE for user/system work");

    runtime_work_item_t user_work = {
        .handler = test_user_work_handler,
        .type = WORK_TYPE_USER,
        .priority = RUNTIME_USER_PRIORITY,
    };

    runtime_work_item_t system_work = {
        .handler = test_system_work_handler,
        .type = WORK_TYPE_SYSTEM,
        .priority = RUNTIME_SYSTEM_PRIORITY,
    };

    assert(schedule_user_work(&user_work));
    assert(schedule_system_work(&system_work));

    wait_for_atomic_bool(
        &user_work_finished,
        WORK_START_TIMEOUT_MS,
        "user work");

    wait_for_atomic_bool(
        &system_work_finished,
        WORK_START_TIMEOUT_MS,
        "system work");

    runtime_manager_deinit();
}


/*
 * Test: curfew hook executes before sleep.
 *
 * Verifies a registered curfew hook runs during the transition out of
 * the active window before the runtime reaches sleep.
 */
static void test_curfew_hook_executes_before_sleep(void) {
    atomic_store(&curfew_hook_finished, false);

    runtime_manager_init();

    assert(runtime_manager_register_hook(test_curfew_hook));

    runtime_manager_set_active_state(true);

    wait_for_runtime_state(
        RUNTIME_STATE_UI_ACTIVE,
        TICK_TIMEOUT_MS,
        "UI_ACTIVE for curfew hook");

    assert(!atomic_load(&curfew_hook_finished));

    wait_for_atomic_bool(
        &curfew_hook_finished,
        WINDOW_UI_MAX_MS + WINDOW_GRACE_PERIOD_MS + 300,
        "curfew hook");

    assert(runtime_get_state() != RUNTIME_STATE_UI_ACTIVE);

    wait_for_runtime_state(
        RUNTIME_STATE_SLEEP,
        WINDOW_GRACE_PERIOD_MS + 300,
        "sleep after curfew hook");

    runtime_manager_deinit();
}


/*
 * Test: work scheduled during grace is deferred.
 *
 * Verifies work submitted by a curfew hook remains pending through
 * grace and sleep, then executes in the next active window.
 */
static void test_work_scheduled_during_grace_is_deferred(void) {
    atomic_store(&curfew_hook_finished, false);
    atomic_store(&grace_user_work_finished, false);
    atomic_store(&grace_system_work_finished, false);

    runtime_manager_init();

    assert(runtime_manager_register_hook(
        test_curfew_hook_schedules_work));

    runtime_manager_set_active_state(true);

    wait_for_runtime_state(
        RUNTIME_STATE_UI_ACTIVE,
        TICK_TIMEOUT_MS,
        "UI_ACTIVE before grace");

    wait_for_runtime_state(
        RUNTIME_STATE_GRACE_PERIOD,
        WINDOW_UI_MAX_MS + 300,
        "GRACE_PERIOD");

    wait_for_atomic_bool(
        &curfew_hook_finished,
        WINDOW_GRACE_PERIOD_MS + 300,
        "curfew hook scheduling work");

    vTaskDelay(pdMS_TO_TICKS(20));

    assert(!atomic_load(&grace_user_work_finished));
    assert(!atomic_load(&grace_system_work_finished));

    wait_for_runtime_state(
        RUNTIME_STATE_SLEEP,
        WINDOW_GRACE_PERIOD_MS + 300,
        "sleep after grace");

    assert(!atomic_load(&grace_user_work_finished));
    assert(!atomic_load(&grace_system_work_finished));

    runtime_manager_set_active_state(true);

    wait_for_runtime_state(
        RUNTIME_STATE_UI_ACTIVE,
        TICK_TIMEOUT_MS,
        "next UI_ACTIVE for pending work");

    wait_for_atomic_bool(
        &grace_user_work_finished,
        WORK_START_TIMEOUT_MS,
        "deferred user work");

    wait_for_atomic_bool(
        &grace_system_work_finished,
        WORK_START_TIMEOUT_MS,
        "deferred system work");

    runtime_manager_deinit();
}


/*
 * Test: user work packing.
 *
 * Verifies user work is limited per window and processed concurrently
 * by the available user workers.
 */
static void test_user_work_packing(void) {
    reset_packed_work_counters();

    runtime_manager_init();

    assert(runtime_get_state() == RUNTIME_STATE_SLEEP);

    runtime_manager_set_active_state(true);

    wait_for_runtime_state(
        RUNTIME_STATE_UI_ACTIVE,
        TICK_TIMEOUT_MS,
        "UI_ACTIVE for packing");

    reset_packed_work_counters();

    runtime_work_item_t work = {
        .handler = packed_work_handler,
        .type = WORK_TYPE_USER,
        .priority = RUNTIME_USER_PRIORITY,
    };

    for (int i = 0; i < MAX_USER_WORK_PER_WINDOW; i++) {
        assert(schedule_user_work(&work));
    }

    assert(!schedule_user_work(&work));

    wait_for_packed_work_finished(
        MAX_USER_WORK_PER_WINDOW,
        WINDOW_UI_MAX_MS + WINDOW_GRACE_PERIOD_MS + 300,
        "first 10 UI user work");

    assert(atomic_load(&packed_work_started) ==
           MAX_USER_WORK_PER_WINDOW);

    assert(atomic_load(&packed_work_finished) ==
           MAX_USER_WORK_PER_WINDOW);

    assert(atomic_load(&packed_work_max_active) == 2);
    assert(atomic_load(&packed_work_active) == 0);

    wait_for_runtime_state(
        RUNTIME_STATE_SLEEP,
        WINDOW_UI_MAX_MS + WINDOW_GRACE_PERIOD_MS + 300,
        "SLEEP after first UI packing window");

    reset_packed_work_counters();

    runtime_manager_set_active_state(true);

    wait_for_runtime_state(
        RUNTIME_STATE_UI_ACTIVE,
        TICK_TIMEOUT_MS,
        "UI_ACTIVE for second packing window");

    for (int i = 0; i < MAX_USER_WORK_PER_WINDOW; i++) {
        assert(schedule_user_work(&work));
    }

    assert(!schedule_user_work(&work));

    wait_for_packed_work_finished(
        MAX_USER_WORK_PER_WINDOW,
        WINDOW_UI_MAX_MS + WINDOW_GRACE_PERIOD_MS + 300,
        "second 10 UI user work");

    assert(atomic_load(&packed_work_started) ==
           MAX_USER_WORK_PER_WINDOW);

    assert(atomic_load(&packed_work_finished) ==
           MAX_USER_WORK_PER_WINDOW);

    assert(atomic_load(&packed_work_max_active) == 2);
    assert(atomic_load(&packed_work_active) == 0);

    wait_for_runtime_state(
        RUNTIME_STATE_SLEEP,
        WINDOW_UI_MAX_MS + WINDOW_GRACE_PERIOD_MS + 300,
        "SLEEP after second UI packing window");

    reset_packed_work_counters();

    runtime_manager_set_active_state(false);

    wait_for_runtime_state(
        RUNTIME_STATE_BACKGROUND_ACTIVE,
        TICK_TIMEOUT_MS,
        "BACKGROUND_ACTIVE for packing");

    int background_start_tick =
        atomic_load(&test_work_tick_count);

    for (int i = 0; i < MAX_USER_WORK_PER_WINDOW; i++) {
        assert(schedule_user_work(&work));
    }

    assert(!schedule_user_work(&work));

    wait_for_work_ticks(
        background_start_tick + 1,
        WORK_TICK_MS + TICK_TIMEOUT_MS,
        "next background work tick");

    assert(runtime_get_state() ==
           RUNTIME_STATE_BACKGROUND_ACTIVE);

    for (int i = 0; i < MAX_USER_WORK_PER_WINDOW; i++) {
        assert(schedule_user_work(&work));
    }

    assert(!schedule_user_work(&work));

    wait_for_work_ticks(
        background_start_tick + 2,
        (2 * WORK_TICK_MS) + TICK_TIMEOUT_MS,
        "second background work tick");

    vTaskDelay(pdMS_TO_TICKS(50));

    int background_started =
        atomic_load(&packed_work_started);

    int background_finished =
        atomic_load(&packed_work_finished);

    int background_max_active =
        atomic_load(&packed_work_max_active);

    assert(background_max_active <= 2);
    assert(background_finished >= 18);
    assert(background_finished <= 20);
    assert(background_started >= background_finished);

    printf("[PASS] user work packed across UI/background windows: "
           "UI=20/20, BG=%d/20 started=%d max_concurrent=%d\n",
           background_finished,
           background_started,
           background_max_active);

    runtime_manager_deinit();
}


/*
 * Test: system work packing.
 *
 * Verifies system work is limited per window and processed concurrently
 * by the available system workers.
 */
static void test_system_work_packing(void) {
    runtime_work_item_t work = {
        .handler = system_packed_work_handler,
        .type = WORK_TYPE_SYSTEM,
        .priority = RUNTIME_SYSTEM_PRIORITY,
    };

    reset_system_packed_work_counters();

    runtime_manager_init();

    assert(runtime_get_state() == RUNTIME_STATE_SLEEP);

    runtime_manager_set_active_state(true);

    wait_for_runtime_state(
        RUNTIME_STATE_UI_ACTIVE,
        TICK_TIMEOUT_MS,
        "UI_ACTIVE for system packing");

    for (int i = 0; i < MAX_SYSTEM_WORK_PER_WINDOW; i++) {
        assert(schedule_system_work(&work));
    }

    assert(!schedule_system_work(&work));

    wait_for_system_packed_work_finished(
        MAX_SYSTEM_WORK_PER_WINDOW,
        WINDOW_UI_MAX_MS + WINDOW_GRACE_PERIOD_MS + 300,
        "first 10 UI system work");

    assert(atomic_load(&system_packed_work_started) ==
           MAX_SYSTEM_WORK_PER_WINDOW);

    assert(atomic_load(&system_packed_work_finished) ==
           MAX_SYSTEM_WORK_PER_WINDOW);

    assert(atomic_load(&system_packed_work_max_active) == 4);
    assert(atomic_load(&system_packed_work_active) == 0);

    wait_for_runtime_state(
        RUNTIME_STATE_SLEEP,
        WINDOW_UI_MAX_MS + WINDOW_GRACE_PERIOD_MS + 300,
        "SLEEP after first UI system packing window");

    reset_system_packed_work_counters();

    runtime_manager_set_active_state(true);

    wait_for_runtime_state(
        RUNTIME_STATE_UI_ACTIVE,
        TICK_TIMEOUT_MS,
        "UI_ACTIVE for second system packing window");

    for (int i = 0;
         i < SYSTEM_PACKED_WORK_ITEM_COUNT -
             MAX_SYSTEM_WORK_PER_WINDOW;
         i++) {
        assert(schedule_system_work(&work));
    }

    wait_for_system_packed_work_finished(
        SYSTEM_PACKED_WORK_ITEM_COUNT -
        MAX_SYSTEM_WORK_PER_WINDOW,
        WINDOW_UI_MAX_MS + WINDOW_GRACE_PERIOD_MS + 300,
        "remaining UI system work");

    assert(atomic_load(&system_packed_work_finished) ==
           SYSTEM_PACKED_WORK_ITEM_COUNT -
           MAX_SYSTEM_WORK_PER_WINDOW);

    assert(atomic_load(&system_packed_work_max_active) <= 4);
    assert(atomic_load(&system_packed_work_active) == 0);

    wait_for_runtime_state(
        RUNTIME_STATE_SLEEP,
        WINDOW_UI_MAX_MS + WINDOW_GRACE_PERIOD_MS + 300,
        "SLEEP after second UI system packing window");

    reset_system_packed_work_counters();

    runtime_manager_set_active_state(false);

    wait_for_runtime_state(
        RUNTIME_STATE_BACKGROUND_ACTIVE,
        TICK_TIMEOUT_MS,
        "BACKGROUND_ACTIVE for system packing");

    int background_start_tick =
        atomic_load(&test_work_tick_count);

    for (int i = 0; i < MAX_SYSTEM_WORK_PER_WINDOW; i++) {
        assert(schedule_system_work(&work));
    }

    assert(!schedule_system_work(&work));

    wait_for_work_ticks(
        background_start_tick + 1,
        WORK_TICK_MS + TICK_TIMEOUT_MS,
        "next system background work tick");

    assert(runtime_get_state() ==
           RUNTIME_STATE_BACKGROUND_ACTIVE);

    for (int i = 0;
         i < SYSTEM_PACKED_WORK_ITEM_COUNT -
             MAX_SYSTEM_WORK_PER_WINDOW;
         i++) {
        assert(schedule_system_work(&work));
    }

    int completion_tick =
        background_start_tick + 1;

    while (atomic_load(&system_packed_work_finished) <
           SYSTEM_PACKED_WORK_ITEM_COUNT) {

        wait_for_work_ticks(
            completion_tick + 1,
            WORK_TICK_MS + TICK_TIMEOUT_MS,
            "system work completion tick");

        completion_tick++;

        if (atomic_load(&system_packed_work_finished) >=
            SYSTEM_PACKED_WORK_ITEM_COUNT) {
            break;
        }
    }

    int finished =
        atomic_load(&system_packed_work_finished);

    int started =
        atomic_load(&system_packed_work_started);

    int max_active =
        atomic_load(&system_packed_work_max_active);

    int ticks_used =
        completion_tick - background_start_tick;

    assert(finished == SYSTEM_PACKED_WORK_ITEM_COUNT);
    assert(started == SYSTEM_PACKED_WORK_ITEM_COUNT);
    assert(max_active == 4);
    assert(atomic_load(&system_packed_work_active) == 0);

    printf("[PASS] system work packed across windows: "
           "UI=12/12, BG=12/12 ticks=%d started=%d "
           "max_concurrent=%d\n",
           ticks_used,
           started,
           max_active);

    runtime_manager_deinit();
}


/*
 * Test: system work is terminated at grace expiry.
 *
 * Verifies long-running system work is forcibly stopped when the grace
 * period expires instead of continuing indefinitely.
 */
static void test_system_work_terminated_at_grace_expiry(void) {
    reset_system_grace_abort_counters();

    runtime_manager_init();

    assert(runtime_get_state() == RUNTIME_STATE_SLEEP);

    runtime_manager_set_active_state(true);

    wait_for_runtime_state(
        RUNTIME_STATE_UI_ACTIVE,
        TICK_TIMEOUT_MS,
        "UI_ACTIVE for grace expiry system-work test");

    runtime_work_item_t work = {
        .handler = system_grace_abort_work_handler,
        .type = WORK_TYPE_SYSTEM,
        .priority = RUNTIME_SYSTEM_PRIORITY,
    };

    for (int i = 0;
         i < SYSTEM_GRACE_ABORT_WORKER_COUNT;
         i++) {
        assert(schedule_system_work(&work));
    }

    /*
     * All four system workers must be executing before grace expires.
     */
    wait_for_system_grace_abort_started(
        SYSTEM_GRACE_ABORT_WORKER_COUNT,
        WORK_START_TIMEOUT_MS,
        "all long-running system workers");

    assert(atomic_load(&system_grace_abort_active) ==
           SYSTEM_GRACE_ABORT_WORKER_COUNT);

    assert(atomic_load(&system_grace_abort_finished) == 0);

    wait_for_runtime_state(
        RUNTIME_STATE_SLEEP,
        WINDOW_UI_MAX_MS + WINDOW_GRACE_PERIOD_MS + 500,
        "sleep after grace expiry");

    assert(atomic_load(&system_grace_abort_finished) == 0);

    assert(atomic_load(&system_grace_abort_started) ==
           SYSTEM_GRACE_ABORT_WORKER_COUNT);

    printf("[PASS] system work terminated at grace expiry: "
           "started=%d finished=%d active=%d\n",
           atomic_load(&system_grace_abort_started),
           atomic_load(&system_grace_abort_finished),
           atomic_load(&system_grace_abort_active));

    runtime_manager_deinit();
}


/*
 * Test: curfew hook registration lifecycle.
 *
 * Verifies hooks can be registered, removed, compacted, and registered
 * again after previous hooks are removed.
 */
static void test_curfew_hook_registration_lifecycle(void) {
    atomic_store(&hook_a_count, 0);
    atomic_store(&hook_b_count, 0);
    atomic_store(&hook_c_count, 0);

    runtime_manager_init();

    assert(!runtime_manager_unregister_hook(NULL));
    assert(!runtime_manager_unregister_hook(test_hook_a));
    assert(runtime_manager_register_hook(test_hook_a));
    assert(runtime_manager_register_hook(test_hook_b));
    assert(runtime_manager_register_hook(test_hook_c));

    /*
     * Removing the middle hook exercises hook-array compaction.
     */
    assert(runtime_manager_unregister_hook(test_hook_b));
    assert(!runtime_manager_unregister_hook(test_hook_b));
    assert(runtime_manager_unregister_hook(test_hook_a));
    assert(runtime_manager_unregister_hook(test_hook_c));
    assert(!runtime_manager_unregister_hook(test_hook_a));
    assert(!runtime_manager_unregister_hook(test_hook_c));
    assert(runtime_manager_register_hook(test_hook_b));
    assert(runtime_manager_unregister_hook(test_hook_b));

    runtime_manager_deinit();

    assert(!runtime_manager_unregister_hook(test_hook_a));

    printf("[PASS] curfew hook registration/unregistration lifecycle\n");
}


/*
 * Test: invalid and post-deinit work submission.
 *
 * Verifies invalid work is rejected and no work can be submitted after
 * the runtime manager has been deinitialized.
 */
static void test_work_rejected_after_deinit(void) {
    atomic_store(&user_work_finished, false);
    atomic_store(&system_work_finished, false);

    runtime_manager_init();

    runtime_work_item_t user_work = {
        .handler = test_user_work_handler,
        .type = WORK_TYPE_USER,
        .priority = RUNTIME_USER_PRIORITY,
    };

    runtime_work_item_t system_work = {
        .handler = test_system_work_handler,
        .type = WORK_TYPE_SYSTEM,
        .priority = RUNTIME_SYSTEM_PRIORITY,
    };

    assert(!schedule_user_work(NULL));
    assert(!schedule_system_work(NULL));

    runtime_work_item_t invalid_work = {
        .handler = NULL,
        .type = WORK_TYPE_USER,
        .priority = RUNTIME_USER_PRIORITY,
    };

    assert(!schedule_user_work(&invalid_work));
    assert(!schedule_system_work(&invalid_work));

    runtime_manager_deinit();

    assert(!schedule_user_work(&user_work));
    assert(!schedule_system_work(&system_work));

    assert(!schedule_user_work(NULL));
    assert(!schedule_system_work(NULL));

    vTaskDelay(pdMS_TO_TICKS(20));

    assert(!atomic_load(&user_work_finished));
    assert(!atomic_load(&system_work_finished));

    printf("[PASS] invalid and post-deinit work submission rejected\n");
}


/*
 * Test: deinit drains existing work.
 *
 * Verifies work accepted before shutdown is fully drained before
 * runtime_manager_deinit() returns.
 */
static void test_deinit_drains_existing_work(void) {
    reset_deinit_work_counters();

    runtime_manager_init();

    runtime_manager_set_active_state(true);

    wait_for_runtime_state(
        RUNTIME_STATE_UI_ACTIVE,
        TICK_TIMEOUT_MS,
        "UI_ACTIVE for deinit drain");

    runtime_work_item_t user_work = {
        .handler = deinit_user_work_handler,
        .type = WORK_TYPE_USER,
        .priority = RUNTIME_USER_PRIORITY,
    };

    runtime_work_item_t system_work = {
        .handler = deinit_system_work_handler,
        .type = WORK_TYPE_SYSTEM,
        .priority = RUNTIME_SYSTEM_PRIORITY,
    };

    for (int i = 0; i < DEINIT_EXISTING_WORK_COUNT; i++) {
        assert(schedule_user_work(&user_work));
        assert(schedule_system_work(&system_work));
    }

    /*
     * Ensure both worker classes are active before shutdown begins.
     */
    wait_for_deinit_work_started(
        WORK_START_TIMEOUT_MS,
        "deinit work");

    runtime_manager_deinit();

    assert(atomic_load(&deinit_user_started) ==
           DEINIT_EXISTING_WORK_COUNT);

    assert(atomic_load(&deinit_user_finished) ==
           DEINIT_EXISTING_WORK_COUNT);

    assert(atomic_load(&deinit_system_started) ==
           DEINIT_EXISTING_WORK_COUNT);

    assert(atomic_load(&deinit_system_finished) ==
           DEINIT_EXISTING_WORK_COUNT);

    printf("[PASS] deinit drains existing work: "
           "user=%d/%d system=%d/%d\n",
           atomic_load(&deinit_user_finished),
           DEINIT_EXISTING_WORK_COUNT,
           atomic_load(&deinit_system_finished),
           DEINIT_EXISTING_WORK_COUNT);
}


/*
 * Test: repeated active-state requests.
 *
 * Verifies repeated requests for the same state do not break the
 * runtime state machine.
 */
static void test_repeated_active_state_requests(void) {
    runtime_manager_init();

    assert(runtime_get_state() == RUNTIME_STATE_SLEEP);

    runtime_manager_set_active_state(true);
    runtime_manager_set_active_state(true);
    runtime_manager_set_active_state(true);

    wait_for_runtime_state(
        RUNTIME_STATE_UI_ACTIVE,
        TICK_TIMEOUT_MS,
        "UI_ACTIVE after repeated active requests");

    runtime_manager_set_active_state(false);
    runtime_manager_set_active_state(false);
    runtime_manager_set_active_state(false);

    wait_for_runtime_state(
        RUNTIME_STATE_BACKGROUND_ACTIVE,
        TICK_TIMEOUT_MS,
        "BACKGROUND_ACTIVE after repeated inactive requests");

    runtime_manager_deinit();

    printf("[PASS] repeated active-state requests are idempotent\n");
}


/*
 * Test: worker priority follows work type.
 *
 * Verifies user and system workers execute at their configured
 * priorities rather than the baseline worker priority.
 */
static void test_worker_priority_follows_work_type(void) {
    reset_scheduling_counters();

    runtime_manager_init();

    runtime_manager_set_active_state(true);

    wait_for_runtime_state(
        RUNTIME_STATE_UI_ACTIVE,
        TICK_TIMEOUT_MS,
        "UI_ACTIVE for worker-priority test");

    runtime_work_item_t user_work = {
        .handler = scheduling_user_work_handler,
        .type = WORK_TYPE_USER,
        .priority = RUNTIME_USER_PRIORITY,
    };

    runtime_work_item_t system_work = {
        .handler = scheduling_system_work_handler,
        .type = WORK_TYPE_SYSTEM,
        .priority = RUNTIME_SYSTEM_PRIORITY,
    };

    for (int i = 0; i < WORKER_POOL_USER_ALLOCATION; i++) {
        assert(schedule_user_work(&user_work));
    }

    for (int i = 0; i < WORKER_POOL_SYSTEM_ALLOCATION; i++) {
        assert(schedule_system_work(&system_work));
    }

    /*
     * Start/finish ordering is not part of the production contract.
     */
    uint32_t elapsed_ms = 0;

    while (atomic_load(&scheduling_user_finished) <
               WORKER_POOL_USER_ALLOCATION ||
           atomic_load(&scheduling_system_finished) <
               WORKER_POOL_SYSTEM_ALLOCATION) {

        vTaskDelay(pdMS_TO_TICKS(1));
        elapsed_ms++;

        if (elapsed_ms >= 1000) {
            fprintf(stderr,
                    "[FAIL] timeout waiting for worker-priority test: "
                    "user=%d/%d system=%d/%d\n",
                    atomic_load(&scheduling_user_finished),
                    WORKER_POOL_USER_ALLOCATION,
                    atomic_load(&scheduling_system_finished),
                    WORKER_POOL_SYSTEM_ALLOCATION);
            assert(false);
        }
    }

    assert(atomic_load(&scheduling_user_started) ==
           WORKER_POOL_USER_ALLOCATION);
    assert(atomic_load(&scheduling_user_finished) ==
           WORKER_POOL_USER_ALLOCATION);
    assert(atomic_load(&scheduling_system_started) ==
           WORKER_POOL_SYSTEM_ALLOCATION);
    assert(atomic_load(&scheduling_system_finished) ==
           WORKER_POOL_SYSTEM_ALLOCATION);
    assert(atomic_load(&scheduling_user_priority) ==
           RUNTIME_USER_PRIORITY);
    assert(atomic_load(&scheduling_system_priority) ==
           RUNTIME_SYSTEM_PRIORITY);
    assert(RUNTIME_SYSTEM_PRIORITY > RUNTIME_USER_PRIORITY);
    assert(RUNTIME_USER_PRIORITY > RUNTIME_BASELINE_PRIORITY);

    printf("[PASS] worker priority follows work type: "
           "system_priority=%d user_priority=%d baseline_priority=%d\n",
           atomic_load(&scheduling_system_priority),
           atomic_load(&scheduling_user_priority),
           RUNTIME_BASELINE_PRIORITY);

    runtime_manager_deinit();
}

/*
 * Test: Deinit timeout and abrupt task deletion lock leak.
 *
 * Exposes the danger of vTaskDelete during runtime_manager_deinit.
 * When deinit times out waiting for workers, it deletes the worker tasks abruptly,
 * leaving any application-level locks held by the worker permanently locked.
 */
static void test_deinit_timeout_task_deletion_leak(void) {
    printf("test_deinit_timeout_task_deletion_leak...\n");

    test_leak_sem = xSemaphoreCreateBinary();
    xSemaphoreGive(test_leak_sem);
    atomic_store(&work_started, false);
    atomic_store(&work_finished, false);

    runtime_manager_init();
    runtime_manager_set_active_state(true);

    wait_for_runtime_state(RUNTIME_STATE_UI_ACTIVE, TICK_TIMEOUT_MS, "UI_ACTIVE");

    runtime_work_item_t work = {
        .handler = leaking_orphaned_worker_handler,
        .type = WORK_TYPE_USER,
        .priority = RUNTIME_USER_PRIORITY,
    };

    assert(schedule_user_work(&work));

    wait_for_atomic_bool(&work_started, WORK_START_TIMEOUT_MS, "leaking work to start");

    vTaskDelay(pdMS_TO_TICKS(50));

    runtime_manager_deinit();

    assert(!atomic_load(&work_finished));

    // Try to take the application sem. It should fail because the worker was killed
    // while holding it, proving the resource leak edge case.
    assert(xSemaphoreTake(test_leak_sem, pdMS_TO_TICKS(100)) == pdFALSE);

    vSemaphoreDelete(test_leak_sem);
    printf("  PASS (Resource leak proven on timeout)\n");
}

/*
 * Test: Grace period queue capacity and overflow safety.
 *
 * Verifies that when the runtime is in GRACE_PERIOD, attempting to schedule
 * more items than the pending_user_work queue can handle (MAX/2) safely
 * rejects the overflow without crashing or corrupting the main queues.
 */
static void test_grace_period_queue_overflow(void) {
    printf("test_grace_period_queue_overflow...\n");
    atomic_store(&curfew_hook_finished, false);

    runtime_manager_init();
    assert(runtime_manager_register_hook(queue_flooding_curfew_hook));

    runtime_manager_set_active_state(true);

    wait_for_runtime_state(RUNTIME_STATE_GRACE_PERIOD, WINDOW_UI_MAX_MS + 300, "GRACE_PERIOD");
    wait_for_atomic_bool(&curfew_hook_finished, WINDOW_GRACE_PERIOD_MS + 300, "hook flooding queue");

    wait_for_runtime_state(RUNTIME_STATE_SLEEP, WINDOW_GRACE_PERIOD_MS + 300, "SLEEP");

    // Reactivate to trigger drain_pending_queue
    runtime_manager_set_active_state(true);
    wait_for_runtime_state(RUNTIME_STATE_UI_ACTIVE, TICK_TIMEOUT_MS, "UI_ACTIVE for drain");

    // The workers will now drain and execute exactly MAX/2 items.
    vTaskDelay(pdMS_TO_TICKS(100));
    runtime_manager_deinit();

    printf("  PASS\n");
}

static void host_test_task(void *arg) {
    (void)arg;

    assert(event_manager_init());
    assert(event_subscribe(
        EVENT_WORK_TICK,
        test_work_tick_observer));

    tick_manager_init();

    tick_manager_generate_tick(TICK_WORK);

    test_first_tick_activates_ui();
    test_second_tick_activates_background();
    test_window_expiry_returns_to_sleep();
    test_schedule_user_and_system_work();
    test_curfew_hook_executes_before_sleep();
    test_work_scheduled_during_grace_is_deferred();
    test_user_work_packing();
    test_system_work_packing();
    test_system_work_terminated_at_grace_expiry();
    test_curfew_hook_registration_lifecycle();
    test_work_rejected_after_deinit();
    test_deinit_drains_existing_work();
    test_repeated_active_state_requests();
    test_worker_priority_follows_work_type();
    test_grace_period_queue_overflow();
    test_deinit_timeout_task_deletion_leak();

    tick_manager_deinit();

    assert(event_unsubscribe(
        EVENT_WORK_TICK,
        test_work_tick_observer));

    event_manager_deinit();

    vTaskEndScheduler();
    vTaskDelete(NULL);
}


int main(void) {
    xTaskCreate(
        host_test_task,
        "runtime_test",
        4096,
        NULL,
        3,
        NULL
    );

    vTaskStartScheduler();
    return 0;
}
