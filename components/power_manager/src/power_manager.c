#include <power_manager.h>
#include <esp_pm.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_log.h>
#include <esp_err.h>
#include <event_manager.h>
#include <runtime_manager.h>
#include <power_types.h>
#include <ui_manager.h>
#include <tick_manager.h>
#include <common_types.h>
#include <esp_system.h>
#include <esp_sleep.h>
#include <gpio_manager.h>

static const char* TAG = "Power Manager";
static int64_t sleep = 0;
static QueueHandle_t power_queue = NULL;
static TaskHandle_t power_task = NULL;
static bool initialized = false;
static power_state_t state = POWER_STATE_SLEEP;

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

static void transition_to_ui_active(void) {
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

static int light_sleep_enter_cb(int64_t sleep_us, void* arg) {
    return ESP_OK;
}

static int light_sleep_exit_cb(int64_t sleep_us, void* arg) {
    sleep += sleep_us;
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_causes();
    if(cause & (1<<ESP_SLEEP_WAKEUP_GPIO)) {
        transition_to_ui_active();
    }
    return ESP_OK;
}

static void sleep_debug_cb(const event_t* event) {
    if(event && event->ev == EVENT_WORK_TICK) {
        ESP_LOGI(TAG, "Slept for %lldus last work window", sleep);
        sleep = 0; 
    }
}

void power_manager_init(void) {
    if(initialized) return;
    ESP_LOGI(TAG, "Initializing Power manager");
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
    initialized = true;
    event_subscribe(EVENT_UI_INACTIVE, ui_inactive_event_cb);
    event_subscribe(EVENT_WORK_TICK, sleep_debug_cb);
    event_subscribe(EVENT_ALARM_TRIGGERED, alarm_triggered_cb);
    tick_manager_generate_tick(TICK_WORK);
    state = POWER_STATE_UI_ACTIVE;    
    sleep = 0;                                
}

void power_manager_deinit(void) {
    event_unsubscribe(EVENT_UI_INACTIVE, ui_inactive_event_cb);
    event_unsubscribe(EVENT_WORK_TICK, sleep_debug_cb);
    event_unsubscribe(EVENT_ALARM_TRIGGERED, alarm_triggered_cb);
    power_cmd_t cmd = POWER_CMD_SHUTDOWN;
    xQueueSend(power_queue, &cmd, 0);
    initialized = false;
}
