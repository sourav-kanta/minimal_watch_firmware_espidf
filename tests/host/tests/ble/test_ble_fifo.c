#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <ble_consts.h>
#include <ble_types.h>
#include <ble_fifo.h>

#define CONCURRENT_NUM_ITEMS 10000

static ble_msg_t make_message(
    uint8_t opcode,
    uint8_t req_app,
    uint8_t len,
    uint8_t seed
) {
    ble_msg_t msg = {0};

    msg.hdr.opcode = opcode;
    msg.hdr.req_app = req_app;
    msg.hdr.len = len;

    for (uint8_t i = 0; i < len; i++) {
        msg.payload[i] = (uint8_t)(seed + i);
    }

    return msg;
}

static void assert_message_equal(
    const ble_msg_t *expected,
    const ble_msg_t *actual
) {
    assert(expected->hdr.opcode == actual->hdr.opcode);
    assert(expected->hdr.req_app == actual->hdr.req_app);
    assert(expected->hdr.len == actual->hdr.len);

    assert(memcmp(
        expected->payload,
        actual->payload,
        expected->hdr.len
    ) == 0);
}

/*
 * Test: initialize empty FIFO
 *
 * Verifies both TX and RX FIFOs start empty after initialization.
 */
static void test_init_empty(void) {
    printf("test_init_empty...\n");

    assert(ble_fifo_init());

    ble_msg_t msg;

    assert(!get_next_ble_msg(&msg, BLE_TX));
    assert(!get_next_ble_msg(&msg, BLE_RX));

    ble_fifo_deinit();

    printf("  PASS\n");
}

/*
 * Test: TX FIFO
 *
 * Verifies messages can be added to and retrieved from the TX FIFO
 * in FIFO order.
 */
static void test_tx_fifo(void) {
    printf("test_tx_fifo...\n");

    assert(ble_fifo_init());

    ble_msg_t msg1 = make_message(0x01, 0x10, 5, 0x20);
    ble_msg_t msg2 = make_message(0x02, 0x11, 8, 0x30);

    assert(add_ble_msg_to_queue(&msg1, BLE_TX));
    assert(add_ble_msg_to_queue(&msg2, BLE_TX));

    ble_msg_t output;

    assert(get_next_ble_msg(&output, BLE_TX));
    assert_message_equal(&msg1, &output);
    assert(get_next_ble_msg(&output, BLE_TX));
    assert_message_equal(&msg2, &output);
    assert(!get_next_ble_msg(&output, BLE_TX));

    ble_fifo_deinit();

    printf("  PASS\n");
}

/*
 * Test: RX FIFO
 *
 * Verifies messages can be added to and retrieved from the RX FIFO
 * in FIFO order.
 */
static void test_rx_fifo(void) {
    printf("test_rx_fifo...\n");

    assert(ble_fifo_init());

    ble_msg_t msg1 = make_message(0x03, 0x20, 4, 0x40);
    ble_msg_t msg2 = make_message(0x04, 0x21, 7, 0x50);

    assert(add_ble_msg_to_queue(&msg1, BLE_RX));
    assert(add_ble_msg_to_queue(&msg2, BLE_RX));

    ble_msg_t output;

    assert(get_next_ble_msg(&output, BLE_RX));
    assert_message_equal(&msg1, &output);
    assert(get_next_ble_msg(&output, BLE_RX));
    assert_message_equal(&msg2, &output);
    assert(!get_next_ble_msg(&output, BLE_RX));

    ble_fifo_deinit();

    printf("  PASS\n");
}

/*
 * Test: TX and RX independence
 *
 * Verifies messages in the TX and RX FIFOs do not affect each other.
 */
static void test_tx_rx_are_independent(void) {
    printf("test_tx_rx_are_independent...\n");

    assert(ble_fifo_init());

    ble_msg_t tx = make_message(0x10, 0x01, 6, 0x60);
    ble_msg_t rx = make_message(0x20, 0x02, 6, 0x70);

    assert(add_ble_msg_to_queue(&tx, BLE_TX));
    assert(add_ble_msg_to_queue(&rx, BLE_RX));

    ble_msg_t output;

    assert(get_next_ble_msg(&output, BLE_TX));
    assert_message_equal(&tx, &output);
    assert(get_next_ble_msg(&output, BLE_RX));
    assert_message_equal(&rx, &output);
    assert(!get_next_ble_msg(&output, BLE_TX));
    assert(!get_next_ble_msg(&output, BLE_RX));

    ble_fifo_deinit();

    printf("  PASS\n");
}

/*
 * Test: FIFO ordering
 *
 * Verifies messages are retrieved in the same order they were added.
 */
static void test_fifo_order(void) {
    printf("test_fifo_order...\n");

    assert(ble_fifo_init());

    for (uint8_t i = 0; i < 10; i++) {
        ble_msg_t msg = make_message(
            i,
            (uint8_t)(i + 1),
            3,
            (uint8_t)(0x80 + i)
        );

        assert(add_ble_msg_to_queue(&msg, BLE_TX));
    }

    for (uint8_t i = 0; i < 10; i++) {
        ble_msg_t expected = make_message(
            i,
            (uint8_t)(i + 1),
            3,
            (uint8_t)(0x80 + i)
        );

        ble_msg_t output;

        assert(get_next_ble_msg(&output, BLE_TX));
        assert_message_equal(&expected, &output);
    }

    ble_msg_t output;
    assert(!get_next_ble_msg(&output, BLE_TX));

    ble_fifo_deinit();

    printf("  PASS\n");
}

/*
 * Test: maximum payload
 *
 * Verifies a message containing the maximum allowed payload is accepted
 * and retrieved without corruption.
 */
static void test_max_payload(void) {
    printf("test_max_payload...\n");

    assert(ble_fifo_init());

    ble_msg_t msg = make_message(
        0x55,
        0xAA,
        BLE_MAX_PAYLOAD_SIZE,
        0x00
    );

    assert(add_ble_msg_to_queue(&msg, BLE_TX));

    ble_msg_t output;

    assert(get_next_ble_msg(&output, BLE_TX));
    assert_message_equal(&msg, &output);

    ble_fifo_deinit();

    printf("  PASS\n");
}

/*
 * Test: payload too large
 *
 * Verifies messages exceeding the maximum payload size are rejected.
 */
static void test_payload_too_large(void) {
    printf("test_payload_too_large...\n");

    assert(ble_fifo_init());

    ble_msg_t msg = {0};

    msg.hdr.opcode = 0x55;
    msg.hdr.req_app = 0xAA;
    msg.hdr.len = BLE_MAX_PAYLOAD_SIZE + 1;

    assert(!add_ble_msg_to_queue(&msg, BLE_TX));

    ble_msg_t output;

    assert(!get_next_ble_msg(&output, BLE_TX));

    ble_fifo_deinit();

    printf("  PASS\n");
}

/*
 * Test: NULL arguments
 *
 * Verifies NULL message pointers are rejected by both enqueue and dequeue
 * operations.
 */
static void test_null_arguments(void) {
    printf("test_null_arguments...\n");

    assert(ble_fifo_init());

    ble_msg_t msg;

    assert(!add_ble_msg_to_queue(NULL, BLE_TX));
    assert(!add_ble_msg_to_queue(NULL, BLE_RX));
    assert(!get_next_ble_msg(NULL, BLE_TX));
    assert(!get_next_ble_msg(NULL, BLE_RX));
    assert(!get_next_ble_msg(&msg, BLE_TX));
    assert(!get_next_ble_msg(&msg, BLE_RX));

    ble_fifo_deinit();

    printf("  PASS\n");
}

/*
 * Test: invalid communication type
 *
 * Verifies invalid FIFO types are rejected without affecting TX or RX.
 */
static void test_invalid_type(void) {
    printf("test_invalid_type...\n");

    assert(ble_fifo_init());

    ble_msg_t msg = make_message(0x01, 0x02, 4, 0x10);
    ble_msg_t output;

    ble_comm_type_t invalid_type = (ble_comm_type_t)99;

    assert(!add_ble_msg_to_queue(&msg, invalid_type));
    assert(!get_next_ble_msg(&output, invalid_type));
    assert(!get_next_ble_msg(&output, BLE_TX));
    assert(!get_next_ble_msg(&output, BLE_RX));

    ble_fifo_deinit();

    printf("  PASS\n");
}

/*
 * Test: drop oldest
 *
 * Verifies FIFO overflow discards the oldest messages while retaining
 * the newest BLE_FIFO_SIZE messages.
 */
static void test_drop_oldest(void) {
    printf("test_drop_oldest...\n");

    assert(ble_fifo_init());

    for (uint8_t i = 0; i < BLE_FIFO_SIZE + 5; i++) {
        ble_msg_t msg = make_message(
            i,
            0x01,
            2,
            i
        );

        assert(add_ble_msg_to_queue(&msg, BLE_TX));
    }

    ble_msg_t output;

    for (uint8_t i = 5; i < BLE_FIFO_SIZE + 5; i++) {
        ble_msg_t expected = make_message(
            i,
            0x01,
            2,
            i
        );

        assert(get_next_ble_msg(&output, BLE_TX));
        assert_message_equal(&expected, &output);
    }

    assert(!get_next_ble_msg(&output, BLE_TX));

    ble_fifo_deinit();

    printf("  PASS\n");
}

/*
 * Test: repeated initialization
 *
 * Verifies the FIFO can be initialized again after being deinitialized
 * and does not retain messages from the previous instance.
 */
static void test_repeated_use(void) {
    printf("test_repeated_use...\n");

    assert(ble_fifo_init());

    ble_msg_t msg1 = make_message(0x11, 0x22, 3, 0x33);

    assert(add_ble_msg_to_queue(&msg1, BLE_TX));

    ble_fifo_deinit();

    assert(ble_fifo_init());

    ble_msg_t output;

    assert(!get_next_ble_msg(&output, BLE_TX));
    assert(!get_next_ble_msg(&output, BLE_RX));

    ble_msg_t msg2 = make_message(0x44, 0x55, 3, 0x66);

    assert(add_ble_msg_to_queue(&msg2, BLE_RX));

    assert(get_next_ble_msg(&output, BLE_RX));
    assert_message_equal(&msg2, &output);

    ble_fifo_deinit();

    printf("  PASS\n");
}

/*
 * Test: operations after deinitialization
 *
 * Verifies FIFO operations are rejected after the FIFO has been
 * deinitialized.
 */
static void test_after_deinit(void) {
    printf("test_after_deinit...\n");

    assert(ble_fifo_init());

    ble_msg_t msg = make_message(0x01, 0x02, 3, 0x10);
    ble_msg_t output;

    ble_fifo_deinit();

    assert(!add_ble_msg_to_queue(&msg, BLE_TX));
    assert(!add_ble_msg_to_queue(&msg, BLE_RX));
    assert(!get_next_ble_msg(&output, BLE_TX));
    assert(!get_next_ble_msg(&output, BLE_RX));

    printf("  PASS\n");
}

/*
 * Test: different payload sizes
 *
 * Verifies messages with zero-length, small, medium, and maximum-sized
 * payloads are preserved correctly.
 */
static void test_different_payload_sizes(void) {
    printf("test_different_payload_sizes...\n");

    assert(ble_fifo_init());

    const uint8_t lengths[] = {
        0,
        1,
        4,
        16,
        64,
        128,
        BLE_MAX_PAYLOAD_SIZE
    };

    for (size_t i = 0; i < sizeof(lengths); i++) {
        ble_msg_t msg = make_message(
            (uint8_t)i,
            0x42,
            lengths[i],
            (uint8_t)(i * 10)
        );

        assert(add_ble_msg_to_queue(&msg, BLE_TX));
    }

    for (size_t i = 0; i < sizeof(lengths); i++) {
        ble_msg_t expected = make_message(
            (uint8_t)i,
            0x42,
            lengths[i],
            (uint8_t)(i * 10)
        );

        ble_msg_t output;

        assert(get_next_ble_msg(&output, BLE_TX));
        assert_message_equal(&expected, &output);
    }

    ble_msg_t output;

    assert(!get_next_ble_msg(&output, BLE_TX));

    ble_fifo_deinit();

    printf("  PASS\n");
}

static atomic_bool concurrent_producer_done;
static atomic_bool concurrent_test_failed;
static atomic_size_t concurrent_consumed;

/*
 * Test: concurrent enqueue and dequeue
 *
 * Verifies the BLE FIFO remains consistent when one task continuously
 * enqueues messages while another task concurrently dequeues them.
 */
static void concurrent_producer_task(void *arg) {
    (void)arg;

    for (uint32_t i = 0; i < CONCURRENT_NUM_ITEMS; i++) {
        ble_msg_t msg = {0};

        msg.hdr.opcode = 0x55;
        msg.hdr.req_app = 0x42;
        msg.hdr.len = sizeof(uint32_t);

        memcpy(msg.payload, &i, sizeof(i));

        if (!add_ble_msg_to_queue(&msg, BLE_TX)) {
            atomic_store(&concurrent_test_failed, true);
            break;
        }

        vTaskDelay(0);
    }

    atomic_store(&concurrent_producer_done, true);
    vTaskDelete(NULL);
}

static atomic_bool concurrent_consumer_done;

static void concurrent_consumer_task(void *arg) {
    (void)arg;

    ble_msg_t output;
    uint32_t previous_sequence = 0;
    bool have_previous = false;

    while (!atomic_load(&concurrent_producer_done)) {
        if (get_next_ble_msg(&output, BLE_TX)) {
            uint32_t sequence;

            if (output.hdr.opcode != 0x55 ||
                output.hdr.req_app != 0x42 ||
                output.hdr.len != sizeof(uint32_t)) {
                atomic_store(&concurrent_test_failed, true);
                break;
            }

            memcpy(&sequence, output.payload, sizeof(sequence));

            if (have_previous && sequence <= previous_sequence) {
                atomic_store(&concurrent_test_failed, true);
                break;
            }

            previous_sequence = sequence;
            have_previous = true;

            atomic_fetch_add(&concurrent_consumed, 1);
        }

        vTaskDelay(0);
    }

    while (get_next_ble_msg(&output, BLE_TX)) {
        uint32_t sequence;

        if (output.hdr.opcode != 0x55 ||
            output.hdr.req_app != 0x42 ||
            output.hdr.len != sizeof(uint32_t)) {
            atomic_store(&concurrent_test_failed, true);
            break;
        }

        memcpy(&sequence, output.payload, sizeof(sequence));

        if (have_previous && sequence <= previous_sequence) {
            atomic_store(&concurrent_test_failed, true);
            break;
        }

        previous_sequence = sequence;
        have_previous = true;

        atomic_fetch_add(&concurrent_consumed, 1);
    }

    atomic_store(&concurrent_consumer_done, true);
    vTaskDelete(NULL);
}

/*
 * Test: concurrent enqueue and dequeue
 *
 * Verifies concurrent FIFO access preserves message integrity and
 * ordering for messages that survive DROP_OLDEST overflow.
 */
static void test_concurrent_enqueue_dequeue(void) {
    printf("test_concurrent_enqueue_dequeue...\n");

    atomic_store(&concurrent_producer_done, false);
    atomic_store(&concurrent_consumer_done, false);
    atomic_store(&concurrent_test_failed, false);
    atomic_store(&concurrent_consumed, 0);

    assert(ble_fifo_init());

    assert(xTaskCreate(
        concurrent_producer_task,
        "ble_producer",
        2048,
        NULL,
        2,
        NULL
    ) == pdPASS);
    assert(xTaskCreate(
        concurrent_consumer_task,
        "ble_consumer",
        2048,
        NULL,
        2,
        NULL
    ) == pdPASS);

    while (!atomic_load(&concurrent_producer_done) ||
           !atomic_load(&concurrent_consumer_done)) {
        vTaskDelay(1);
    }

    assert(!atomic_load(&concurrent_test_failed));
    assert(atomic_load(&concurrent_consumed) > 0);
    assert(atomic_load(&concurrent_consumed) <= CONCURRENT_NUM_ITEMS);

    ble_msg_t output;
    assert(!get_next_ble_msg(&output, BLE_TX));

    ble_fifo_deinit();

    printf(
        "  PASS (%zu/%d messages consumed)\n",
        atomic_load(&concurrent_consumed),
        CONCURRENT_NUM_ITEMS
    );
}

static void test_task(void *arg) {
    (void)arg;

    printf("=== BLE FIFO tests ===\n");

    test_init_empty();
    test_tx_fifo();
    test_rx_fifo();
    test_tx_rx_are_independent();
    test_fifo_order();
    test_max_payload();
    test_payload_too_large();
    test_null_arguments();
    test_invalid_type();
    test_drop_oldest();
    test_repeated_use();
    test_after_deinit();
    test_different_payload_sizes();
    test_concurrent_enqueue_dequeue();

    printf("=== ALL BLE FIFO TESTS PASSED ===\n");

    exit(0);
}

int main(void) {
    assert(xTaskCreate(
        test_task,
        "ble_fifo_tests",
        4096,
        NULL,
        1,
        NULL
    ) == pdPASS);

    vTaskStartScheduler();

    return 0;
}
