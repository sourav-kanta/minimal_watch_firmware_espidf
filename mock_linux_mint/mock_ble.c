#include <stdio.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_log.h"
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <runtime_manager.h>
#include <ble_request_handler.h>
#include <ble_response_handler.h>
#include <ble_types.h>
#include <ble_consts.h>
#include <ble_packet_handler.h>
#include <ble_fifo.h>
#include <driver/uart.h>
#include <event_manager.h>

#define SERIAL_FRAME_MAGIC     0x7E
#define SERIAL_MAX_PAYLOAD     512
#define MOCK_UART_NUM          UART_NUM_1

static TaskHandle_t serial_task_handle = NULL;

static void write_serial_frame(uint8_t* src, uint16_t len) {
    uint8_t serial_buf[1024];
    serial_buf[0] = SERIAL_FRAME_MAGIC;
    serial_buf[2] = (len >> 8) & 0xFF;
    serial_buf[1] = len & 0xFF;
    memcpy(&serial_buf[3], src, len);
    uart_write_bytes(MOCK_UART_NUM, serial_buf, len+3);
}

static void serial_receive_task(void *arg)
{
    uint8_t buffer[512];
    uint8_t frame[SERIAL_MAX_PAYLOAD];

    size_t frame_pos = 0;
    size_t frame_len = 0;
    bool receiving = false;

    printf("[QEMU MOCK] Serial receiver started\n");

    while (1) {
        int count = uart_read_bytes(
            MOCK_UART_NUM,
            buffer,
            sizeof(buffer),
            pdMS_TO_TICKS(1000)
        );
        if(count == 0) continue;
        ESP_LOGI("BLE Mock", "Read %d bytes on UART1", count);
        for (int i = 0; i < count; i++) {
            uint8_t byte = buffer[i];

            if (!receiving) {
                if (byte == SERIAL_FRAME_MAGIC) {
                    receiving = true;
                    frame_pos = 0;
                    frame_len = 0;
                }
                continue;
            }

            if (frame_pos < 2) {
                frame_len |= ((size_t)byte << (frame_pos * 8));
                frame_pos++;

                if (frame_pos == 2 && frame_len > SERIAL_MAX_PAYLOAD) {
                    ESP_LOGW("QEMU_MOCK", "Invalid serial frame length: %u",
                             (unsigned)frame_len);
                    receiving = false;
                }

                continue;
            }

            frame[frame_pos - 2] = byte;
            frame_pos++;

            if (frame_pos == frame_len + 2) {
                ble_retrieve_packet(frame, frame_len);
                ESP_LOGI("BLE Mock", "Received a serial packet of len = %d", frame_len);
                receiving = false;
                frame_pos = 0;
                frame_len = 0;
            }
        }
    }
}

void mock_serial_init(void)
{
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_param_config(MOCK_UART_NUM, &uart_config));

    ESP_ERROR_CHECK(uart_driver_install(
        MOCK_UART_NUM,
        512,
        0,
        0,
        NULL,
        0
    ));
    xTaskCreate(
        serial_receive_task,
        "mock_serial_rx",
        4096,
        NULL,
        2,
        &serial_task_handle
    );

    printf("[QEMU MOCK] Serial receiver initialized\n");
}

static void populate_tx_buffers(void) {
    uint8_t tx_buf[256];
    uint16_t len;
    while (ble_build_next_packet(tx_buf, 244, &len)) {
        ESP_LOGI("BLE Mock", "New packet to be sent = %d", len);
        write_serial_frame(tx_buf, len);
    }
}

static void populate_rx_buffers(void) {
    ble_msg_t msg;
    while(get_next_ble_msg(&msg, BLE_RX)) {
        handle_ble_response(&msg);
    }
}

static void submit_ble_request(const event_t* event) {
    if(!event) return;
    assert(event->payload_len == sizeof(ble_req_t));
    handle_ble_request((ble_req_t*)event->data);
}

static void mock_serial_deinit(void) {
    if(serial_task_handle) {
        vTaskDelete(serial_task_handle);
        serial_task_handle = NULL;
        printf("[QEMU MOCK] Serial receiver deinitialized\n");
    }
}

void __wrap_ble_manager_init(void) {
    ble_fifo_init();
    mock_serial_init();
    event_subscribe(EVENT_BLE_REQUEST, submit_ble_request);
    runtime_manager_register_hook(populate_tx_buffers);
    runtime_manager_register_hook(populate_rx_buffers);
    printf("[QEMU MOCK] BLE driver bypassed successfully.\n");
}

void __wrap_ble_manager_deinit(void) {
    mock_serial_deinit();
    ble_fifo_deinit();
    printf("[QEMU MOCK] BLE driver bypassed successfully.\n");
}
