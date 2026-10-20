#include <ble_types.h>
#include <ble_multichunk_handler.h>
#include <esp_rom_crc.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TEST_OPCODE                 0x42
#define TEST_REQ_APP                0x17
#define TEST_PAYLOAD_SIZE           40U
#define TEST_FIRST_CHUNK_SIZE       10U
#define TEST_CRC_SIZE               4U

#define MULTICHUNK_HEADER_SIZE      6U
#define MULTICHUNK_CHUNK_ID_SIZE    1U
#define CRC_CONTINUATION            0U

static uint8_t captured_data[BLE_MAX_MULTICHUNK_SIZE];
static uint16_t captured_len;
static ble_opcode_t captured_opcode;
static uint8_t captured_req_app;
static uint32_t handler_call_count;

static void capture_response(const ble_multichunk_msg_t *msg)
{
    assert(msg != NULL);
    assert(msg->len <= sizeof(captured_data));

    captured_opcode = msg->opcode;
    captured_req_app = msg->req_app;
    captured_len = msg->len;

    if (msg->len > 0U) {
        assert(msg->data != NULL);
        memcpy(captured_data, msg->data, msg->len);
    }

    handler_call_count++;

    free(msg->data);
}

static void reset_environment(void)
{
    parse_new_chunk(NULL, NULL);

    memset(captured_data, 0, sizeof(captured_data));
    captured_len = 0U;
    captured_opcode = 0U;
    captured_req_app = 0U;
    handler_call_count = 0U;
}

static void make_payload(uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0U; i < len; i++) {
        data[i] = (uint8_t)((i * 37U + 13U) & 0xFFU);
    }
}

static void feed_chunk(const uint8_t *payload,
                       uint8_t len,
                       uint8_t req_app)
{
    ble_msg_t msg = {0};

    assert(payload != NULL);
    assert(len <= BLE_MAX_PAYLOAD_SIZE);

    msg.hdr.req_app = req_app;
    msg.hdr.len = len;

    if (len > 0U) {
        memcpy(msg.payload, payload, len);
    }

    parse_new_chunk(&msg, capture_response);
}

static void assert_response(const uint8_t *expected_data,
                            uint16_t expected_len,
                            ble_opcode_t expected_opcode,
                            uint8_t expected_req_app)
{
    assert(handler_call_count == 1U);
    assert(captured_len == expected_len);
    assert(captured_opcode == expected_opcode);
    assert(captured_req_app == expected_req_app);

    if (expected_len > 0U) {
        assert(memcmp(captured_data, expected_data, expected_len) == 0);
    }
}

/*
 * Sends a multichunk transfer. The first chunk may contain a smaller
 * amount of data than its maximum capacity.
 *
 * If crc_inline is nonzero, that many CRC bytes are appended to the
 * final data chunk. Remaining CRC bytes are sent in continuation
 * messages without a chunk ID.
 */
static void send_transfer(const uint8_t *data,
                          uint16_t len,
                          uint8_t first_chunk_size,
                          bool crc_inline,
                          const uint8_t *crc_continuation_sizes,
                          uint8_t crc_continuation_count,
                          uint8_t req_app)
{
    uint8_t crc_bytes[TEST_CRC_SIZE];
    uint8_t payload[BLE_MAX_PAYLOAD_SIZE];

    assert(data != NULL);
    assert(len > 0U);
    assert(len <= BLE_MAX_MULTICHUNK_SIZE);
    assert(first_chunk_size > 0U);
    assert(first_chunk_size <= BLE_MAX_PAYLOAD_SIZE -
                               MULTICHUNK_HEADER_SIZE);

    uint16_t first_data_count =
        len < first_chunk_size ? len : first_chunk_size;

    uint16_t remaining = len - first_data_count;

    uint16_t total_chunks =
        1U + (remaining + (BLE_MAX_PAYLOAD_SIZE -
                           MULTICHUNK_CHUNK_ID_SIZE - 1U)) /
             (BLE_MAX_PAYLOAD_SIZE - MULTICHUNK_CHUNK_ID_SIZE);

    assert(total_chunks <= UINT8_MAX);

    uint32_t crc = esp_rom_crc32_le(0U, data, len);

    crc_bytes[0] = (uint8_t)((crc >> 24) & 0xFFU);
    crc_bytes[1] = (uint8_t)((crc >> 16) & 0xFFU);
    crc_bytes[2] = (uint8_t)((crc >> 8) & 0xFFU);
    crc_bytes[3] = (uint8_t)(crc & 0xFFU);

    uint16_t data_offset = 0U;
    uint8_t chunk_id = 0U;

    while (data_offset < len) {
        memset(payload, 0, sizeof(payload));

        uint16_t header_size;
        uint16_t data_count;
        uint16_t message_len;

        if (chunk_id == 0U) {
            header_size = MULTICHUNK_HEADER_SIZE;

            data_count = len - data_offset;
            if (data_count > first_chunk_size) {
                data_count = first_chunk_size;
            }

            payload[0] = BLE_MULTICHUNK_MAGIC;
            payload[1] = (uint8_t)TEST_OPCODE;
            payload[2] = (uint8_t)total_chunks;
            payload[3] = (uint8_t)((len >> 8) & 0xFFU);
            payload[4] = (uint8_t)(len & 0xFFU);
            payload[5] = chunk_id;
        } else {
            header_size = MULTICHUNK_CHUNK_ID_SIZE;

            data_count = len - data_offset;
            if (data_count > BLE_MAX_PAYLOAD_SIZE -
                             MULTICHUNK_CHUNK_ID_SIZE) {
                data_count = BLE_MAX_PAYLOAD_SIZE -
                             MULTICHUNK_CHUNK_ID_SIZE;
            }

            payload[0] = chunk_id;
        }

        memcpy(&payload[header_size], &data[data_offset], data_count);

        message_len = header_size + data_count;
        data_offset += data_count;

        bool final_chunk = (data_offset == len);

        if (final_chunk && crc_inline) {
            uint16_t inline_crc_count = TEST_CRC_SIZE;

            if (crc_continuation_count > 0U) {
                inline_crc_count = crc_continuation_sizes[0];

                assert(inline_crc_count <= TEST_CRC_SIZE);
            }

            assert(message_len + inline_crc_count <= sizeof(payload));

            memcpy(&payload[message_len], crc_bytes, inline_crc_count);
            message_len += inline_crc_count;

            feed_chunk(payload, (uint8_t)message_len, req_app);

            if (inline_crc_count < TEST_CRC_SIZE) {
                uint8_t crc_offset = (uint8_t)inline_crc_count;

                for (uint8_t i = 1U;
                     i < crc_continuation_count;
                     i++) {
                    uint8_t continuation_len =
                        crc_continuation_sizes[i];

                    assert(continuation_len > 0U);
                    assert(crc_offset + continuation_len <=
                           TEST_CRC_SIZE);

                    feed_chunk(&crc_bytes[crc_offset],
                               continuation_len,
                               req_app);

                    crc_offset += continuation_len;
                }

                if (crc_offset < TEST_CRC_SIZE) {
                    feed_chunk(&crc_bytes[crc_offset],
                               (uint8_t)(TEST_CRC_SIZE - crc_offset),
                               req_app);
                }

                assert(crc_offset <= TEST_CRC_SIZE);
            }
        } else {
            feed_chunk(payload, (uint8_t)message_len, req_app);
        }

        chunk_id++;
    }

    if (!crc_inline) {
        uint8_t crc_offset = 0U;

        for (uint8_t i = 0U; i < crc_continuation_count; i++) {
            uint8_t continuation_len = crc_continuation_sizes[i];

            assert(continuation_len > 0U);
            assert(crc_offset + continuation_len <= TEST_CRC_SIZE);

            feed_chunk(&crc_bytes[crc_offset],
                       continuation_len,
                       req_app);

            crc_offset += continuation_len;
        }

        if (crc_offset < TEST_CRC_SIZE) {
            feed_chunk(&crc_bytes[crc_offset],
                       (uint8_t)(TEST_CRC_SIZE - crc_offset),
                       req_app);
        }

        assert(crc_offset <= TEST_CRC_SIZE);
    }
}

static void test_single_chunk_transfer(void)
{
    printf("[TEST] test_single_chunk_transfer\n");

    reset_environment();

    uint8_t data[TEST_PAYLOAD_SIZE];
    make_payload(data, sizeof(data));

    send_transfer(data,
                  sizeof(data),
                  TEST_FIRST_CHUNK_SIZE,
                  true,
                  NULL,
                  0U,
                  TEST_REQ_APP);

    assert_response(data,
                    sizeof(data),
                    TEST_OPCODE,
                    TEST_REQ_APP);
}

static void test_multiple_chunk_transfer(void)
{
    printf("[TEST] test_multiple_chunk_transfer\n");

    reset_environment();

    static uint8_t data[600];
    make_payload(data, sizeof(data));

    send_transfer(data,
                  sizeof(data),
                  TEST_FIRST_CHUNK_SIZE,
                  false,
                  NULL,
                  0U,
                  TEST_REQ_APP);

    assert_response(data,
                    sizeof(data),
                    TEST_OPCODE,
                    TEST_REQ_APP);
}

static void test_crc_split_into_single_bytes(void)
{
    printf("[TEST] test_crc_split_into_single_bytes\n");

    reset_environment();

    uint8_t data[TEST_PAYLOAD_SIZE];
    const uint8_t crc_sizes[] = {1U, 1U, 1U, 1U};

    make_payload(data, sizeof(data));

    send_transfer(data,
                  sizeof(data),
                  TEST_FIRST_CHUNK_SIZE,
                  false,
                  crc_sizes,
                  sizeof(crc_sizes),
                  TEST_REQ_APP);

    assert_response(data,
                    sizeof(data),
                    TEST_OPCODE,
                    TEST_REQ_APP);
}

static void test_crc_split_across_data_and_continuation(void)
{
    printf("[TEST] test_crc_split_across_data_and_continuation\n");

    reset_environment();

    uint8_t data[TEST_PAYLOAD_SIZE];
    const uint8_t crc_sizes[] = {1U, 2U};

    make_payload(data, sizeof(data));

    send_transfer(data,
                  sizeof(data),
                  TEST_FIRST_CHUNK_SIZE,
                  true,
                  crc_sizes,
                  sizeof(crc_sizes),
                  TEST_REQ_APP);

    assert_response(data,
                    sizeof(data),
                    TEST_OPCODE,
                    TEST_REQ_APP);
}

static void test_crc_mismatch(void)
{
    printf("[TEST] test_crc_mismatch\n");

    reset_environment();

    uint8_t data[TEST_PAYLOAD_SIZE];
    uint8_t payload[BLE_MAX_PAYLOAD_SIZE];

    make_payload(data, sizeof(data));

    payload[0] = BLE_MULTICHUNK_MAGIC;
    payload[1] = (uint8_t)TEST_OPCODE;
    payload[2] = 1U;
    payload[3] = 0U;
    payload[4] = sizeof(data);
    payload[5] = 0U;

    memcpy(&payload[MULTICHUNK_HEADER_SIZE], data, sizeof(data));

    uint16_t message_len = MULTICHUNK_HEADER_SIZE + sizeof(data);

    uint32_t crc = esp_rom_crc32_le(0U, data, sizeof(data));
    crc ^= 0x00000001U;

    payload[message_len++] = (uint8_t)((crc >> 24) & 0xFFU);
    payload[message_len++] = (uint8_t)((crc >> 16) & 0xFFU);
    payload[message_len++] = (uint8_t)((crc >> 8) & 0xFFU);
    payload[message_len++] = (uint8_t)(crc & 0xFFU);

    feed_chunk(payload, (uint8_t)message_len, TEST_REQ_APP);

    assert(handler_call_count == 0U);
}

static void test_invalid_header_size(void)
{
    printf("[TEST] test_invalid_header_size\n");

    reset_environment();

    uint8_t payload[MULTICHUNK_HEADER_SIZE] = {
        BLE_MULTICHUNK_MAGIC,
        (uint8_t)TEST_OPCODE,
        1U,
        0U,
        0U,
        0U
    };

    feed_chunk(payload, sizeof(payload), TEST_REQ_APP);

    assert(handler_call_count == 0U);
}

static void test_wrong_chunk_id(void)
{
    printf("[TEST] test_wrong_chunk_id\n");

    reset_environment();

    uint8_t data[600];
    uint8_t payload[BLE_MAX_PAYLOAD_SIZE];

    make_payload(data, sizeof(data));

    payload[0] = BLE_MULTICHUNK_MAGIC;
    payload[1] = (uint8_t)TEST_OPCODE;
    payload[2] = 2U;
    payload[3] = (uint8_t)((sizeof(data) >> 8) & 0xFFU);
    payload[4] = (uint8_t)(sizeof(data) & 0xFFU);
    payload[5] = 0U;

    memcpy(&payload[MULTICHUNK_HEADER_SIZE],
           data,
           TEST_FIRST_CHUNK_SIZE);

    feed_chunk(payload,
               MULTICHUNK_HEADER_SIZE + TEST_FIRST_CHUNK_SIZE,
               TEST_REQ_APP);

    memset(payload, 0, sizeof(payload));
    payload[0] = 2U;

    memcpy(&payload[1],
           &data[TEST_FIRST_CHUNK_SIZE],
           BLE_MAX_PAYLOAD_SIZE - 1U);

    feed_chunk(payload,
               BLE_MAX_PAYLOAD_SIZE,
               TEST_REQ_APP);

    assert(handler_call_count == 0U);
}

static void test_reset_incomplete_transfer(void)
{
    printf("[TEST] test_reset_incomplete_transfer\n");

    reset_environment();

    uint8_t data[600];
    uint8_t payload[BLE_MAX_PAYLOAD_SIZE];

    make_payload(data, sizeof(data));

    payload[0] = BLE_MULTICHUNK_MAGIC;
    payload[1] = (uint8_t)TEST_OPCODE;
    payload[2] = 4U;
    payload[3] = (uint8_t)((sizeof(data) >> 8) & 0xFFU);
    payload[4] = (uint8_t)(sizeof(data) & 0xFFU);
    payload[5] = 0U;

    memcpy(&payload[MULTICHUNK_HEADER_SIZE],
           data,
           TEST_FIRST_CHUNK_SIZE);

    feed_chunk(payload,
               MULTICHUNK_HEADER_SIZE + TEST_FIRST_CHUNK_SIZE,
               TEST_REQ_APP);

    assert(handler_call_count == 0U);

    reset_environment();

    uint8_t valid_data[TEST_PAYLOAD_SIZE];
    make_payload(valid_data, sizeof(valid_data));

    send_transfer(valid_data,
                  sizeof(valid_data),
                  TEST_FIRST_CHUNK_SIZE,
                  true,
                  NULL,
                  0U,
                  TEST_REQ_APP);

    assert_response(valid_data,
                    sizeof(valid_data),
                    TEST_OPCODE,
                    TEST_REQ_APP);
}

static void test_maximum_payload_transfer(void)
{
    printf("[TEST] test_maximum_payload_transfer\n");

    reset_environment();

    static uint8_t data[BLE_MAX_MULTICHUNK_SIZE];
    make_payload(data, sizeof(data));

    send_transfer(data,
                  sizeof(data),
                  BLE_MAX_PAYLOAD_SIZE -
                      MULTICHUNK_HEADER_SIZE - TEST_CRC_SIZE,
                  false,
                  NULL,
                  0U,
                  TEST_REQ_APP);

    assert_response(data,
                    sizeof(data),
                    TEST_OPCODE,
                    TEST_REQ_APP);
}

static void test_incomplete_crc_reset(void)
{
    printf("[TEST] test_incomplete_crc_reset\n");

    reset_environment();

    uint8_t data[TEST_PAYLOAD_SIZE];
    uint8_t payload[BLE_MAX_PAYLOAD_SIZE];

    make_payload(data, sizeof(data));

    payload[0] = BLE_MULTICHUNK_MAGIC;
    payload[1] = (uint8_t)TEST_OPCODE;
    payload[2] = 1U;
    payload[3] = 0U;
    payload[4] = sizeof(data);
    payload[5] = 0U;

    memcpy(&payload[MULTICHUNK_HEADER_SIZE], data, sizeof(data));

    uint16_t message_len = MULTICHUNK_HEADER_SIZE + sizeof(data);

    uint32_t crc = esp_rom_crc32_le(0U, data, sizeof(data));

    payload[message_len++] = (uint8_t)((crc >> 24) & 0xFFU);
    payload[message_len++] = (uint8_t)((crc >> 16) & 0xFFU);

    feed_chunk(payload, (uint8_t)message_len, TEST_REQ_APP);

    assert(handler_call_count == 0U);

    reset_environment();

    send_transfer(data,
                  sizeof(data),
                  TEST_FIRST_CHUNK_SIZE,
                  true,
                  NULL,
                  0U,
                  TEST_REQ_APP);

    assert_response(data,
                    sizeof(data),
                    TEST_OPCODE,
                    TEST_REQ_APP);
}

static void test_malformed_input_recovery(void)
{
    printf("[TEST] test_malformed_input_recovery\n");

    reset_environment();

    uint32_t seed = 0x12345678U;

    for (uint16_t i = 0U; i < 1000U; i++) {
        uint8_t payload[BLE_MAX_PAYLOAD_SIZE];

        seed = seed * 1664525U + 1013904223U;

        uint8_t len = (uint8_t)(seed % (BLE_MAX_PAYLOAD_SIZE + 1U));

        for (uint16_t j = 0U; j < len; j++) {
            seed = seed * 1664525U + 1013904223U;
            payload[j] = (uint8_t)(seed >> 24);
        }

        parse_new_chunk(NULL, NULL);
        feed_chunk(payload, len, TEST_REQ_APP);

        assert(handler_call_count == 0U);
    }

    reset_environment();

    uint8_t data[TEST_PAYLOAD_SIZE];
    make_payload(data, sizeof(data));

    send_transfer(data,
                  sizeof(data),
                  TEST_FIRST_CHUNK_SIZE,
                  true,
                  NULL,
                  0U,
                  TEST_REQ_APP);

    assert_response(data,
                    sizeof(data),
                    TEST_OPCODE,
                    TEST_REQ_APP);
}

static void test_task(void *arg)
{
    (void)arg;

    test_single_chunk_transfer();
    test_multiple_chunk_transfer();
    test_crc_split_into_single_bytes();
    test_crc_split_across_data_and_continuation();
    test_crc_mismatch();
    test_invalid_header_size();
    test_wrong_chunk_id();
    test_reset_incomplete_transfer();
    test_maximum_payload_transfer();
    test_incomplete_crc_reset();
    test_malformed_input_recovery();

    printf("\nAll multichunk handler tests passed.\n");

    exit(0);
}

int main(void)
{
    xTaskCreate(test_task,
                "ble_multichunk_test",
                8192,
                NULL,
                5,
                NULL);

    vTaskStartScheduler();

    return 0;
}
