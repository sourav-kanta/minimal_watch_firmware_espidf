#include <driver/pulse_cnt.h>
#include <driver/gpio.h>
#include <esp_timer.h>
#include <stdlib.h>
#include <gpio_pins.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <encoder_driver.h>
#include <esp_log.h>

static esp_timer_handle_t long_press_timer;
static pcnt_unit_handle_t pcnt_unit;
static esp_timer_handle_t debounce_timer;
static const int DEBOUNCE_MS = 50;
static const int LONG_PRESS_MS = 250;
static const char* TAG = "Encoder input";
static pcnt_channel_handle_t channel = NULL;
static QueueHandle_t xinputQueue = NULL;

static void register_input(input_key_t key) {
    if (!xinputQueue) return;

    encoder_input_t evt_press   = { .key = key, .state = INPUT_STATE_PRESSED };
    encoder_input_t evt_release = { .key = key, .state = INPUT_STATE_RELEASED };

    if (xPortInIsrContext()) {
        xQueueSendFromISR(xinputQueue, &evt_press, NULL);
        xQueueSendFromISR(xinputQueue, &evt_release, NULL);
    } 
    else {
        xQueueSend(xinputQueue, &evt_press, 0);
        xQueueSend(xinputQueue, &evt_release, 0);
    }
}

static bool pcnt_on_reach_cb(pcnt_unit_handle_t unit, 
                             const pcnt_watch_event_data_t *edata,
                             void *user_ctx) {
    if (edata->watch_point_value > 0)
        register_input(INPUT_RIGHT);
    else
        register_input(INPUT_LEFT);
    pcnt_unit_clear_count(unit);
    return false;
}



static void long_press_timer_cb(void* arg) {

    bool button_pressed = !gpio_get_level(ENCODER_KEY_OK); // Active low
    if (button_pressed == true) {
        register_input(INPUT_ESC);
    }

    else {
        register_input(INPUT_OK);
    }
}

static void gpio_isr_handler_cb(void* arg) {
    esp_timer_stop(debounce_timer);
    esp_timer_start_once(debounce_timer, DEBOUNCE_MS * 1000);
}

static void debounce_timer_cb(void* arg) {
    bool button_pressed = !gpio_get_level(ENCODER_KEY_OK); // Active low
    if (button_pressed == true) {
        esp_timer_start_once(long_press_timer, LONG_PRESS_MS * 1000);
    }
    else {
        if (esp_timer_is_active(long_press_timer)) {
            esp_timer_stop(long_press_timer);
        }
        register_input(INPUT_OK);
    }
}

void encoder_init(QueueHandle_t queue) {
    xinputQueue = queue;
    if(xinputQueue == NULL) {
        esp_system_abort("Failed creating encoder queue");
    }
    pcnt_unit_config_t unit_config = { .high_limit = 100, .low_limit = -100 };
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_unit));
    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 5000,
    };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit, &filter_config));
    pcnt_chan_config_t chan_a = { .edge_gpio_num = ENCODER_PIN_A, .level_gpio_num = ENCODER_PIN_B };
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_a, &channel));
    ESP_ERROR_CHECK(
        pcnt_channel_set_edge_action(channel, 
            PCNT_CHANNEL_EDGE_ACTION_DECREASE,
            PCNT_CHANNEL_EDGE_ACTION_INCREASE)
    );

    ESP_ERROR_CHECK(
        pcnt_channel_set_level_action(
            channel,
            PCNT_CHANNEL_LEVEL_ACTION_KEEP,
            PCNT_CHANNEL_LEVEL_ACTION_INVERSE
        )
    );
    ESP_ERROR_CHECK(pcnt_unit_add_watch_point(pcnt_unit, 2));
    ESP_ERROR_CHECK(pcnt_unit_add_watch_point(pcnt_unit, -2));
    pcnt_event_callbacks_t cbs = { .on_reach = pcnt_on_reach_cb };
    ESP_ERROR_CHECK(pcnt_unit_register_event_callbacks(pcnt_unit, &cbs, NULL));
    ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit));

    ESP_ERROR_CHECK(gpio_set_direction(ENCODER_KEY_OK, GPIO_MODE_INPUT));
    ESP_ERROR_CHECK(gpio_set_pull_mode(ENCODER_KEY_OK, GPIO_PULLUP_ONLY));

    // Active low so need to watch for negedge
    gpio_set_intr_type(ENCODER_KEY_OK, GPIO_INTR_NEGEDGE);
    ESP_ERROR_CHECK(gpio_isr_handler_add(ENCODER_KEY_OK, gpio_isr_handler_cb, NULL));
    esp_timer_create_args_t timer_args = { .callback = long_press_timer_cb };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &long_press_timer));
    esp_timer_create_args_t deb_args = { .callback = debounce_timer_cb };
    ESP_ERROR_CHECK(esp_timer_create(&deb_args, &debounce_timer));
    ESP_LOGI(TAG, "Encoder driver initialized");
}



void encoder_deinit(void) {
    esp_timer_stop(long_press_timer);
    esp_timer_delete(long_press_timer);
    esp_timer_stop(debounce_timer);
    esp_timer_delete(debounce_timer);
    ESP_ERROR_CHECK(gpio_isr_handler_remove(ENCODER_KEY_OK));
    ESP_ERROR_CHECK(pcnt_unit_stop(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_disable(pcnt_unit));
    if(channel) {
        ESP_ERROR_CHECK(pcnt_del_channel(channel));
    }
    ESP_ERROR_CHECK(pcnt_del_unit(pcnt_unit));
    xinputQueue = NULL;
    ESP_LOGI(TAG, "Encoder driver deinitialized");
} 

