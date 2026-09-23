#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <FreeRTOS.h>
#include <task.h>

#include <ring_buffer.h>

/*
 * Test: basic FIFO behavior.
 *
 * Verifies insertion and removal preserve FIFO ordering.
 */
static void test_basic_fifo(void) {
    printf("test_basic_fifo...\n");

    ring_buffer_t rb;
    int storage[4];

    assert(ringbuf_init(
        &rb,
        storage,
        4,
        sizeof(int),
        RB_OVERFLOW_DROP_OLDEST
    ));

    assert(ringbuf_capacity(&rb) == 4);
    assert(ringbuf_is_empty(&rb));
    assert(!ringbuf_is_full(&rb));

    int value;

    assert(ringbuf_push(&rb, &(int){10}));
    assert(ringbuf_push(&rb, &(int){20}));
    assert(ringbuf_push(&rb, &(int){30}));
    assert(!ringbuf_is_empty(&rb));
    assert(!ringbuf_is_full(&rb));
    assert(ringbuf_pop(&rb, &value));
    assert(value == 10);
    assert(ringbuf_pop(&rb, &value));
    assert(value == 20);
    assert(ringbuf_pop(&rb, &value));
    assert(value == 30);
    assert(ringbuf_is_empty(&rb));

    ringbuf_deinit(&rb);

    printf("  PASS\n");
}

/* Test: pop when empty
 *
 * Verifies popping from an empty buffer fails safely.
 */
static void test_empty_pop(void) {
    printf("test_empty_pop...\n");

    ring_buffer_t rb;
    int storage[4];

    assert(ringbuf_init(
        &rb,
        storage,
        4,
        sizeof(int),
        RB_OVERFLOW_DROP_OLDEST
    ));

    int value;

    assert(!ringbuf_pop(&rb, &value));
    assert(ringbuf_is_empty(&rb));

    ringbuf_deinit(&rb);

    printf("  PASS\n");
}

/* Test: full state
 *
 * Verifies the buffer reports full after reaching capacity.
 */
static void test_full_state(void) {
    printf("test_full_state...\n");

    ring_buffer_t rb;
    int storage[3];

    assert(ringbuf_init(
        &rb,
        storage,
        3,
        sizeof(int),
        RB_OVERFLOW_DROP_OLDEST
    ));

    assert(!ringbuf_is_full(&rb));
    assert(ringbuf_push(&rb, &(int){1}));
    assert(!ringbuf_is_full(&rb));
    assert(ringbuf_push(&rb, &(int){2}));
    assert(!ringbuf_is_full(&rb));
    assert(ringbuf_push(&rb, &(int){3}));
    assert(ringbuf_is_full(&rb));

    ringbuf_deinit(&rb);

    printf("  PASS\n");
}

/*
 * Test: oldest overwrite policy
 *
 * Verifies the oldest element is dropped when the buffer is full.
 */
static void test_drop_oldest(void) {
    printf("test_drop_oldest...\n");

    ring_buffer_t rb;
    int storage[3];

    assert(ringbuf_init(
        &rb,
        storage,
        3,
        sizeof(int),
        RB_OVERFLOW_DROP_OLDEST
    ));

    assert(ringbuf_push(&rb, &(int){1}));
    assert(ringbuf_push(&rb, &(int){2}));
    assert(ringbuf_push(&rb, &(int){3}));
    assert(ringbuf_is_full(&rb));
    assert(ringbuf_push(&rb, &(int){4}));

    int value;

    assert(ringbuf_pop(&rb, &value));
    assert(value == 2);
    assert(ringbuf_pop(&rb, &value));
    assert(value == 3);
    assert(ringbuf_pop(&rb, &value));
    assert(value == 4);
    assert(ringbuf_is_empty(&rb));

    ringbuf_deinit(&rb);

    printf("  PASS\n");
}

/*
 * Test: repeated oldest drop policy
 *
 * Verifies repeated overwrites retain the newest elements in FIFO order.
 */
static void test_drop_oldest_repeated(void) {
    printf("test_drop_oldest_repeated...\n");

    ring_buffer_t rb;
    int storage[3];

    assert(ringbuf_init(
        &rb,
        storage,
        3,
        sizeof(int),
        RB_OVERFLOW_DROP_OLDEST
    ));

    for (int i = 1; i <= 10; i++) {
        assert(ringbuf_push(&rb, &i));
    }

    int value;

    assert(ringbuf_pop(&rb, &value));
    assert(value == 8);
    assert(ringbuf_pop(&rb, &value));
    assert(value == 9);
    assert(ringbuf_pop(&rb, &value));
    assert(value == 10);
    assert(ringbuf_is_empty(&rb));

    ringbuf_deinit(&rb);

    printf("  PASS\n");
}

/*
 * Test: newest drop policy
 *
 * Verifies pushes are rejected when the buffer is full.
 */
static void test_drop_newest(void) {
    printf("test_drop_newest...\n");

    ring_buffer_t rb;
    int storage[3];

    assert(ringbuf_init(
        &rb,
        storage,
        3,
        sizeof(int),
        RB_OVERFLOW_DROP_NEWEST
    ));

    assert(ringbuf_push(&rb, &(int){1}));
    assert(ringbuf_push(&rb, &(int){2}));
    assert(ringbuf_push(&rb, &(int){3}));
    assert(ringbuf_is_full(&rb));
    assert(!ringbuf_push(&rb, &(int){4}));

    int value;

    assert(ringbuf_pop(&rb, &value));
    assert(value == 1);
    assert(ringbuf_pop(&rb, &value));
    assert(value == 2);
    assert(ringbuf_pop(&rb, &value));
    assert(value == 3);
    assert(ringbuf_is_empty(&rb));

    ringbuf_deinit(&rb);

    printf("  PASS\n");
}

/*
 * Test: buffer with capacity 1
 *
 * Verifies FIFO and overflow behavior for a single-element buffer.
 */
static void test_capacity_one(void) {
    printf("test_capacity_one...\n");

    ring_buffer_t rb;
    int storage[1];

    assert(ringbuf_init(
        &rb,
        storage,
        1,
        sizeof(int),
        RB_OVERFLOW_DROP_OLDEST
    ));

    assert(ringbuf_capacity(&rb) == 1);
    assert(ringbuf_is_empty(&rb));
    assert(ringbuf_push(&rb, &(int){10}));
    assert(ringbuf_is_full(&rb));

    int value;

    assert(ringbuf_pop(&rb, &value));
    assert(value == 10);
    assert(ringbuf_is_empty(&rb));
    assert(ringbuf_push(&rb, &(int){20}));
    assert(ringbuf_push(&rb, &(int){30}));
    assert(ringbuf_is_full(&rb));
    assert(ringbuf_pop(&rb, &value));
    assert(value == 30);
    assert(ringbuf_is_empty(&rb));

    ringbuf_deinit(&rb);

    printf("  PASS\n");
}

/*
 * Test: wraparound
 *
 * Verifies FIFO behavior across buffer index wraparound.
 */
static void test_wraparound(void) {
    printf("test_wraparound...\n");

    ring_buffer_t rb;
    int storage[4];

    assert(ringbuf_init(
        &rb,
        storage,
        4,
        sizeof(int),
        RB_OVERFLOW_DROP_OLDEST
    ));

    int value;

    assert(ringbuf_push(&rb, &(int){1}));
    assert(ringbuf_push(&rb, &(int){2}));
    assert(ringbuf_pop(&rb, &value));
    assert(value == 1);
    assert(ringbuf_pop(&rb, &value));
    assert(value == 2);
    assert(ringbuf_push(&rb, &(int){3}));
    assert(ringbuf_push(&rb, &(int){4}));
    assert(ringbuf_push(&rb, &(int){5}));
    assert(ringbuf_push(&rb, &(int){6}));
    assert(ringbuf_is_full(&rb));
    assert(ringbuf_pop(&rb, &value));
    assert(value == 3);
    assert(ringbuf_pop(&rb, &value));
    assert(value == 4);
    assert(ringbuf_pop(&rb, &value));
    assert(value == 5);
    assert(ringbuf_pop(&rb, &value));
    assert(value == 6);
    assert(ringbuf_is_empty(&rb));

    ringbuf_deinit(&rb);

    printf("  PASS\n");
}

/*
 * Test: pop when buffer full
 *
 * Verifies pop works when buffer is full and subsequent
 * push fills up buffer again. Further pops work till empty
 */
static void test_full_pop_then_push(void) {
    printf("test_full_pop_then_push...\n");

    ring_buffer_t rb;
    int storage[3];

    assert(ringbuf_init(
        &rb,
        storage,
        3,
        sizeof(int),
        RB_OVERFLOW_DROP_OLDEST
    ));

    assert(ringbuf_push(&rb, &(int){1}));
    assert(ringbuf_push(&rb, &(int){2}));
    assert(ringbuf_push(&rb, &(int){3}));
    assert(ringbuf_is_full(&rb));

    int value;

    assert(ringbuf_pop(&rb, &value));
    assert(value == 1);
    assert(!ringbuf_is_full(&rb));
    assert(ringbuf_push(&rb, &(int){4}));
    assert(ringbuf_is_full(&rb));
    assert(ringbuf_pop(&rb, &value));
    assert(value == 2);
    assert(ringbuf_pop(&rb, &value));
    assert(value == 3);
    assert(ringbuf_pop(&rb, &value));
    assert(value == 4);
    assert(ringbuf_is_empty(&rb));

    ringbuf_deinit(&rb);

    printf("  PASS\n");
}

/*
 * Test: clear functionality
 *
 * Verifies clear empties buffer and further pops fail
 */
static void test_clear(void) {
    printf("test_clear...\n");

    ring_buffer_t rb;
    int storage[4];

    assert(ringbuf_init(
        &rb,
        storage,
        4,
        sizeof(int),
        RB_OVERFLOW_DROP_OLDEST
    ));

    assert(ringbuf_push(&rb, &(int){10}));
    assert(ringbuf_push(&rb, &(int){20}));
    assert(!ringbuf_is_empty(&rb));
    assert(ringbuf_clear(&rb));
    assert(ringbuf_is_empty(&rb));
    assert(!ringbuf_is_full(&rb));

    int value;
    assert(!ringbuf_pop(&rb, &value));

    ringbuf_deinit(&rb);

    printf("  PASS\n");
}

/*
 * Test: clear functionality after wrap around
 *
 * Verifies clear resets the buffer correctly after wrapped indices.
 */
static void test_clear_after_wraparound(void) {
    printf("test_clear_after_wraparound...\n");

    ring_buffer_t rb;
    int storage[3];

    assert(ringbuf_init(
        &rb,
        storage,
        3,
        sizeof(int),
        RB_OVERFLOW_DROP_OLDEST
    ));

    int value;

    assert(ringbuf_push(&rb, &(int){1}));
    assert(ringbuf_push(&rb, &(int){2}));
    assert(ringbuf_pop(&rb, &value));
    assert(value == 1);
    assert(ringbuf_push(&rb, &(int){3}));
    assert(ringbuf_push(&rb, &(int){4}));
    assert(ringbuf_clear(&rb));
    assert(ringbuf_is_empty(&rb));
    assert(!ringbuf_is_full(&rb));
    assert(ringbuf_push(&rb, &(int){100}));
    assert(ringbuf_push(&rb, &(int){200}));
    assert(ringbuf_pop(&rb, &value));
    assert(value == 100);
    assert(ringbuf_pop(&rb, &value));
    assert(value == 200);
    assert(ringbuf_is_empty(&rb));

    ringbuf_deinit(&rb);

    printf("  PASS\n");
}

typedef struct {
    uint8_t id;
    uint32_t timestamp;
    char name[8];
} test_struct_t;

/*
 * Test: different element size
 *
 * Verifies the buffer correctly stores and retrieves structured elements.
 */
static void test_different_element_size(void) {
    printf("test_different_element_size...\n");

    ring_buffer_t rb;
    test_struct_t storage[3];

    assert(ringbuf_init(
        &rb,
        storage,
        3,
        sizeof(test_struct_t),
        RB_OVERFLOW_DROP_OLDEST
    ));

    const test_struct_t input1 = {
        .id = 1,
        .timestamp = 123456,
        .name = "first"
    };
    const test_struct_t input2 = {
        .id = 2,
        .timestamp = 789012,
        .name = "second"
    };

    assert(ringbuf_push(&rb, &input1));
    assert(ringbuf_push(&rb, &input2));

    test_struct_t output;

    assert(ringbuf_pop(&rb, &output));
    assert(memcmp(&output, &input1, sizeof(output)) == 0);
    assert(ringbuf_pop(&rb, &output));
    assert(memcmp(&output, &input2, sizeof(output)) == 0);
    assert(ringbuf_is_empty(&rb));

    ringbuf_deinit(&rb);

    printf("  PASS\n");
}

/*
 * Test: Invalid initialization sequence
 */
static void test_invalid_initialization(void) {
    printf("test_invalid_initialization...\n");

    ring_buffer_t rb;
    int storage[4];

    assert(!ringbuf_init(
        NULL,
        storage,
        4,
        sizeof(int),
        RB_OVERFLOW_DROP_OLDEST
    ));

    assert(!ringbuf_init(
        &rb,
        NULL,
        4,
        sizeof(int),
        RB_OVERFLOW_DROP_OLDEST
    ));
    assert(!ringbuf_init(
        &rb,
        storage,
        0,
        sizeof(int),
        RB_OVERFLOW_DROP_OLDEST
    ));
    assert(!ringbuf_init(
        &rb,
        storage,
        4,
        0,
        RB_OVERFLOW_DROP_OLDEST
    ));

    printf("  PASS\n");
}

/*
 * Test: invalid API calls
 *
 * Verifies invalid handles and uninitialized buffers are rejected safely.
 */
static void test_invalid_operations(void) {
    printf("test_invalid_operations...\n");

    ring_buffer_t rb;
    int storage[4];
    int value = 123;

    assert(!ringbuf_push(NULL, &value));
    assert(!ringbuf_push(NULL, NULL));
    assert(!ringbuf_pop(NULL, &value));
    assert(!ringbuf_pop(NULL, NULL));
    assert(ringbuf_is_empty(NULL));
    assert(ringbuf_is_full(NULL));
    assert(!ringbuf_clear(NULL));

    memset(&rb, 0, sizeof(rb));

    assert(!ringbuf_push(&rb, &value));
    assert(!ringbuf_pop(&rb, &value));
    assert(ringbuf_is_empty(&rb));
    assert(ringbuf_is_full(&rb));
    assert(!ringbuf_clear(&rb));

    (void)storage;

    printf("  PASS\n");
}

/*
 * Test: operations after deinit
 *
 * Verifies the buffer rejects operations after deinitialization.
 */
static void test_deinit(void) {
    printf("test_deinit...\n");

    ring_buffer_t rb;
    int storage[4];

    assert(ringbuf_init(
        &rb,
        storage,
        4,
        sizeof(int),
        RB_OVERFLOW_DROP_OLDEST
    ));
    assert(rb.mutex != NULL);

    ringbuf_deinit(&rb);

    assert(rb.mutex == NULL);

    int value;

    assert(!ringbuf_push(&rb, &(int){10}));
    assert(!ringbuf_pop(&rb, &value));
    assert(ringbuf_is_empty(&rb));
    assert(ringbuf_is_full(&rb));
    assert(!ringbuf_clear(&rb));

    printf("  PASS\n");
}

static void test_task(void *arg) {
    (void)arg;

    printf("=== Ring buffer tests ===\n");

    test_basic_fifo();
    test_empty_pop();
    test_full_state();

    test_drop_oldest();
    test_drop_oldest_repeated();
    test_drop_newest();

    test_capacity_one();
    test_wraparound();
    test_full_pop_then_push();

    test_clear();
    test_clear_after_wraparound();

    test_different_element_size();

    test_invalid_initialization();
    test_invalid_operations();
    test_deinit();

    printf("=== ALL TESTS PASSED ===\n");

    exit(0);
}

int main(void) {
    assert(xTaskCreate(
        test_task,
        "ring_buffer_test",
        2048,
        NULL,
        1,
        NULL
    ) == pdPASS);

    vTaskStartScheduler();

    return 0;
}
