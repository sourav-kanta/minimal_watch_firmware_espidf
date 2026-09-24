#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <alarm_manager.h>
#include <alarm_manager_internal.h>
#include <state_manager.h>
#include <event_manager.h>
#include <common_consts.h>

static uint32_t mock_epoch_time = 100000;
static alarm_sync_t test_alarms;
static atomic_int alarms_fired;
static alarm_t last_fired_alarm;

uint32_t state_manager_get_epoch_time(void) {
    return mock_epoch_time;
}

static void test_alarm_event_observer(const event_t *event) {
    assert(event != NULL);
    assert(event->ev == EVENT_ALARM_TRIGGERED);

    if (event->payload_len == sizeof(alarm_t) && event->data != NULL) {
        memcpy(&last_fired_alarm, event->data, sizeof(alarm_t));
    }

    atomic_fetch_add(&alarms_fired, 1);
}

static void reset_test_state(void) {
    memset(&test_alarms, 0, sizeof(alarm_sync_t));
    memset(&last_fired_alarm, 0, sizeof(alarm_t));
    mock_epoch_time = 100000;
    atomic_store(&alarms_fired, 0);
}

/*
 * Test: Initialization with empty list.
 *
 * Verifies that alarm manager initializes safely when provided with an
 * empty alarm synchronization structure.
 */
static void test_init_empty(void) {
    printf("test_init_empty...\n");

    reset_test_state();
    
    alarm_manager_init(&test_alarms);

    alarm_t buff[MAX_WATCH_ALARMS];
    assert(alarm_manager_get_all_alarms(buff) == 0);

    alarm_manager_deinit();

    printf("  PASS\n");
}

/*
 * Test: Initialization sorts and discards.
 *
 * Verifies that initialization correctly discards expired alarms and
 * sorts remaining alarms chronologically.
 */
static void test_init_sorts_and_discards(void) {
    printf("test_init_sorts_and_discards...\n");

    reset_test_state();

    test_alarms.n_alarms = 4;
    test_alarms.alarms[0].epoch = 100020;
    test_alarms.alarms[1].epoch = 100001; 
    test_alarms.alarms[2].epoch = 100010;
    test_alarms.alarms[3].epoch = 100004;

    alarm_manager_init(&test_alarms);

    alarm_t buff[MAX_WATCH_ALARMS];
    uint8_t count = alarm_manager_get_all_alarms(buff);

    assert(count == 2);
    assert(test_alarms.n_alarms == 2);
    assert(buff[0].epoch == 100010);
    assert(buff[1].epoch == 100020);

    alarm_manager_deinit();

    printf("  PASS\n");
}

/*
 * Test: Create alarm validation limits.
 *
 * Verifies that alarms scheduled in the past, within the buffer period,
 * at the buffer boundary, too far in the future, or exactly duplicating
 * an existing alarm are rejected.
 */
static void test_create_alarm_validation(void) {
    printf("test_create_alarm_validation...\n");

    reset_test_state();
    alarm_manager_init(&test_alarms);

    alarm_t past_alarm = { .epoch = 99999 };
    assert(!alarm_manager_create_alarm(&past_alarm));

    alarm_t buffer_alarm = { .epoch = 100004 };
    assert(!alarm_manager_create_alarm(&buffer_alarm));

    alarm_t boundary_alarm = { .epoch = mock_epoch_time + 5 };
    assert(!alarm_manager_create_alarm(&boundary_alarm));

    alarm_t valid_boundary_alarm = { .epoch = mock_epoch_time + 6 };
    assert(alarm_manager_create_alarm(&valid_boundary_alarm));

    alarm_t far_future_alarm = { .epoch = mock_epoch_time + 31536001ULL };
    assert(!alarm_manager_create_alarm(&far_future_alarm));

    assert(!alarm_manager_create_alarm(&valid_boundary_alarm));

    alarm_manager_deinit();

    printf("  PASS\n");
}

/*
 * Test: Create alarm sorting.
 *
 * Verifies that randomly ordered alarm insertions are automatically
 * sorted sequentially by epoch time.
 */
static void test_create_alarm_sorting(void) {
    printf("test_create_alarm_sorting...\n");

    reset_test_state();
    alarm_manager_init(&test_alarms);

    alarm_t a1 = { .epoch = 100030 };
    alarm_t a2 = { .epoch = 100010 };
    alarm_t a3 = { .epoch = 100020 };

    assert(alarm_manager_create_alarm(&a1));
    assert(alarm_manager_create_alarm(&a2));
    assert(alarm_manager_create_alarm(&a3));

    alarm_t buff[MAX_WATCH_ALARMS];
    uint8_t count = alarm_manager_get_all_alarms(buff);

    assert(count == 3);
    assert(buff[0].epoch == 100010);
    assert(buff[1].epoch == 100020);
    assert(buff[2].epoch == 100030);

    alarm_manager_deinit();

    printf("  PASS\n");
}

/*
 * Test: Create alarm capacity limits.
 *
 * Verifies that the manager drops the latest chronological alarm to accommodate
 * a new earlier alarm when maximum capacity is reached, and rejects new alarms
 * that execute later than the current latest alarm.
 */
static void test_create_alarm_capacity(void) {
    printf("test_create_alarm_capacity...\n");

    reset_test_state();
    alarm_manager_init(&test_alarms);

    for (int i = 0; i < MAX_WATCH_ALARMS; i++) {
        alarm_t a = { .epoch = mock_epoch_time + 10 + i };
        assert(alarm_manager_create_alarm(&a));
    }

    alarm_t late_alarm = { .epoch = mock_epoch_time + 1000 };
    assert(!alarm_manager_create_alarm(&late_alarm));

    alarm_t early_alarm = { .epoch = mock_epoch_time + 8 };
    assert(alarm_manager_create_alarm(&early_alarm));

    alarm_t buff[MAX_WATCH_ALARMS];
    uint8_t count = alarm_manager_get_all_alarms(buff);

    assert(count == MAX_WATCH_ALARMS);
    assert(buff[0].epoch == mock_epoch_time + 8);

    for (int i = 1; i < MAX_WATCH_ALARMS; i++) {
        assert(buff[i].epoch == mock_epoch_time + 9 + i);
    }

    alarm_manager_deinit();

    printf("  PASS\n");
}

/*
 * Test: Delete alarm.
 *
 * Verifies that alarms can be removed by index, shifting the internal
 * array sequentially and handling invalid indexes safely.
 */
static void test_delete_alarm(void) {
    printf("test_delete_alarm...\n");

    reset_test_state();
    alarm_manager_init(&test_alarms);

    alarm_t a1 = { .epoch = 100010 };
    alarm_t a2 = { .epoch = 100020 };
    alarm_t a3 = { .epoch = 100030 };

    assert(alarm_manager_create_alarm(&a1));
    assert(alarm_manager_create_alarm(&a2));
    assert(alarm_manager_create_alarm(&a3));

    assert(!alarm_manager_delete_alarm(3));
    assert(!alarm_manager_delete_alarm(-1));

    assert(alarm_manager_delete_alarm(1));

    alarm_t buff[MAX_WATCH_ALARMS];
    uint8_t count = alarm_manager_get_all_alarms(buff);

    assert(count == 2);
    assert(buff[0].epoch == 100010);
    assert(buff[1].epoch == 100030);

    assert(alarm_manager_delete_alarm(0));

    count = alarm_manager_get_all_alarms(buff);
    assert(count == 1);
    assert(buff[0].epoch == 100030);

    alarm_manager_deinit();

    printf("  PASS\n");
}

/*
 * Test: Delete only alarm.
 *
 * Verifies that deleting the final alarm empties the queue and stops
 * the active timer safely.
 */
static void test_delete_only_alarm(void) {
    printf("test_delete_only_alarm...\n");

    reset_test_state();
    alarm_manager_init(&test_alarms);

    alarm_t alarm = { .epoch = 100010 };
    assert(alarm_manager_create_alarm(&alarm));

    assert(alarm_manager_delete_alarm(0));

    alarm_t buff[MAX_WATCH_ALARMS];
    assert(alarm_manager_get_all_alarms(buff) == 0);

    alarm_manager_deinit();

    printf("  PASS\n");
}

/*
 * Test: Edit alarm.
 *
 * Verifies that valid edits replace the target alarm properly and invalid
 * edits trigger a rollback restoring the original alarm.
 */
static void test_edit_alarm(void) {
    printf("test_edit_alarm...\n");

    reset_test_state();
    alarm_manager_init(&test_alarms);

    alarm_t a1 = { .epoch = 100010 };
    alarm_t a2 = { .epoch = 100020 };

    assert(alarm_manager_create_alarm(&a1));
    assert(alarm_manager_create_alarm(&a2));

    alarm_t valid_edit = { .epoch = 100030 };
    assert(alarm_manager_edit_alarm(0, &valid_edit));

    alarm_t invalid_edit_duplicate = { .epoch = 100030 };
    assert(!alarm_manager_edit_alarm(0, &invalid_edit_duplicate));

    alarm_t invalid_edit_past = { .epoch = 99999 };
    assert(!alarm_manager_edit_alarm(1, &invalid_edit_past));

    alarm_t buff[MAX_WATCH_ALARMS];
    uint8_t count = alarm_manager_get_all_alarms(buff);

    assert(count == 2);
    assert(buff[0].epoch == 100020);
    assert(buff[1].epoch == 100030);

    alarm_manager_deinit();

    printf("  PASS\n");
}

/*
 * Test: Revalidate alarms.
 *
 * Verifies that advancing the global epoch and triggering revalidation
 * discards alarms that fall behind the time buffer.
 */
static void test_revalidate(void) {
    printf("test_revalidate...\n");

    reset_test_state();
    alarm_manager_init(&test_alarms);

    alarm_t a1 = { .epoch = 100010 };
    alarm_t a2 = { .epoch = 100020 };

    assert(alarm_manager_create_alarm(&a1));
    assert(alarm_manager_create_alarm(&a2));

    mock_epoch_time = 100014;

    assert(alarm_manager_revalidate());

    alarm_t buff[MAX_WATCH_ALARMS];
    uint8_t count = alarm_manager_get_all_alarms(buff);

    assert(count == 1);
    assert(buff[0].epoch == 100020);

    alarm_manager_deinit();

    printf("  PASS\n");
}

/*
 * Test: Revalidate all expired alarms.
 *
 * Verifies that revalidation removes all alarms that have entered the
 * buffer window and leaves the manager with an empty queue.
 */
static void test_revalidate_all_expired(void) {
    printf("test_revalidate_all_expired...\n");

    reset_test_state();
    alarm_manager_init(&test_alarms);

    alarm_t a1 = { .epoch = 100010 };
    alarm_t a2 = { .epoch = 100020 };

    assert(alarm_manager_create_alarm(&a1));
    assert(alarm_manager_create_alarm(&a2));

    mock_epoch_time = 100020;

    assert(alarm_manager_revalidate());

    alarm_t buff[MAX_WATCH_ALARMS];
    assert(alarm_manager_get_all_alarms(buff) == 0);

    alarm_manager_deinit();

    printf("  PASS\n");
}

/*
 * Test: Alarm firing.
 *
 * Verifies that an active timer triggers the callback, removes the expired
 * alarm from the queue, schedules the next alarm, and dispatches the event.
 */
static void test_alarm_firing(void) {
    printf("test_alarm_firing...\n");

    reset_test_state();
    alarm_manager_init(&test_alarms);

    alarm_t a1 = {
        .epoch = 100006,
    };
    alarm_t a2 = {
        .epoch = 100020,
    };

    assert(alarm_manager_create_alarm(&a1));
    assert(alarm_manager_create_alarm(&a2));

    uint32_t elapsed = 0;
    while (atomic_load(&alarms_fired) == 0 && elapsed < 7000) {
        vTaskDelay(pdMS_TO_TICKS(10));
        elapsed += 10;
    }

    assert(atomic_load(&alarms_fired) == 1);
    assert(memcmp(&last_fired_alarm, &a1, sizeof(alarm_t)) == 0);

    alarm_t buff[MAX_WATCH_ALARMS];
    uint8_t count = alarm_manager_get_all_alarms(buff);

    assert(count == 1);
    assert(buff[0].epoch == 100020);

    alarm_manager_deinit();

    printf("  PASS\n");
}

/*
 * Test: Multiple alarms firing.
 *
 * Verifies that when multiple alarms are queued, the manager successfully
 * fires the first, removes it, and schedules the subsequent alarm which 
 * also fires successfully.
 */
static void test_multiple_alarms_firing(void) {
    printf("test_multiple_alarms_firing...\n");

    reset_test_state();
    alarm_manager_init(&test_alarms);

    alarm_t a1 = {
        .epoch = 100006,
    };
    alarm_t a2 = {
        .epoch = 100007,
    };

    assert(alarm_manager_create_alarm(&a1));
    assert(alarm_manager_create_alarm(&a2));

    uint32_t elapsed = 0;
    while (atomic_load(&alarms_fired) == 0 && elapsed < 8000) {
        vTaskDelay(pdMS_TO_TICKS(10));
        elapsed += 10;
    }

    assert(atomic_load(&alarms_fired) == 1);
    assert(memcmp(&last_fired_alarm, &a1, sizeof(alarm_t)) == 0);

    elapsed = 0;
    while (atomic_load(&alarms_fired) == 1 && elapsed < 8000) {
        vTaskDelay(pdMS_TO_TICKS(10));
        elapsed += 10;
    }

    assert(atomic_load(&alarms_fired) == 2);
    assert(memcmp(&last_fired_alarm, &a2, sizeof(alarm_t)) == 0);

    alarm_t buff[MAX_WATCH_ALARMS];
    uint8_t count = alarm_manager_get_all_alarms(buff);

    assert(count == 0);

    alarm_manager_deinit();

    printf("  PASS\n");
}

/*
 * Test: Post-deinit safety.
 *
 * Verifies operations fail gracefully without faulting if called after
 * the manager is deinitialized.
 */
static void test_deinit_safety(void) {
    printf("test_deinit_safety...\n");

    reset_test_state();
    alarm_manager_init(&test_alarms);
    alarm_manager_deinit();

    alarm_t a = { .epoch = 100010 };
    alarm_t buff[MAX_WATCH_ALARMS];

    assert(!alarm_manager_create_alarm(&a));
    assert(!alarm_manager_delete_alarm(0));
    assert(!alarm_manager_edit_alarm(0, &a));
    assert(!alarm_manager_revalidate());
    assert(alarm_manager_get_all_alarms(buff) == 0);

    printf("  PASS\n");
}

static void host_test_task(void *arg) {
    (void)arg;

    assert(event_manager_init());
    assert(event_subscribe(EVENT_ALARM_TRIGGERED, test_alarm_event_observer));

    test_init_empty();
    test_init_sorts_and_discards();
    test_create_alarm_validation();
    test_create_alarm_sorting();
    test_create_alarm_capacity();
    test_delete_alarm();
    test_delete_only_alarm();
    test_edit_alarm();
    test_revalidate();
    test_revalidate_all_expired();
    test_alarm_firing();
    test_multiple_alarms_firing();
    test_deinit_safety();

    assert(event_unsubscribe(EVENT_ALARM_TRIGGERED, test_alarm_event_observer));
    event_manager_deinit();

    printf("[TEST] all alarm manager tests passed\n");
    
    vTaskEndScheduler();
    vTaskDelete(NULL);
}

int main(void) {
    xTaskCreate(
        host_test_task,
        "alarm_test",
        4096,
        NULL,
        3,
        NULL
    );

    vTaskStartScheduler();
    return 0;
}
