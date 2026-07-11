#include <lvgl.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <esp_system.h>
#include <esp_log.h>
#include <encoder_driver.h>
#include <global_locks.h>

#include "lvgl_input_translator.h"

static QueueHandle_t xinputQueue;
static const char* TAG = "LVGL Key Tanslator";
static uint32_t last_key = 0;
static lv_indev_state_t last_input_state = LV_INDEV_STATE_RELEASED;
static lv_indev_t *keypad_indev = NULL;
static lv_group_t *key_grp = NULL;

static void keypad_read_callback(lv_indev_t * indev,
                        lv_indev_data_t * data)
{
    if(!xinputQueue) {
        esp_system_abort("Undefined input queue");
    }

    encoder_input_t received_key;
    if (xQueueReceive(xinputQueue, &received_key, 0) == pdTRUE) {
        switch(received_key.key) {
            case INPUT_OK :
                last_key = LV_KEY_ENTER;
                ESP_LOGI(TAG, "Enter key");
                break;
            case INPUT_ESC :
                last_key = LV_KEY_ESC;
                ESP_LOGI(TAG, "Esc key");
                break;
            case INPUT_LEFT :
                last_key = LV_KEY_LEFT;
                ESP_LOGI(TAG, "LEFT key");
                break;
            case INPUT_RIGHT :
                last_key = LV_KEY_RIGHT;
                ESP_LOGI(TAG, "RIGHT key");
                break;
            default :
               ESP_LOGE(TAG, "Unknown input : %d", received_key.key); 
        }

        if(received_key.state == INPUT_STATE_PRESSED) {
            last_input_state = LV_INDEV_STATE_PRESSED;
        }
        else {
            last_input_state = LV_INDEV_STATE_RELEASED;
        }
    }
    data->key = last_key;
    data->state = last_input_state;
}

void setup_keyboard(QueueHandle_t inputQueue)
{
    xinputQueue = inputQueue;
    WITH_UI_LOCK() {
        keypad_indev = lv_indev_create();
        lv_indev_set_type(keypad_indev,
                          LV_INDEV_TYPE_KEYPAD);
        lv_indev_set_read_cb(keypad_indev,
                             keypad_read_callback);

        key_grp = lv_group_create();
        lv_group_set_default(key_grp);
        lv_indev_set_group(keypad_indev,
                           key_grp);
    }
    ESP_LOGI(TAG, "Keyboard setup done");
}

void delete_keyboard() {
    WITH_UI_LOCK() {
        if(keypad_indev) {
            lv_indev_set_read_cb(keypad_indev, NULL);
            lv_indev_delete(keypad_indev);
            keypad_indev = NULL;
        }
        if(key_grp) {
            lv_group_delete(key_grp);
            key_grp = NULL;
        }
    }
    xinputQueue = NULL;
    last_key = 0;
    last_input_state = LV_INDEV_STATE_RELEASED;
}

void suspend_keyboard(void) {
    if (keypad_indev) {
        WITH_UI_LOCK() {
            lv_indev_enable(keypad_indev, false);
        }
    }
}

void resume_keyboard(void) {
    if (keypad_indev) {
        WITH_UI_LOCK() {
            lv_indev_enable(keypad_indev, true);
        }
    }
}

lv_group_t* get_key_group(void) {
    return key_grp;
}

const lv_indev_t* get_keypad_indev(void) {
    return keypad_indev;
}
