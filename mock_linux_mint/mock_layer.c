#include <stdio.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_qemu_rgb.h"
#include <stdio.h>
#include <unistd.h>
#include <encoder_driver.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <runtime_manager.h>
#include <ble_response_handler.h> 
#include <ble_types.h>
#include <ble_consts.h>
#include <driver/ledc.h>

esp_lcd_panel_handle_t qemu_mock_panel_handle = NULL;
void (*callback) ();
extern void init_fsm(void);
static  uint32_t argb_buffer[128*160];
static QueueHandle_t xinputQueue = NULL;

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

        int16_t temp = 285;

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

        int16_t temp = 285;

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
    xTaskCreate(keyboard_task, "keyboard_input", 4096, NULL, 5, NULL);
}

void __wrap_encoder_deinit(void) {
    printf("[QEMU MOCK] Keyboard encoder stopped\n");
}

void __wrap_ble_manager_init(void) {
    printf("[QEMU MOCK] BLE driver bypassed successfully.\n");
}

void __wrap_init_display(void (*cb)(void)) {
    ESP_LOGI("QEMU_MOCK", "Intercepting high-level init_display() function...");
    esp_lcd_rgb_qemu_config_t qemu_screen_config = {
        .width = 128,  
        .height = 160  
    };

    esp_err_t ret = esp_lcd_new_rgb_qemu(&qemu_screen_config, &qemu_mock_panel_handle);
    
    if (ret == ESP_OK && qemu_mock_panel_handle) {
        printf("[QEMU DISPLAY] Virtual display matrix generated successfully!\n");
        esp_lcd_panel_reset(qemu_mock_panel_handle);
        esp_lcd_panel_init(qemu_mock_panel_handle);
    } else {
        ESP_LOGE("QEMU_MOCK", "Failed to allocate virtual framebuffer memory.");
        return;
    }
    callback = cb;
    init_fsm();
}

void __wrap_display_draw_bitmap(int x1, int y1, int x2, int y2, const void* pixels) {
     if (!qemu_mock_panel_handle)
        return;

    const uint16_t *src = pixels;

    int width = x2 - x1;
    int height = y2 - y1;

    for(int y = 0; y < height; y++) {
        for(int x = 0; x < width; x++) {
            uint16_t swapped = src[y * width + x];
    
            // LVGL RGB565_SWAPPED -> normal RGB565
            uint16_t rgb565 =
                (swapped >> 8) |
                (swapped << 8);
    
            uint8_t r = ((rgb565 >> 11) & 0x1F) << 3;
            uint8_t g = ((rgb565 >> 5) & 0x3F) << 2;
            uint8_t b = (rgb565 & 0x1F) << 3;
    
            argb_buffer[y * width + x] =
                0xFF000000 |
                (r << 16) |
                (g << 8) |
                b;
        }
    }

    esp_lcd_panel_draw_bitmap(qemu_mock_panel_handle, x1, y1, x2, y2, argb_buffer);
    callback();
}

bool __wrap_display_on(void) {
    printf("[QEMU DISPLAY] Screen Power -> ON\n");
    if (qemu_mock_panel_handle) {
        esp_lcd_panel_disp_on_off(qemu_mock_panel_handle, true);
    }
    return true; 
}

bool __wrap_display_off(void) {
    printf("[QEMU DISPLAY] Screen Power -> OFF\n");
    if (qemu_mock_panel_handle) {
        esp_lcd_panel_disp_on_off(qemu_mock_panel_handle, false);
    }
    return true;
}

bool __wrap_display_sleep(void) {
    printf("[QEMU DISPLAY] Screen Power -> OFF\n");
    return true;
}

esp_err_t __wrap_led_strip_new_rmt_device(const void* strip_conf, const void* rmt_conf, void** ret_strip) {
    printf("[QEMU MOCK] LED Strip RMT device bypassed.\n");
    return ESP_OK;
}

esp_err_t __wrap_gpio_sleep_sel_en(int gpio_num) {
    return ESP_OK;
}

esp_err_t __wrap_esp_sleep_enable_gpio_wakeup(void) {
    return ESP_OK;
}

esp_err_t __wrap_ledc_set_duty(ledc_mode_t speed_mode, ledc_channel_t channel, uint32_t duty) {
    (void)speed_mode;
    (void)channel;
    (void)duty;
    return ESP_OK;
}

esp_err_t __wrap_ledc_update_duty(ledc_mode_t speed_mode, ledc_channel_t channel) {
    (void)speed_mode;
    (void)channel;
    return ESP_OK;
}
