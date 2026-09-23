#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <ring_buffer.h>

#define BUFFER_CAPACITY  64
#define NUM_ITEMS        10000

static ring_buffer_t rb;
static int storage[BUFFER_CAPACITY];

static atomic_bool producer_done = false;
static atomic_bool consumer_done = false;
static atomic_bool test_failed = false;

static size_t consumed_items = 0;

static void producer_task(void *arg) {
    (void)arg;

    for (int i = 0; i < NUM_ITEMS; i++) {
        if (!ringbuf_push(&rb, &i)) {
            atomic_store(&test_failed, true);
            break;
        }

        vTaskDelay(0);
    }

    atomic_store(&producer_done, true);
    vTaskDelete(NULL);
}

static void consumer_task(void *arg) {
    (void)arg;

    int value;
    int previous_value = -1;

    while (!atomic_load(&producer_done) || !ringbuf_is_empty(&rb)) {
        if (ringbuf_pop(&rb, &value)) {
            if (value < 0 || value >= NUM_ITEMS) {
                atomic_store(&test_failed, true);
                break;
            }

            if (value <= previous_value) {
                atomic_store(&test_failed, true);
                break;
            }

            previous_value = value;
            consumed_items++;

            if (consumed_items > NUM_ITEMS) {
                atomic_store(&test_failed, true);
                break;
            }
        }
        vTaskDelay(0);
    }

    atomic_store(&consumer_done, true);
    vTaskDelete(NULL);
}

/*
 * Test: concurrent push and pop
 *
 * Verifies producer and consumer tasks can access the buffer concurrently,
 * while preserving valid FIFO ordering for values that are retained.
 */
static void test_task(void *arg) {
    (void)arg;

    printf("test_concurrent_push_pop...\n");

    assert(ringbuf_init(
        &rb,
        storage,
        BUFFER_CAPACITY,
        sizeof(int),
        RB_OVERFLOW_DROP_OLDEST
    ));
    assert(xTaskCreate(
        producer_task,
        "producer",
        2048,
        NULL,
        2,
        NULL
    ) == pdPASS);
    assert(xTaskCreate(
        consumer_task,
        "consumer",
        2048,
        NULL,
        2,
        NULL
    ) == pdPASS);

    while (!atomic_load(&producer_done) || !atomic_load(&consumer_done)) {
        vTaskDelay(1);
    }

    assert(!atomic_load(&test_failed));
    assert(consumed_items > 0);
    assert(consumed_items <= NUM_ITEMS);
    assert(ringbuf_is_empty(&rb));

    ringbuf_deinit(&rb);

    printf("  PASS (%zu/%d values consumed)\n",
           consumed_items,
           NUM_ITEMS);
    printf("=== CONCURRENCY TEST PASSED ===\n");

    exit(0);
}

int main(void) {
    assert(xTaskCreate(
        test_task,
        "test",
        4096,
        NULL,
        1,
        NULL
    ) == pdPASS);

    vTaskStartScheduler();
    return 0;
}
