#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <ble_consts.h>
#include <ble_fifo.h>
#include <ble_types.h>
#include <ble_packet_handler.h>

#define TEST_MTU_MIN       23
#define TEST_MTU_64        64
#define TEST_MTU_128       128
#define TEST_MTU_244       244
#define TEST_MTU_247       247

static void reset_environment(void) {
    ble_fifo_deinit();

    assert(ble_fifo_init());

    rx_reset_context();
    tx_reset_context();
}

static ble_msg_t make_message(uint8_t opcode,
                              uint8_t req_app,
                              uint8_t len,
                              uint8_t seed) {
    ble_msg_t msg = {0};

    msg.hdr.opcode = opcode;
    msg.hdr.req_app = req_app;
    msg.hdr.len = len;

    for (uint16_t i = 0; i < len; i++) {
        msg.payload[i] = (uint8_t)(seed + i);
    }

    return msg;
}

static void assert_message_equal(const ble_msg_t *a,
                                 const ble_msg_t *b) {
    assert(a->hdr.opcode == b->hdr.opcode);
    assert(a->hdr.req_app == b->hdr.req_app);
    assert(a->hdr.len == b->hdr.len);
    assert(memcmp(a->payload,
                  b->payload,
                  a->hdr.len) == 0);
}

static void assert_rx_empty(void) {
    ble_msg_t msg;

    assert(!get_next_ble_msg(&msg, BLE_RX));
}

static void assert_tx_empty(void) {
    ble_msg_t msg;

    assert(!get_next_ble_msg(&msg, BLE_TX));
}

/*
 * Test: TX with an empty FIFO.
 *
 * Verifies that no packet is produced when the TX FIFO has no messages.
 */
static void test_tx_empty_fifo(void) {
    uint8_t packet[247] = {0};
    uint16_t len = 0;

    printf("test_tx_empty_fifo...\n");

    reset_environment();

    assert(!ble_build_next_packet(packet, TEST_MTU_MIN, &len));
    assert(len == 1);
    assert(packet[0] == 0);

    printf("  PASS\n");
}

/*
 * Test: TX message with an empty payload.
 *
 * Verifies that a header-only message is encoded correctly.
 */
static void test_tx_empty_payload(void) {
    uint8_t packet[247] = {0};
    uint16_t len = 0;

    ble_msg_t msg = make_message(0x10, 0x20, 0, 0);

    printf("test_tx_empty_payload...\n");

    reset_environment();

    assert(add_ble_msg_to_queue(&msg, BLE_TX));
    assert(ble_build_next_packet(packet, TEST_MTU_MIN, &len));
    assert(len == 5);
    assert(packet[0] == 0);
    assert(packet[1] == BLE_MAGIC);
    assert(packet[2] == msg.hdr.opcode);
    assert(packet[3] == msg.hdr.req_app);
    assert(packet[4] == msg.hdr.len);
    assert_tx_empty();

    printf("  PASS\n");
}

/*
 * Test: TX small message.
 *
 * Verifies that a message that fits in one packet is encoded correctly.
 */
static void test_tx_small_message(void) {
    uint8_t packet[247] = {0};
    uint16_t len = 0;

    ble_msg_t msg = make_message(0x11, 0x22, 5, 0x30);

    printf("test_tx_small_message...\n");

    reset_environment();

    assert(add_ble_msg_to_queue(&msg, BLE_TX));
    assert(ble_build_next_packet(packet, TEST_MTU_MIN, &len));
    assert(len == 10);
    assert(packet[0] == 0);
    assert(packet[1] == BLE_MAGIC);
    assert(packet[2] == msg.hdr.opcode);
    assert(packet[3] == msg.hdr.req_app);
    assert(packet[4] == msg.hdr.len);
    assert(memcmp(&packet[5], msg.payload, msg.hdr.len) == 0);
    assert_tx_empty();

    printf("  PASS\n");
}

/*
 * Test: TX fragmented message.
 *
 * Verifies that a message larger than the MTU is split across packets
 * and that the continuation packet contains the remaining payload.
 */
static void test_tx_fragmented_message(void) {
    uint8_t packet[247] = {0};
    uint16_t len = 0;

    ble_msg_t msg = make_message(0x40, 0x41, 40, 0x50);

    printf("test_tx_fragmented_message...\n");

    reset_environment();

    assert(add_ble_msg_to_queue(&msg, BLE_TX));
    assert(ble_build_next_packet(packet, TEST_MTU_MIN, &len));
    assert(len == 23);
    assert(packet[0] == 0);
    assert(packet[1] == BLE_MAGIC);
    assert(packet[2] == msg.hdr.opcode);
    assert(packet[3] == msg.hdr.req_app);
    assert(packet[4] == msg.hdr.len);
    assert(memcmp(&packet[5], msg.payload, 18) == 0);

    assert(ble_build_next_packet(packet, TEST_MTU_MIN, &len));
    assert(len == 23);
    assert(packet[0] == 1);
    assert(memcmp(&packet[1], &msg.payload[18], 22) == 0);
    assert_tx_empty();

    printf("  PASS\n");
}

/*
 * Test: TX multiple messages in one packet.
 *
 * Verifies that multiple complete messages are packed sequentially when
 * enough MTU space is available.
 */
static void test_tx_multiple_messages(void) {
    uint8_t packet[247] = {0};
    uint16_t len = 0;

    ble_msg_t msg1 = make_message(0x01, 1, 3, 0x10);
    ble_msg_t msg2 = make_message(0x02, 2, 4, 0x20);
    ble_msg_t msg3 = make_message(0x03, 3, 2, 0x30);

    printf("test_tx_multiple_messages...\n");

    reset_environment();

    assert(add_ble_msg_to_queue(&msg1, BLE_TX));
    assert(add_ble_msg_to_queue(&msg2, BLE_TX));
    assert(add_ble_msg_to_queue(&msg3, BLE_TX));

    assert(ble_build_next_packet(packet, TEST_MTU_64, &len));
    assert(packet[0] == 0);

    uint16_t i = 1;

    assert(packet[i++] == BLE_MAGIC);
    assert(packet[i++] == msg1.hdr.opcode);
    assert(packet[i++] == msg1.hdr.req_app);
    assert(packet[i++] == msg1.hdr.len);
    assert(memcmp(&packet[i], msg1.payload, 3) == 0);

    i += 3;

    assert(packet[i++] == BLE_MAGIC);
    assert(packet[i++] == msg2.hdr.opcode);
    assert(packet[i++] == msg2.hdr.req_app);
    assert(packet[i++] == msg2.hdr.len);
    assert(memcmp(&packet[i], msg2.payload, 4) == 0);

    i += 4;

    assert(packet[i++] == BLE_MAGIC);
    assert(packet[i++] == msg3.hdr.opcode);
    assert(packet[i++] == msg3.hdr.req_app);
    assert(packet[i++] == msg3.hdr.len);
    assert(memcmp(&packet[i], msg3.payload, 2) == 0);

    i += 2;

    assert(len == i);
    assert_tx_empty();

    printf("  PASS\n");
}

/*
 * Test: RX single complete message.
 *
 * Verifies that one complete incoming packet is decoded into the RX FIFO.
 */
static void test_rx_single_message(void) {
    uint8_t packet[32];

    ble_msg_t msg;
    ble_msg_t received;

    printf("test_rx_single_message...\n");

    reset_environment();

    msg = make_message(0x01, 0x02, 5, 0x10);

    packet[0] = 0;
    packet[1] = BLE_MAGIC;
    packet[2] = msg.hdr.opcode;
    packet[3] = msg.hdr.req_app;
    packet[4] = msg.hdr.len;

    memcpy(&packet[5], msg.payload, msg.hdr.len);

    ble_retrieve_packet(packet, 10);
    assert(get_next_ble_msg(&received, BLE_RX));
    assert_message_equal(&msg, &received);
    assert_rx_empty();

    printf("  PASS\n");
}

/*
 * Test: RX fragmented payload.
 *
 * Verifies that a message split across two incoming packets is correctly
 * reassembled.
 */
static void test_rx_payload_fragmentation(void) {
    uint8_t packet[32];

    ble_msg_t msg;
    ble_msg_t received;

    printf("test_rx_payload_fragmentation...\n");

    reset_environment();

    msg = make_message(0x51, 0x52, 20, 0x60);

    /*
     * First fragment contains the complete header and the first 7
     * payload bytes.
     */
    packet[0] = 0;
    packet[1] = BLE_MAGIC;
    packet[2] = msg.hdr.opcode;
    packet[3] = msg.hdr.req_app;
    packet[4] = msg.hdr.len;

    memcpy(&packet[5], msg.payload, 7);

    ble_retrieve_packet(packet, 12);
    assert_rx_empty();

    /*
     * Second fragment contains the remaining 13 payload bytes.
     */
    packet[0] = 1;

    memcpy(&packet[1], &msg.payload[7], 13);

    ble_retrieve_packet(packet, 14);
    assert(get_next_ble_msg(&received, BLE_RX));
    assert_message_equal(&msg, &received);
    assert_rx_empty();

    printf("  PASS\n");
}

/*
 * Test: RX packet ending immediately after the magic byte.
 *
 * Verifies that an incomplete header is retained correctly until the next
 * packet completes it.
 */
static void test_rx_magic_at_end_of_packet(void) {
    uint8_t packet[32];

    ble_msg_t msg;
    ble_msg_t received;

    printf("test_rx_magic_at_end_of_packet...\n");

    reset_environment();

    packet[0] = 0;
    packet[1] = BLE_MAGIC;

    ble_retrieve_packet(packet, 2);
    assert_rx_empty();

    msg = make_message(0xC1, 1, 3, 0x20);

    packet[0] = 0;
    packet[1] = BLE_MAGIC;
    packet[2] = msg.hdr.opcode;
    packet[3] = msg.hdr.req_app;
    packet[4] = msg.hdr.len;

    memcpy(&packet[5], msg.payload, 3);

    ble_retrieve_packet(packet, 8);

    assert(get_next_ble_msg(&received, BLE_RX));
    assert_message_equal(&msg, &received);

    printf("  PASS\n");
}

/*
 * Test: RX magic followed by an invalid payload length.
 *
 * Verifies that messages exceeding the maximum payload size are rejected.
 */
static void test_rx_invalid_length(void) {
    uint8_t packet[32];

    printf("test_rx_invalid_length...\n");

    reset_environment();

    packet[0] = 0;
    packet[1] = BLE_MAGIC;
    packet[2] = 0x11;
    packet[3] = 0x22;
    packet[4] = BLE_MAX_PAYLOAD_SIZE + 1;

    ble_retrieve_packet(packet, 5);

    assert_rx_empty();

    printf("  PASS\n");
}

/*
 * Test: RX unexpected continuation packet.
 *
 * Verifies that a complete message following an unexpected continuation
 * marker can still be recovered safely.
 */
static void test_rx_unexpected_continuation(void) {
    printf("test_rx_unexpected_continuation...\n");

    reset_environment();

    uint8_t packet[] = {
        1,

        BLE_MAGIC,
        0x10,
        1,
        2,
        0xAA,
        0xAB
    };

    ble_retrieve_packet(packet, sizeof(packet));

    ble_msg_t received;
    assert(get_next_ble_msg(&received, BLE_RX));

    ble_msg_t expected = make_message(0x10, 1, 2, 0xAA);

    assert_message_equal(&received, &expected);
    assert_rx_empty();

    printf("  PASS\n");
}

/*
 * Test: RX context reset.
 *
 * Verifies that a partially received message is discarded when the RX
 * context is reset and does not contaminate the next message.
 */
static void test_rx_reset(void) {
    uint8_t packet[32];

    ble_msg_t msg;
    ble_msg_t received;

    printf("test_rx_reset...\n");

    reset_environment();

    msg = make_message(0x21, 1, 10, 0x30);

    packet[0] = 0;
    packet[1] = BLE_MAGIC;
    packet[2] = msg.hdr.opcode;
    packet[3] = msg.hdr.req_app;
    packet[4] = msg.hdr.len;

    memcpy(&packet[5], msg.payload, 4);

    ble_retrieve_packet(packet, 9);
    assert_rx_empty();

    rx_reset_context();

    packet[0] = 1;

    memcpy(&packet[1], &msg.payload[4], 6);

    ble_retrieve_packet(packet, 7);
    assert_rx_empty();

    msg = make_message(0x22, 2, 3, 0x80);

    packet[0] = 0;
    packet[1] = BLE_MAGIC;
    packet[2] = msg.hdr.opcode;
    packet[3] = msg.hdr.req_app;
    packet[4] = msg.hdr.len;

    memcpy(&packet[5], msg.payload, 3);

    ble_retrieve_packet(packet, 8);

    assert(get_next_ble_msg(&received, BLE_RX));
    assert_message_equal(&msg, &received);

    printf("  PASS\n");
}

/*
 * Test: TX/RX round trip at minimum MTU.
 *
 * Verifies that a large message can be fragmented into multiple packets,
 * passed through the RX parser, and reconstructed without corruption.
 */
static void test_round_trip(void) {
    uint8_t packet[247];

    ble_msg_t original;
    ble_msg_t received;

    printf("test_round_trip...\n");

    reset_environment();

    original = make_message(0x70, 0x71, 250, 0x10);

    assert(add_ble_msg_to_queue(&original, BLE_TX));

    bool first_packet = true;
    uint16_t safety_counter = 0;

    while (true) {
        uint16_t len = 0;

        bool result =
            ble_build_next_packet(packet, TEST_MTU_MIN, &len);

        if (!result) {
            break;
        }

        assert(len <= TEST_MTU_MIN);
        assert(len >= 1);

        if (first_packet) {
            assert(packet[0] == 0);
            first_packet = false;
        }
        else {
            assert(packet[0] == 1);
        }

        ble_retrieve_packet(packet, len);

        safety_counter++;
        assert(safety_counter < 100);
    }

    assert(get_next_ble_msg(&received, BLE_RX));
    assert_message_equal(&original, &received);
    assert_rx_empty();
    assert_tx_empty();

    printf("  PASS\n");
}

/*
 * Test: TX/RX round trip across the supported MTU range.
 *
 * Verifies fragmentation and reassembly at each supported MTU size.
 */
static void test_round_trip_mtu_matrix(void) {
    const uint16_t mtus[] = {
        TEST_MTU_MIN,
        TEST_MTU_64,
        TEST_MTU_128,
        TEST_MTU_244,
        TEST_MTU_247
    };

    printf("test_round_trip_mtu_matrix...\n");

    for (size_t i = 0;
         i < sizeof(mtus) / sizeof(mtus[0]);
         i++) {
        uint8_t packet[247];

        ble_msg_t original;
        ble_msg_t received;

        reset_environment();

        original = make_message(
            (uint8_t)(0x80 + i),
            (uint8_t)(0x90 + i),
            250,
            (uint8_t)(0x10 + i)
        );

        assert(add_ble_msg_to_queue(&original, BLE_TX));

        bool first_packet = true;
        uint16_t safety_counter = 0;

        while (true) {
            uint16_t len = 0;

            bool result =
                ble_build_next_packet(packet, mtus[i], &len);

            if (!result) {
                break;
            }

            assert(len <= mtus[i]);

            if (first_packet) {
                assert(packet[0] == 0);
                first_packet = false;
            }
            else {
                assert(packet[0] == 1);
            }

            ble_retrieve_packet(packet, len);

            safety_counter++;
            assert(safety_counter < 100);
        }

        assert(get_next_ble_msg(&received, BLE_RX));
        assert_message_equal(&original, &received);
        assert_rx_empty();
        assert_tx_empty();
    }

    printf("  PASS\n");
}

/*
 * Test: packet dump safety.
 *
 * Verifies that the packet dump helper safely handles NULL, empty,
 * complete, and truncated packet buffers.
 */
static void test_dump_ble_packet_safety(void) {
    uint8_t packet[] = {
        0,
        BLE_MAGIC,
        0x01,
        0x02,
        3,
        0x10,
        0x11,
        0x12
    };

    uint8_t truncated_header[] = {
        0,
        BLE_MAGIC,
        0x01
    };

    uint8_t truncated_payload[] = {
        0,
        BLE_MAGIC,
        0x01,
        0x02,
        10,
        0x10,
        0x11
    };

    printf("test_dump_ble_packet_safety...\n");

    dump_ble_packet(NULL, 0);
    dump_ble_packet(packet, 0);
    dump_ble_packet(packet, sizeof(packet));
    dump_ble_packet(truncated_header, sizeof(truncated_header));
    dump_ble_packet(truncated_payload, sizeof(truncated_payload));

    printf("  PASS\n");
}

/*
 * Test: RX packet containing only magic and opcode.
 *
 * Verifies that an incomplete header is not emitted as a message.
 */
static void test_rx_magic_opcode_only_at_packet_end(void) {
    uint8_t packet[32];

    printf("test_rx_magic_opcode_only_at_packet_end...\n");

    reset_environment();

    packet[0] = 0;
    packet[1] = BLE_MAGIC;
    packet[2] = 0x20;

    ble_retrieve_packet(packet, 3);

    assert_rx_empty();

    printf("  PASS\n");
}

/*
 * Test: RX magic appearing near the end of a packet.
 *
 * Verifies that unrelated bytes following a magic byte are not incorrectly
 * interpreted as a complete header when insufficient bytes remain.
 */
static void test_rx_magic_with_three_bytes_remaining(void) {
    uint8_t packet[32];

    printf("test_rx_magic_with_three_bytes_remaining...\n");

    reset_environment();

    packet[0] = 0;
    packet[1] = 0x11;
    packet[2] = BLE_MAGIC;
    packet[3] = 0x20;
    packet[4] = 0x01;

    ble_retrieve_packet(packet, 5);

    assert_rx_empty();

    printf("  PASS\n");
}

/*
 * Test: RX complete zero-length message at packet end.
 *
 * Verifies that a complete header with a zero-length payload is emitted
 * correctly even when it ends exactly at the packet boundary.
 */
static void test_rx_complete_header_at_end(void) {
    uint8_t packet[32];

    ble_msg_t received;

    printf("test_rx_complete_header_at_end...\n");

    reset_environment();

    packet[0] = 0;
    packet[1] = BLE_MAGIC;
    packet[2] = 0x20;
    packet[3] = 0x01;
    packet[4] = 0;

    ble_retrieve_packet(packet, 5);

    assert(get_next_ble_msg(&received, BLE_RX));
    assert(received.hdr.opcode == 0x20);
    assert(received.hdr.req_app == 0x01);
    assert(received.hdr.len == 0);
    assert_rx_empty();

    printf("  PASS\n");
}

/*
 * Test: BLE magic inside payload.
 *
 * Verifies that the magic byte has no special meaning once the parser is
 * inside a message payload.
 */
static void test_rx_magic_inside_payload(void) {
    uint8_t packet[32];

    ble_msg_t msg;
    ble_msg_t received;

    printf("test_rx_magic_inside_payload...\n");

    reset_environment();

    msg = make_message(0x30, 0x01, 4, 0x10);
    msg.payload[1] = BLE_MAGIC;

    packet[0] = 0;
    packet[1] = BLE_MAGIC;
    packet[2] = msg.hdr.opcode;
    packet[3] = msg.hdr.req_app;
    packet[4] = msg.hdr.len;

    memcpy(&packet[5], msg.payload, msg.hdr.len);

    ble_retrieve_packet(packet, 9);

    assert(get_next_ble_msg(&received, BLE_RX));
    assert_message_equal(&msg, &received);
    assert_rx_empty();

    printf("  PASS\n");
}

/*
 * Test: TX payload exactly fills the MTU.
 *
 * Verifies the boundary case where the header and payload exactly consume
 * the available MTU.
 */
static void test_tx_mtu_exact_payload_boundary(void) {
    uint8_t packet[247] = {0};
    uint16_t len = 0;

    printf("test_tx_mtu_exact_payload_boundary...\n");

    reset_environment();

    ble_msg_t msg = make_message(0xA0, 0x01, 18, 0x40);

    assert(add_ble_msg_to_queue(&msg, BLE_TX));
    assert(ble_build_next_packet(packet, TEST_MTU_MIN, &len));

    assert(len == TEST_MTU_MIN);
    assert(packet[0] == 0);
    assert(packet[1] == BLE_MAGIC);
    assert(packet[2] == msg.hdr.opcode);
    assert(packet[3] == msg.hdr.req_app);
    assert(packet[4] == msg.hdr.len);
    assert(memcmp(&packet[5],
                  msg.payload,
                  msg.hdr.len) == 0);

    assert_tx_empty();

    printf("  PASS\n");
}

/*
 * Test: TX/RX payload containing BLE magic bytes.
 *
 * Verifies that magic bytes appearing in a payload are transmitted and
 * parsed as ordinary payload data.
 */
static void test_tx_payload_contains_magic(void) {
    uint8_t packet[247] = {0};
    uint16_t len = 0;

    printf("test_tx_payload_contains_magic...\n");

    reset_environment();

    ble_msg_t msg = make_message(0xB0, 0x02, 10, 0x20);

    msg.payload[2] = BLE_MAGIC;
    msg.payload[7] = BLE_MAGIC;

    assert(add_ble_msg_to_queue(&msg, BLE_TX));
    assert(ble_build_next_packet(packet, TEST_MTU_MIN, &len));

    assert(len == 15);
    assert(packet[0] == 0);
    assert(packet[1] == BLE_MAGIC);
    assert(packet[2] == msg.hdr.opcode);
    assert(packet[3] == msg.hdr.req_app);
    assert(packet[4] == msg.hdr.len);
    assert(memcmp(&packet[5],
                  msg.payload,
                  msg.hdr.len) == 0);

    assert_tx_empty();

    ble_retrieve_packet(packet, len);

    ble_msg_t received;
    assert(get_next_ble_msg(&received, BLE_RX));
    assert_message_equal(&msg, &received);
    assert_rx_empty();

    printf("  PASS\n");
}

static void test_task(void *arg)
{
    (void)arg;

    printf("\n");
    printf("========================================\n");
    printf("BLE PACKET HANDLER TESTS\n");
    printf("========================================\n");

    fprintf(stderr, "[TEST] test_tx_empty_fifo\n");
    test_tx_empty_fifo();

    fprintf(stderr, "[TEST] test_tx_empty_payload\n");
    test_tx_empty_payload();

    fprintf(stderr, "[TEST] test_tx_small_message\n");
    test_tx_small_message();

    fprintf(stderr, "[TEST] test_tx_fragmented_message\n");
    test_tx_fragmented_message();

    fprintf(stderr, "[TEST] test_tx_multiple_messages\n");
    test_tx_multiple_messages();

    fprintf(stderr, "[TEST] test_tx_mtu_exact_payload_boundary\n");
    test_tx_mtu_exact_payload_boundary();

    fprintf(stderr, "[TEST] test_tx_payload_contains_magic\n");
    test_tx_payload_contains_magic();

    fprintf(stderr, "[TEST] test_rx_single_message\n");
    test_rx_single_message();

    fprintf(stderr, "[TEST] test_rx_payload_fragmentation\n");
    test_rx_payload_fragmentation();

    fprintf(stderr, "[TEST] test_rx_magic_at_end_of_packet\n");
    test_rx_magic_at_end_of_packet();

    fprintf(stderr, "[TEST] test_rx_magic_opcode_only_at_packet_end\n");
    test_rx_magic_opcode_only_at_packet_end();

    fprintf(stderr, "[TEST] test_rx_magic_with_three_bytes_remaining\n");
    test_rx_magic_with_three_bytes_remaining();

    fprintf(stderr, "[TEST] test_rx_complete_header_at_end\n");
    test_rx_complete_header_at_end();

    fprintf(stderr, "[TEST] test_rx_magic_inside_payload\n");
    test_rx_magic_inside_payload();

    fprintf(stderr, "[TEST] test_rx_invalid_length\n");
    test_rx_invalid_length();

    fprintf(stderr, "[TEST] test_rx_unexpected_continuation\n");
    test_rx_unexpected_continuation();

    fprintf(stderr, "[TEST] test_rx_reset\n");
    test_rx_reset();

    fprintf(stderr, "[TEST] test_dump_ble_packet_safety\n");
    test_dump_ble_packet_safety();

    fprintf(stderr, "[TEST] test_round_trip\n");
    test_round_trip();

    fprintf(stderr, "[TEST] test_round_trip_mtu_matrix\n");
    test_round_trip_mtu_matrix();

    ble_fifo_deinit();

    printf("\n");
    printf("========================================\n");
    printf("ALL BLE PACKET HANDLER TESTS PASSED\n");
    printf("========================================\n");

    exit(0);
}

int main(void)
{
    assert(xTaskCreate(
        test_task,
        "ble_packet_tests",
        4096,
        NULL,
        1,
        NULL
    ) == pdPASS);

    vTaskStartScheduler();

    return 0;
}
