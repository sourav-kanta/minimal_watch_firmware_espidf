#include <alarm_manager.h>
#include <alarm_manager_internal.h>
#include <esp_timer.h>
#include <stdlib.h>
#include <string.h>
#include <common_consts.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_log.h>
#include <common_types.h>
#include <state_manager.h>
#include <event_manager.h>

static alarm_sync_t* alarms = NULL;
static esp_timer_handle_t alarm_timer = NULL;
static const uint32_t ALARM_BUFFER_SEC = 5;
static SemaphoreHandle_t alarm_mutex = NULL; 
static const char* TAG = "Alarm Manager";

static void discard_expired_alarms(uint32_t now) {
    if(!alarms || alarms->n_alarms == 0) {
        return;
    }

    int valid_idx = 0;
    while(valid_idx < alarms->n_alarms && alarms->alarms[valid_idx].epoch <= now + ALARM_BUFFER_SEC) {
        ESP_LOGW(TAG, "Discarding expired alarm: epoch %u", (unsigned int)alarms->alarms[valid_idx].epoch);
        valid_idx++;
    }

    if(valid_idx == 0) {
        return;
    }

    int remaining = alarms->n_alarms - valid_idx;
    if (remaining > 0) {
        memmove(&alarms->alarms[0], &alarms->alarms[valid_idx], remaining * sizeof(alarm_t));
    }
    alarms->n_alarms = remaining;
}

static void schedule_next_alarm_internal(void) {
    if(!alarms || alarms->n_alarms == 0 || !alarm_timer) return;

    esp_timer_stop(alarm_timer);

    uint32_t now = get_epoch_time();
    uint32_t target_epoch = alarms->alarms[0].epoch;

    uint64_t timeout_us = (target_epoch > now + ALARM_BUFFER_SEC) 
        ? ((uint64_t)(target_epoch - now) * 1000000ULL) 
        : 5*1000000ULL; 

    esp_err_t err = esp_timer_start_once(alarm_timer, timeout_us);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start timer: %s", esp_err_to_name(err));
    }
}

bool alarm_manager_revalidate(void) {
    if(!alarms || !alarm_timer) {
        return false;
    }

    if(alarm_mutex && xSemaphoreTake(alarm_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire alarm lock for revalidation");
        return false;
    }

    uint32_t now = get_epoch_time();
    discard_expired_alarms(now);

    if(alarms->n_alarms > 0) {
        schedule_next_alarm_internal();
    } else {
        esp_timer_stop(alarm_timer);
    }

    if(alarm_mutex) {
        xSemaphoreGive(alarm_mutex);
    }

    return true;
}

uint8_t alarm_manager_get_all_alarms(alarm_t* alarms_buff) {
    if(alarm_mutex == NULL || alarms_buff == NULL) {
        ESP_LOGE(TAG, "Alarm mutex is invalid or invalid buffer");
        return 0;
    }

    if(alarm_mutex && xSemaphoreTake(alarm_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire alarm lock for read");
        return 0;
    }

    if(alarms == NULL || alarms->n_alarms > MAX_WATCH_ALARMS) {
        if (alarm_mutex) {
            xSemaphoreGive(alarm_mutex);
        }
        return 0;
    }

    uint8_t total_alarms =  alarms->n_alarms;
    memcpy(alarms_buff, alarms->alarms, alarms->n_alarms*sizeof(alarm_t)); 
    if(alarm_mutex) {
        xSemaphoreGive(alarm_mutex);
    }
    return total_alarms;
}

bool alarm_manager_create_alarm(alarm_t* alarm) {
    if(!alarm || !alarms) return false;
    assert(alarms->n_alarms <= MAX_WATCH_ALARMS);

    if(alarm_mutex && xSemaphoreTake(alarm_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire alarm lock");
        return false;
    }

    uint32_t now = get_epoch_time();

    if(alarm->epoch <= now || (alarm->epoch - now) <= ALARM_BUFFER_SEC) {
        ESP_LOGE(TAG, "Alarm epoch in past or buffer window, discarding : %u : %u", now, alarm->epoch);
        if (alarm_mutex) xSemaphoreGive(alarm_mutex);
        return false;
    }

    uint32_t diff_sec = alarm->epoch - now; 
    if(diff_sec > 31536000ULL) {
        ESP_LOGE(TAG, "Alarm too far in future, discarding");
        if(alarm_mutex) xSemaphoreGive(alarm_mutex);
        return false;
    }

    for(int k = 0; k < alarms->n_alarms; k++) {
        if(alarms->alarms[k].epoch == alarm->epoch) {
            ESP_LOGW(TAG, "Duplicate alarm, discarding");
            if (alarm_mutex) xSemaphoreGive(alarm_mutex);
            return false;
        }
    }

    int count = alarms->n_alarms;
    if(count == MAX_WATCH_ALARMS) {
        if(alarm->epoch >= alarms->alarms[MAX_WATCH_ALARMS - 1].epoch) {
            ESP_LOGW(TAG, "Array full and alarm exceeds max, discarding");
            if(alarm_mutex) xSemaphoreGive(alarm_mutex);
            return false;
        }
        count = MAX_WATCH_ALARMS - 1;
    }

    int insert_idx = count - 1;
    while(insert_idx >= 0 && alarms->alarms[insert_idx].epoch > alarm->epoch) {
        alarms->alarms[insert_idx + 1] = alarms->alarms[insert_idx];
        insert_idx--;
    }

    alarms->alarms[insert_idx + 1] = *alarm;

    if(alarms->n_alarms < MAX_WATCH_ALARMS) {
        alarms->n_alarms++;
    }

    if(insert_idx + 1 == 0) {
        schedule_next_alarm_internal();
    }

    if(alarm_mutex) xSemaphoreGive(alarm_mutex);
    return true;
}

bool alarm_manager_delete_alarm(int index) {
    if(!alarms) return false;

    if(alarm_mutex && xSemaphoreTake(alarm_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire lock for delete");
        return false;
    }

    if(index < 0 || index >= alarms->n_alarms) {
        ESP_LOGE(TAG, "Invalid index for deletion: %d", index);
        if (alarm_mutex) xSemaphoreGive(alarm_mutex);
        return false;
    }

    int num_to_move = alarms->n_alarms - 1 - index;
    if(num_to_move > 0) {
        memmove(&alarms->alarms[index], &alarms->alarms[index + 1], num_to_move * sizeof(alarm_t));
    }
    alarms->n_alarms--;

    if(index == 0) {
        if (alarms->n_alarms > 0) {
            schedule_next_alarm_internal();
        } else {
            esp_timer_stop(alarm_timer);
        }
    }

    if(alarm_mutex) xSemaphoreGive(alarm_mutex);
    return true;
}

bool alarm_manager_edit_alarm(int index, alarm_t* new_alarm) {
    if(!alarms || !new_alarm) return false;

    if(alarm_mutex && xSemaphoreTake(alarm_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire lock for edit");
        return false;
    }

    if(index < 0 || index >= alarms->n_alarms) {
        ESP_LOGE(TAG, "Invalid index for edit: %d", index);
        if (alarm_mutex) xSemaphoreGive(alarm_mutex);
        return false;
    }

    alarm_t old_alarm = alarms->alarms[index];

    if(alarm_mutex) xSemaphoreGive(alarm_mutex);

    if(!alarm_manager_delete_alarm(index)) {
        ESP_LOGE(TAG, "Failed to modify the alarm at index : %d", index);
        return false;
    }

    if(!alarm_manager_create_alarm(new_alarm)) {
        ESP_LOGW(TAG, "New alarm invalid, rolling back edit");
        alarm_manager_create_alarm(&old_alarm);
        return false;
    }

    return true;
}

static int compare_alarms(const void* arg1, const void* arg2) {
    const alarm_t* a1 = (const alarm_t*) arg1;
    const alarm_t* a2 = (const alarm_t*) arg2;
    if(a1->epoch < a2->epoch) return -1;
    if(a1->epoch > a2->epoch) return 1;
    return 0;
}

static void alarm_fired_cb(void* arg) {
    event_t alarm_event = {
        .ev = EVENT_ALARM_TRIGGERED,
        .payload_len = 0,
    }; 
    alarm_t fired_alarm;
    if(alarm_mutex && xSemaphoreTake(alarm_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if(alarms && alarms->n_alarms > 0) {
            ESP_LOGI(TAG, "Alarm fired: epoch %u", (unsigned int)alarms->alarms[0].epoch);

            memcpy(&fired_alarm, &alarms->alarms[0], sizeof(alarm_t));
            int num_to_move = alarms->n_alarms - 1;
            if(num_to_move > 0) {
                memmove(&alarms->alarms[0], &alarms->alarms[1], num_to_move * sizeof(alarm_t));
            }
            alarms->n_alarms--;

            if(alarms->n_alarms > 0) {
                schedule_next_alarm_internal();
            }

            alarm_event.payload_len = sizeof(alarm_t);
            alarm_event.data = &fired_alarm;
        }
        if(alarm_mutex) xSemaphoreGive(alarm_mutex);
        if(alarm_event.payload_len != 0) event_publish(&alarm_event);
    }
}

void alarm_manager_init(alarm_sync_t* rtc_alarms) {
    alarms = rtc_alarms;
    assert(alarms);

    assert(alarms->n_alarms <= MAX_WATCH_ALARMS);
    
    if(alarms->n_alarms > 0) {
        qsort(alarms->alarms, alarms->n_alarms, sizeof(alarm_t), compare_alarms);
    }

    esp_timer_create_args_t timer_arg = {
        .name = "Alarm timer",
        .arg = NULL,
        .callback = alarm_fired_cb,
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_arg, &alarm_timer));
    alarm_mutex = xSemaphoreCreateMutex();
    assert(alarm_mutex);
    uint32_t now = get_epoch_time();
    discard_expired_alarms(now);

    if(alarms->n_alarms > 0) {
        schedule_next_alarm_internal();
    }
}

void alarm_manager_deinit(void) {
    if(alarm_timer) {
        esp_timer_stop(alarm_timer);
        esp_timer_delete(alarm_timer);
        alarm_timer = NULL;
    }
    if(alarm_mutex) {
        vSemaphoreDelete(alarm_mutex);
        alarm_mutex = NULL;
    }
    alarms = NULL;
}
