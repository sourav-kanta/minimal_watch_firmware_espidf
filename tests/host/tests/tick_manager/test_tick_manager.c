#include <assert.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_timer.h>

#include <event_manager.h>
#include <tick_consts.h>
#include <tick_manager.h>
#include <tick_types.h>

#define TIMING_TOLERANCE_PERCENT    20
#define SHIFT_US                    500000U
#define TICKS_PER_PHASE             5
#define MAX_RECORDED_TICKS          16

typedef struct {
    int64_t timestamps[MAX_RECORDED_TICKS];
    atomic_uint count;
} tick_test_state_t;

static tick_test_state_t watchface_state;
static tick_test_state_t work_state;

static void reset_tick_state(tick_test_state_t *state) {
    for(unsigned int i = 0; i < MAX_RECORDED_TICKS; i++) {
        state->timestamps[i] = 0;
    }

    atomic_store(&state->count, 0);
}

static void reset_test_state(void) {
    reset_tick_state(&watchface_state);
    reset_tick_state(&work_state);
}

static void tick_event_handler(const event_t *event) {
    tick_test_state_t *state = NULL;

    if(event->ev == EVENT_WATCHFACE_UPDATE) {
        state = &watchface_state;
    }
    else if(event->ev == EVENT_WORK_TICK) {
        state = &work_state;
    }
    else {
        return;
    }

    unsigned int index = atomic_load(&state->count);

    if(index < MAX_RECORDED_TICKS) {
        state->timestamps[index] = esp_timer_get_time();
        atomic_fetch_add(&state->count, 1);
    }
}

static void setup_test(void) {
    tick_manager_deinit();
    event_manager_deinit();

    reset_test_state();

    assert(event_manager_init());
    assert(event_subscribe(EVENT_WATCHFACE_UPDATE, tick_event_handler));
    assert(event_subscribe(EVENT_WORK_TICK, tick_event_handler));

    tick_manager_init();
}

static void teardown_test(void) {
    tick_manager_deinit();

    assert(event_unsubscribe(EVENT_WATCHFACE_UPDATE, tick_event_handler));
    assert(event_unsubscribe(EVENT_WORK_TICK, tick_event_handler));

    event_manager_deinit();
}

static bool wait_for_count(atomic_uint *count, unsigned int expected, unsigned int timeout_ms) {
    unsigned int elapsed_ms = 0;

    while(elapsed_ms < timeout_ms) {
        if(atomic_load(count) >= expected) {
            return true;
        }

        vTaskDelay(pdMS_TO_TICKS(5));
        elapsed_ms += 5;
    }

    return atomic_load(count) >= expected;
}

static void assert_tick_interval(const tick_test_state_t *state,
                                 unsigned int index,
                                 int64_t expected_us) {
    int64_t actual_us = state->timestamps[index] - state->timestamps[index - 1];
    int64_t error_us = actual_us - expected_us;

    if(error_us < 0) {
        error_us = -error_us;
    }

    int64_t tolerance_us =
        (expected_us * TIMING_TOLERANCE_PERCENT) / 100;

    printf("[TEST] index=%u actual=%lld us expected=%lld us error=%lld us tolerance=%lld us\n",
           index, actual_us, expected_us, error_us, tolerance_us);

    assert(error_us <= tolerance_us);
}

/*
 * Test: public tick-manager API edge cases.
 *
 * Verifies tick intervals are reported correctly, shifting before the
 * first work tick is rejected, and invalid tick types are ignored.
 */
static void test_api_edge_cases(void) {
    printf("[TEST] test_api_edge_cases\n");

    setup_test();

    assert(tick_manager_get_tick_interval_ms(TICK_WATCHFACE) == tick_intervals_ms[TICK_WATCHFACE]);
    assert(tick_manager_get_tick_interval_ms(TICK_WORK) == tick_intervals_ms[TICK_WORK]);
    assert(tick_manager_get_tick_interval_ms(TICK_INVALID) == 0);

    assert(!tick_manager_shift_work_tick(SHIFT_US));

    tick_manager_generate_tick(TICK_INVALID);
    tick_manager_stop_tick(TICK_INVALID);

    teardown_test();
}

/*
 * Test: individual tick start and stop.
 *
 * Verifies the watchface and work timers independently generate events
 * at their configured intervals and stop generating events when stopped.
 */
static void test_individual_start_stop(void) {
    printf("[TEST] test_individual_start_stop\n");

    setup_test();

    tick_manager_generate_tick(TICK_WATCHFACE);
    assert(wait_for_count(&watchface_state.count, 3, 500));

    tick_manager_stop_tick(TICK_WATCHFACE);
    unsigned int watchface_count = atomic_load(&watchface_state.count);

    vTaskDelay(pdMS_TO_TICKS(100));
    assert(atomic_load(&watchface_state.count) == watchface_count);

    tick_manager_generate_tick(TICK_WORK);
    assert(wait_for_count(&work_state.count, 2, 2500));

    tick_manager_stop_tick(TICK_WORK);
    unsigned int work_count = atomic_load(&work_state.count);

    vTaskDelay(pdMS_TO_TICKS(1200));
    assert(atomic_load(&work_state.count) == work_count);

    teardown_test();
}

/*
 * Test: simultaneous watchface and work operation.
 *
 * Verifies both timers can run concurrently and that each timer maintains
 * its own configured cadence while the other timer is active.
 */
static void test_simultaneous_operation(void) {
    printf("[TEST] test_simultaneous_operation\n");

    setup_test();

    tick_manager_generate_tick(TICK_WATCHFACE);
    tick_manager_generate_tick(TICK_WORK);

    assert(wait_for_count(&watchface_state.count, TICKS_PER_PHASE, 1000));
    assert(wait_for_count(&work_state.count, TICKS_PER_PHASE, 6000));

    for(unsigned int i = 1; i < TICKS_PER_PHASE; i++) {
        assert_tick_interval(&watchface_state, i, tick_intervals_ms[TICK_WATCHFACE] * 1000LL);
    }

    for(unsigned int i = 1; i < TICKS_PER_PHASE; i++) {
        assert_tick_interval(&work_state, i, tick_intervals_ms[TICK_WORK] * 1000LL);
    }

    teardown_test();
}

/*
 * Test: work tick shift.
 *
 * Verifies five normal work ticks are generated, the next work tick is
 * shifted by 500 ms, five additional work ticks continue afterward at
 * the normal configured interval, and stopping the timer prevents any
 * further work ticks.
 */
static void test_work_tick_shift(void) {
    printf("[TEST] test_work_tick_shift\n");

    setup_test();

    tick_manager_generate_tick(TICK_WORK);

    assert(wait_for_count(&work_state.count, TICKS_PER_PHASE, 6000));

    int64_t shift_request_us = esp_timer_get_time();
    printf("[TEST] fifth tick timestamp=%lld us\n",
           work_state.timestamps[TICKS_PER_PHASE - 1]);
    printf("[TEST] shift request timestamp=%lld us\n",
           shift_request_us);
    printf("[TEST] time since fifth tick=%lld us\n",
           shift_request_us - work_state.timestamps[TICKS_PER_PHASE - 1]);

    assert(tick_manager_shift_work_tick(SHIFT_US));
    assert(wait_for_count(&work_state.count, TICKS_PER_PHASE * 2, 7000));

    printf("[TEST] sixth tick timestamp=%lld us\n",
           work_state.timestamps[TICKS_PER_PHASE]);
    printf("[TEST] time from shift request to sixth tick=%lld us\n",
           work_state.timestamps[TICKS_PER_PHASE] - shift_request_us);

    int64_t work_interval_us = tick_intervals_ms[TICK_WORK] * 1000LL;
    assert_tick_interval(&work_state, 5, work_interval_us + SHIFT_US);

    for(unsigned int i = 6; i < TICKS_PER_PHASE * 2; i++) {
        assert_tick_interval(&work_state, i, work_interval_us);
    }

    tick_manager_stop_tick(TICK_WORK);
    unsigned int work_count = atomic_load(&work_state.count);

    vTaskDelay(pdMS_TO_TICKS(1200));
    assert(atomic_load(&work_state.count) == work_count);

    teardown_test();
}

/*
 * Test: work tick shift with watchface operation.
 *
 * Verifies shifting the work timer by 500 ms does not alter the watchface
 * timer cadence, five additional work ticks are generated afterward, and
 * stopping both timers prevents any further ticks.
 */
static void test_work_tick_shift_with_watchface(void) {
    printf("[TEST] test_work_tick_shift_with_watchface\n");

    setup_test();

    tick_manager_generate_tick(TICK_WATCHFACE);
    tick_manager_generate_tick(TICK_WORK);

    assert(wait_for_count(&work_state.count, TICKS_PER_PHASE, 6000));
    assert(wait_for_count(&watchface_state.count, TICKS_PER_PHASE, 1000));

    reset_tick_state(&watchface_state);

    assert(tick_manager_shift_work_tick(SHIFT_US));

    assert(wait_for_count(&work_state.count, TICKS_PER_PHASE * 2, 7000));
    assert(wait_for_count(&watchface_state.count, TICKS_PER_PHASE, 1000));

    int64_t work_interval_us = tick_intervals_ms[TICK_WORK] * 1000LL;
    int64_t watchface_interval_us = tick_intervals_ms[TICK_WATCHFACE] * 1000LL;

    assert_tick_interval(&work_state, 5, work_interval_us + SHIFT_US);

    for(unsigned int i = 6; i < TICKS_PER_PHASE * 2; i++) {
        assert_tick_interval(&work_state, i, work_interval_us);
    }

    for(unsigned int i = 1; i < TICKS_PER_PHASE; i++) {
        assert_tick_interval(&watchface_state, i, watchface_interval_us);
    }

    tick_manager_stop_tick(TICK_WORK);
    unsigned int work_count = atomic_load(&work_state.count);

    tick_manager_stop_tick(TICK_WATCHFACE);
    unsigned int watchface_count = atomic_load(&watchface_state.count);

    vTaskDelay(pdMS_TO_TICKS(1200));

    assert(atomic_load(&work_state.count) == work_count);
    assert(atomic_load(&watchface_state.count) == watchface_count);

    teardown_test();
}

static void host_test_task(void *arg) {
    (void)arg;

    test_api_edge_cases();
    test_individual_start_stop();
    test_simultaneous_operation();
    test_work_tick_shift();
    test_work_tick_shift_with_watchface();

    printf("[TEST] all tick manager tests passed\n");
    exit(0);
}

int main(void) {
    BaseType_t result = xTaskCreate(host_test_task, "Host test", 4096, NULL, 1, NULL);
    assert(result == pdPASS);

    vTaskStartScheduler();

    return 0;
}
