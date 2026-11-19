#include <encoder_driver.h>
#include "esp_err.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <ble_types.h>
#include <ble_response_handler.h>

static QueueHandle_t xinputQueue = NULL;
static TaskHandle_t keyboard_task_handle = NULL;

static void register_input(input_key_t key) {
    if (!xinputQueue)
        return;

    encoder_input_t evt_press = {
        .key = key,
        .state = INPUT_STATE_PRESSED
    };

    encoder_input_t evt_release = {
        .key = key,
        .state = INPUT_STATE_RELEASED
    };

    xQueueSend(xinputQueue, &evt_press, 0);
    xQueueSend(xinputQueue, &evt_release, 0);
}

void mock_send_time(void) {
    uint32_t timestamp = 1785984486;

    ble_msg_t msg = {
        .hdr = {
            .opcode = BLE_OP_TIME_UPDATE,
            .req_app = 0,
            .len = BLE_MSG_TIME_SIZE
        }
    };

    msg.payload[0] = (timestamp >> 24) & 0xff;
    msg.payload[1] = (timestamp >> 16) & 0xff;
    msg.payload[2] = (timestamp >> 8) & 0xff;
    msg.payload[3] = timestamp & 0xff;

    handle_ble_response(&msg);
}

void mock_send_weather(void) {
    ble_msg_t msg = {
        .hdr = {
            .opcode = BLE_OP_WEATHER_UPDATE,
            .req_app = 0,
            .len = BLE_MSG_WEATHER_SIZE
        }
    };

    uint8_t *p = msg.payload;
    p[0] = 0xFF;
    p[1] = 0xFF;
    p[2] = 0xFF;
    p[3] = 0xFF;

    p += 4;

    for (int i = 0; i < 24; i++) {

        int16_t temp = 285 + i*10;

        *p++ = (temp >> 8) & 0xFF;
        *p++ = temp & 0xFF;

        *p++ = 65;   // humidity
        *p++ = 10;   // precipitation probability
        *p++ = 2;    // weather code
        *p++ = 12;   // wind speed
    }

    handle_ble_response(&msg);
}

void mock_send_dated_weather_response(void) {
    ble_msg_t msg = {
        .hdr = {
            .opcode = BLE_OP_DATED_WEATHER_QUERY,
            .req_app = 15,
            .len = BLE_MSG_WEATHER_SIZE
        }
    };

    uint8_t *p = msg.payload;
    p[0] = 0xFF;
    p[1] = 0xFF;
    p[2] = 0xFF;
    p[3] = 0xFF;

    p += 4;

    for (int i = 0; i < 24; i++) {

        int16_t temp = 195 + i*10;

        *p++ = (temp >> 8) & 0xFF;
        *p++ = temp & 0xFF;

        *p++ = 65;   // humidity
        *p++ = 10;   // precipitation probability
        *p++ = 2;    // weather code
        *p++ = 12;   // wind speed
    }

    handle_ble_response(&msg);
}

static void keyboard_task(void* arg) {
    printf("[QEMU MOCK] Keyboard active\n");
    printf("Controls:\n");
    printf("  a -> left\n");
    printf("  d -> right\n");
    printf("  enter -> ok\n");
    printf("  esc -> back\n");
    printf("  s -> time update\n");
    printf("  w -> weather_update\n");

    while(1)
    {
        int c = getchar();

        switch(c)
        {
            case 'a':
                register_input(INPUT_LEFT);
                break;

            case 'd':
                register_input(INPUT_RIGHT);
                break;

            case '\n':
                register_input(INPUT_OK);
                break;

            case 's':
                mock_send_time();
                break;

            case 'w':
                mock_send_weather();
                break;

            case 'q':
                mock_send_dated_weather_response();
                break;

            case 27:
                register_input(INPUT_ESC);
                break;

            default:
                break;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void __wrap_encoder_init(QueueHandle_t queue) {
    printf("[QEMU MOCK] Encoder replaced by keyboard\n");
    xinputQueue = queue;
    xTaskCreate(keyboard_task, "keyboard_input", 2048, NULL, 4, &keyboard_task_handle);
}

void __wrap_encoder_deinit(void) {
    printf("[QEMU MOCK] Mocked Keyboard encoder stopped\n");
    vTaskDelete(keyboard_task_handle);
    keyboard_task_handle = NULL;
}
