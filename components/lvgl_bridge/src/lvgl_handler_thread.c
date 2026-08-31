#include <lvgl_handler_thread.h>

#include <lvgl.h>
#include <esp_timer.h>
#include <stdatomic.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_log.h>
#include <esp_system.h>
#include <common_consts.h>
#include <global_locks.h>
#include <event_manager.h>
#include <stdint.h>
#include <storage_manager.h>
#include <lvgl_bridge_types.h>

static atomic_bool lvgl_run_flag = false;
static TaskHandle_t worker_handle = NULL;
static const char* TAG = "LVGL Thread";
static SemaphoreHandle_t lvgl_exit_semaphore = NULL;
static uint32_t input_timeout = DISPLAY_MIN_USER_INPUT_TIMEOUT;
static wakelock_check_func_t wakelock_check_func = NULL;

static void lvgl_task(void *pvParameter) {
    bool first_render = true;    
    lv_display_trigger_activity(NULL);
    int64_t time_prev, time_now = esp_timer_get_time() / 1000;
    while(atomic_load(&lvgl_run_flag)) {
        time_prev = time_now;
        uint32_t lvgl_next_refresh_interval = 30;  
        uint32_t idle_time = 0; 

        WITH_UI_LOCK() {
            if(first_render) {
                lv_obj_invalidate(lv_screen_active());
                lv_refr_now(NULL);
                first_render = false;
            }
            time_now = esp_timer_get_time() / 1000;
            int64_t elapsed = time_now - time_prev;
            // Tell LVGL how much time has passed
            lv_tick_inc((uint32_t) elapsed);
            lvgl_next_refresh_interval = lv_timer_handler();
            idle_time = lv_display_get_inactive_time(NULL);
        }

        if(wakelock_check_func && !wakelock_check_func() && (idle_time > input_timeout)) {
            // Raise event to put display to sleep
            ESP_LOGI(TAG, "User inactivity, putting display to sleep");
            event_t ev = {
                .ev = EVENT_UI_INACTIVE,
                .payload_len = 0,
                .data = NULL
            };
            event_publish(&ev);
        }
        
        // Target ~33fps on the display
        if(lvgl_next_refresh_interval == LV_NO_TIMER_READY || lvgl_next_refresh_interval > 30) {
            lvgl_next_refresh_interval = 30;
        }

        uint32_t ticks_to_delay = pdMS_TO_TICKS(lvgl_next_refresh_interval);        
        // Protect against 0 delay which will crash FreeRTOS
        if(ticks_to_delay == 0) {
            ticks_to_delay = 1;
        }
        xTaskNotifyWait(0, UINT32_MAX, NULL, ticks_to_delay);
    }

    xSemaphoreGive(lvgl_exit_semaphore);
    worker_handle = NULL;
    vTaskDelete(NULL);
}

static bool save_input_timeout(uint32_t timeout) {
    uint8_t to[4] = {(timeout & 0xFF) , (timeout >> 8)&0xFF, (timeout >> 16)&0xFF, (timeout >> 24)&0xFF };
    bool success = storage_manager_save_key(CORE_SYSTEM_APP_ID, "TIMEOUT", to, sizeof(uint32_t));
    if(!success) {
        ESP_LOGE(TAG, "Unable to store timeout");
        return false;
    }
    return true;
}

static uint32_t retrieve_input_timeout(void) {
    uint32_t timeout;
    uint8_t data[4];
    uint8_t data_len = sizeof(uint32_t);
    bool success = storage_manager_retrieve_key(CORE_SYSTEM_APP_ID, "TIMEOUT", data, &data_len);
    if(success) {
        assert(data_len == sizeof(uint32_t));
        timeout = ((uint32_t)data[3] << 24) | ((uint32_t)data[2] << 16) | 
                  ((uint32_t)data[1] << 8) | (uint32_t)data[0]; 
        if(timeout < DISPLAY_MIN_USER_INPUT_TIMEOUT) {
            // Timeout too short overwrite with default
            input_timeout =  DISPLAY_MIN_USER_INPUT_TIMEOUT;
            save_input_timeout(DISPLAY_MIN_USER_INPUT_TIMEOUT);
        }
        else {
            return timeout;
        }
    }
    else {
        // No key found, first time boot
        input_timeout = DISPLAY_MIN_USER_INPUT_TIMEOUT;
        save_input_timeout(DISPLAY_MIN_USER_INPUT_TIMEOUT);
    }
    return input_timeout;
}

bool update_input_timeout(uint32_t timeout) {
    assert(timeout >= DISPLAY_MIN_USER_INPUT_TIMEOUT);
    input_timeout = timeout;
    return save_input_timeout(input_timeout);    
}

uint32_t get_input_timeout(void) {
    return input_timeout;
}

void start_lvgl_thread(wakelock_check_func_t func) {
    input_timeout = retrieve_input_timeout();
    wakelock_check_func = func;
    if(worker_handle != NULL) {
        ESP_LOGW(TAG, "LVGL task already running");
        if(lvgl_exit_semaphore)
            stop_lvgl_thread();
    }
    atomic_store(&lvgl_run_flag, true);
    lvgl_exit_semaphore = xSemaphoreCreateBinary();
    if (lvgl_exit_semaphore == NULL) {
        ESP_LOGE(TAG, "Failed creating exit semaphore");
        esp_system_abort("LVGL semaphore allocation failed");
    }
    BaseType_t result = xTaskCreatePinnedToCore(lvgl_task, "LVGL Handler", DISPLAY_LVGL_STACK_SIZE, NULL, 
                                                5, &worker_handle, DISPLAY_LVGL_CPU_CORE);
    if(result!= pdPASS) {
        ESP_LOGE(TAG, "Critical : Failed launching LVGL");
        atomic_store(&lvgl_run_flag, false);
        vSemaphoreDelete(lvgl_exit_semaphore);
        lvgl_exit_semaphore = NULL;
        worker_handle = NULL;
    } 
    else {
        ESP_LOGI(TAG, "Started LVGL thread");
    }
}

void stop_lvgl_thread(void) {
    atomic_store(&lvgl_run_flag, false);
    if (worker_handle) {
        xTaskNotifyGive(worker_handle);
    }
    if(!lvgl_exit_semaphore) return;
    //xSemaphoreTake(lvgl_exit_semaphore, 0);
    if(xSemaphoreTake(lvgl_exit_semaphore, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "Unable to join with stopped lvgl thread, proceeding anyway");
    }
    vSemaphoreDelete(lvgl_exit_semaphore);
    lvgl_exit_semaphore = NULL;
    wakelock_check_func = NULL;
}

bool lvgl_thread_exists(void) {
    return worker_handle != NULL;
}
