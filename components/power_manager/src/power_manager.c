#include <power_manager.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <event_manager.h>
#include <runtime_manager.h>
#include <gpio_manager.h>
#include <power_types.h>
#include <tick_manager.h>
#include <ui_manager.h>
#include <common_types.h>
#include <esp_system.h>
#include <esp_sleep.h>
#include <esp_pm.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_attr.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <string.h>

typedef struct {
    int64_t last_bt_wake_time;
    int64_t delta_mean;
    uint8_t shift_sample;
    bool early_wakeup;
} phase_shift_calculator_t;

static const char* TAG = "Power Manager";
static int64_t sleep = 0;
static int64_t last_sleep_exit_us = 0;
static int64_t first_sleep_exit_us = 0;
static bool first_sleep = true;
static uint8_t sleep_wakes = 0;
static uint8_t gpio_sleep_wakes = 0;
static uint8_t bt_sleep_wakes = 0;
static uint8_t unknown_sleep_wakes = 0;
static esp_sleep_wakeup_cause_t last_wakeup_cause = ESP_SLEEP_WAKEUP_UNDEFINED;
static int64_t requested_sleep_total = 0;
static uint32_t sleep_entries = 0;
static QueueHandle_t power_queue = NULL;
static TaskHandle_t power_task = NULL;
static TaskHandle_t main_task = NULL;
static bool initialized = false;
static power_state_t state = POWER_STATE_SLEEP;
static esp_pm_lock_handle_t hw_sleep_lock = NULL;
static phase_shift_calculator_t shift_calculator = {0};
static const int64_t MAX_ACCEPTABLE_DELTA_CHANGE_US = 1000LL;
static const uint8_t MIN_SHIFT_SAMPLES_FOR_SHIFT = 5;
static const unsigned int WORK_TICK_OVERLAP_BUFFER = 2000LL;

static void power_task_fn(void *arg)
{
    power_cmd_t cmd;

    while (true) {
        if (xQueueReceive(power_queue, &cmd, portMAX_DELAY) != pdTRUE)
            continue;
        switch (cmd) {
            case POWER_CMD_UI_SLEEP:
                if(state == POWER_STATE_BACKGROUND) break;
                state = POWER_STATE_BACKGROUND;
                ESP_LOGI(TAG, "Suspending UI");
                ui_sleep();
                gpio_manager_enter_background_mode();
                runtime_manager_set_active_state(false);
                break;

            case POWER_CMD_UI_WAKE:
                if(state == POWER_STATE_UI_ACTIVE) break;
                state = POWER_STATE_UI_ACTIVE;
                gpio_manager_enter_active_mode();
                ESP_LOGI(TAG, "Resuming UI");
                runtime_manager_set_active_state(true);
                ui_resume();
                break;

            case POWER_CMD_SHUTDOWN:
                if(state == POWER_STATE_SLEEP) break;
                ESP_LOGI(TAG, "Power task shutting down");
                vQueueDelete(power_queue);
                state = POWER_STATE_SLEEP;
                power_queue = NULL;
                power_task = NULL;
                vTaskDelete(NULL);
                break;
            default :
                break;
        }
    }
}

static IRAM_ATTR void transition_to_ui_active(void) {
    power_cmd_t new_cmd = POWER_CMD_UI_WAKE;
    if (power_queue) {
        (void)xQueueSend(power_queue, &new_cmd, 0);
    }
}

static void ui_inactive_event_cb(const event_t* event) {
    if(event->ev == EVENT_UI_INACTIVE) {
        power_cmd_t new_cmd = POWER_CMD_UI_SLEEP;
        xQueueSend(power_queue, &new_cmd, 0);
    }
}

static void alarm_triggered_cb(const event_t* event) {
    if(state != POWER_STATE_UI_ACTIVE) {
        transition_to_ui_active();
    }
}

static IRAM_ATTR int light_sleep_enter_cb(int64_t sleep_us, void* arg) {
    requested_sleep_total += sleep_us;
    sleep_entries++;
    return ESP_OK;
}

static IRAM_ATTR int light_sleep_exit_cb(int64_t sleep_us, void* arg) {
    sleep += sleep_us;
    sleep_wakes++;
    last_sleep_exit_us = esp_timer_get_time();
    if(first_sleep) {
        first_sleep_exit_us = last_sleep_exit_us;
        first_sleep = false;
    }
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_causes();
    if(cause & (1<<ESP_SLEEP_WAKEUP_GPIO)) {
        gpio_sleep_wakes++;
        transition_to_ui_active();
    }
    else if(cause & (1<<ESP_SLEEP_WAKEUP_BT)) {
        bt_sleep_wakes++;
        shift_calculator.early_wakeup = true;
        shift_calculator.last_bt_wake_time = esp_timer_get_time();
    }
    else {
        unknown_sleep_wakes++;
        last_wakeup_cause = cause;
    }
    return ESP_OK;
}

static void sync_bt_with_work_window(void) {
    if(shift_calculator.last_bt_wake_time <= 0) {
        // Invalid scenario, skip
        memset(&shift_calculator, 0, sizeof(shift_calculator));
    }
    else {
        int64_t delta_now = esp_timer_get_time() - shift_calculator.last_bt_wake_time;
        ESP_LOGI(TAG, "Work tick and bt unsynced, delta = %lld", delta_now);
        if(delta_now > 0) {
            if(shift_calculator.shift_sample != 0 &&
               (delta_now - shift_calculator.delta_mean) > MAX_ACCEPTABLE_DELTA_CHANGE_US) {
                ESP_LOGD(TAG, "Large delta variance, letting it stabilize");
                memset(&shift_calculator, 0, sizeof(shift_calculator));
            }
            else {
                shift_calculator.delta_mean = (shift_calculator.delta_mean *
                                              shift_calculator.shift_sample +
                                              delta_now) / (shift_calculator.shift_sample+1);
                shift_calculator.shift_sample++;
                if(shift_calculator.shift_sample >= MIN_SHIFT_SAMPLES_FOR_SHIFT) {
                    int64_t work_interval_us = tick_manager_get_tick_interval_ms(TICK_WORK) * 1000LL;
                    int64_t shift = work_interval_us - shift_calculator.delta_mean -
                                    WORK_TICK_OVERLAP_BUFFER;
                    if(shift <= 0 || shift > work_interval_us) {
                        ESP_LOGE(TAG, "Invalid shift : %lld , delta_mean = %lld",
                                 shift, shift_calculator.delta_mean);
                        memset(&shift_calculator, 0, sizeof(shift_calculator));
                    }
                    else {
                        bool success = tick_manager_shift_work_tick((unsigned int) shift);
                        if(!success) {
                            ESP_LOGE(TAG, "Failed to shift work tick");
                        }
                        else {
                            ESP_LOGI(TAG, "Shifted work tick by %u", shift);
                        }
                        // Regardless of success reset the calculator so it
                        // calculates everything again
                        memset(&shift_calculator, 0, sizeof(shift_calculator));
                    }
                }
            }
        }
        else {
            // Unstable drifts (maybe connection interval not
            // negotiated yet), reset state
            memset(&shift_calculator, 0, sizeof(shift_calculator));
        }
    }
}

static void print_debug_logs(void) {
    ESP_LOGI(TAG, "Slept for %lldus last work window", sleep);
    ESP_LOGD(TAG, "Woke up %u times last work window, GPIO : %u  \
             BT : %u Unknown : %u, Last unknown wakeup : 0x%08x",
             sleep_wakes, gpio_sleep_wakes, bt_sleep_wakes, unknown_sleep_wakes,
             last_wakeup_cause);
    ESP_LOGD(TAG,
         "Light sleep: entries=%u requested=%lldus actual=%lldus",
         sleep_entries, requested_sleep_total, sleep);
    ESP_LOGD(TAG, "First sleep timestamp : %lld Last sleep exit timestamp : %lld",
             first_sleep_exit_us, last_sleep_exit_us);
    sleep = 0;
    sleep_wakes = 0;
    gpio_sleep_wakes = 0;
    bt_sleep_wakes = 0;
    unknown_sleep_wakes = 0;
    requested_sleep_total = 0;
    sleep_entries = 0;
    first_sleep = true;
    last_wakeup_cause = ESP_SLEEP_WAKEUP_UNDEFINED;
    ESP_LOGI(TAG, "Heap: free=%u KiB, largest=%u KiB, min=%u KiB",
             heap_caps_get_free_size(MALLOC_CAP_8BIT) / 1024,
             heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) / 1024,
             heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT) / 1024);
}

static void sleep_debug_cb(const event_t* event) {
    if(event && event->ev == EVENT_WORK_TICK) {
        if(shift_calculator.early_wakeup == true) {
            ESP_LOGI(TAG, "Fired BT wakeup reason");
            shift_calculator.early_wakeup = false;
            sync_bt_with_work_window();
        }
        else {
            if(shift_calculator.shift_sample > 0) {
                ESP_LOGI(TAG, "Missed contiguous BT wakeup, resetting.");
                memset(&shift_calculator, 0, sizeof(shift_calculator));
            }
        }
        print_debug_logs();
    }
}

void power_manager_prevent_sleep(void) {
    assert(initialized);
    assert(hw_sleep_lock);
    ESP_ERROR_CHECK(esp_pm_lock_acquire(hw_sleep_lock));
}

void power_manager_allow_sleep(void) {
    assert(initialized);
    assert(hw_sleep_lock);
    ESP_ERROR_CHECK(esp_pm_lock_release(hw_sleep_lock));
}

void power_manager_engage_deep_sleep(void) {
    ESP_LOGI(TAG, "Goodbye. Dropping to deep sleep!");
    esp_deep_sleep_start();
}

static void no_motion_detected_handler(const event_t *event) {
    if(main_task) {
        xTaskNotify(main_task, POWER_MANAGER_SHUTDOWN_REASON_DEEP_SLEEP, eSetValueWithOverwrite);
        main_task = NULL;
        ESP_LOGI(TAG, "Received no motion trigger, shutdown commencing");
    }
    else {
        ESP_LOGE(TAG, "Main task is already shutting down");
    }
}

static void dfu_update_start_handler(const event_t *event) {
    if(main_task) {
        power_manager_prevent_sleep();
        xTaskNotify(main_task, POWER_MANAGER_SHUTDOWN_REASON_DFU, eSetValueWithOverwrite);
        main_task = NULL;
        ESP_LOGI(TAG, "Received DFU update event, shutdown commencing");
    }
    else {
        ESP_LOGE(TAG, "Main task is already shutting down");
    }
}

void power_manager_init(TaskHandle_t task) {
    if(initialized) return;
    ESP_LOGI(TAG, "Initializing Power manager");
    main_task = task;
    power_queue = xQueueCreate(4, sizeof(power_cmd_t));
    if (power_queue == NULL) {
        ESP_LOGE(TAG, "Failed creating power queue");
        esp_system_abort("Power queue allocation failed");
    }

    BaseType_t rc = xTaskCreatePinnedToCore(power_task_fn, "Power Manager", 3072, NULL,
                                            2, &power_task, tskNO_AFFINITY);

    if (rc != pdPASS) {
        ESP_LOGE(TAG, "Failed creating power task");
        esp_system_abort("Power task creation failed");
    }
    esp_pm_config_t pm_config = {
        .max_freq_mhz = 160,
        .min_freq_mhz = 160,
        .light_sleep_enable = true,
    };
    esp_err_t err = esp_pm_configure(&pm_config);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Error in initalizing Power manager %s", esp_err_to_name(err));
    }
    esp_pm_sleep_cbs_register_config_t pm_callbacks = {
        .enter_cb = light_sleep_enter_cb,
        .exit_cb = light_sleep_exit_cb,
        .enter_cb_prior = 5,
        .exit_cb_prior = 5,
        .enter_cb_user_arg = NULL,
        .exit_cb_user_arg = NULL
    };
    err = esp_pm_light_sleep_register_cbs(&pm_callbacks);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Error registering PM callbacks");
    }
    memset(&shift_calculator, 0, sizeof(shift_calculator));
    event_subscribe(EVENT_UI_INACTIVE, ui_inactive_event_cb);
    event_subscribe(EVENT_WORK_TICK, sleep_debug_cb);
    event_subscribe(EVENT_ALARM_TRIGGERED, alarm_triggered_cb);
    //event_subscribe(EVENT_WATCH_STATIONARY, no_motion_detected_handler);
    event_subscribe(EVENT_DFU_START, dfu_update_start_handler);
    ESP_ERROR_CHECK(esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "Sleep lock", &hw_sleep_lock));
    assert(hw_sleep_lock);
    state = POWER_STATE_UI_ACTIVE;
    sleep = 0;
    initialized = true;
}

void power_manager_deinit(void) {
    initialized = false;
    if(hw_sleep_lock) {
        // Will fail if not acquired so no error check
        while (esp_pm_lock_release(hw_sleep_lock) == ESP_OK) { }
        ESP_ERROR_CHECK(esp_pm_lock_delete(hw_sleep_lock));
        hw_sleep_lock = NULL;
    }
    memset(&shift_calculator, 0, sizeof(shift_calculator));
    //event_unsubscribe(EVENT_WATCH_STATIONARY, no_motion_detected_handler);
    event_unsubscribe(EVENT_DFU_START, dfu_update_start_handler);
    event_unsubscribe(EVENT_ALARM_TRIGGERED, alarm_triggered_cb);
    event_unsubscribe(EVENT_UI_INACTIVE, ui_inactive_event_cb);
    event_unsubscribe(EVENT_WORK_TICK, sleep_debug_cb);
    power_cmd_t cmd = POWER_CMD_SHUTDOWN;
    xQueueSend(power_queue, &cmd, 0);
}
