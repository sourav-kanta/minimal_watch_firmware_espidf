#include <esp_log.h>
#include <common_types.h>
#include <common_consts.h>
#include <esp_attr.h>
#include <state_registry.h>
#include <event_manager.h>
#include <string.h>
#include <esp_timer.h>
#include <alarm_manager_internal.h>
#include <sys/time.h>
#include <soc/rtc.h>
#include <time.h>

#include <state_manager.h>

#define USEC_TO_SEC(x) ((x)/1000000ULL)

RTC_DATA_ATTR static watch_state_t watch_state;
RTC_DATA_ATTR static state_registry_t state_registry;
static const char* TAG = "State Manager";

static inline void state_request_complete(state_entry_t *entry)
{
    entry->last_req_time = 0;
    entry->request_pending = false;
}

static void update_time_state_cb(const event_t* event) {
    assert(event->payload_len == sizeof(uint32_t));
    assert(event->data);
    time_sync_t sync_time = {0};
    const uint32_t* epoch = (const uint32_t*) event->data;
    sync_time.last_sync_time = *epoch;
    sync_time.valid = 1;
    memcpy(&watch_state.time_state, &sync_time, sizeof(time_sync_t));
    state_request_complete(&state_registry.time);
    struct timeval tv = { .tv_sec = *epoch, .tv_usec = 0 };
    settimeofday(&tv, NULL);
    ESP_LOGI(TAG, "Updated system time");

    // Revalidate all alarms
    alarm_manager_revalidate();
}

static void update_weather_state_cb(const event_t* event) {
    assert(event->payload_len == sizeof(weather_sync_t));
    assert(event->data);
    const weather_sync_t* sync_weather = (const weather_sync_t*) event->data;
    memcpy(&watch_state.weather_state, sync_weather, sizeof(weather_sync_t));
    state_request_complete(&state_registry.weather);
    ESP_LOGI(TAG, "Updated system weather");
}

static void update_steps_cb(const event_t* ev) {
    assert(ev);
    assert(ev->payload_len == sizeof(uint32_t));
    assert(ev->data);
    uint32_t* new_steps = ev->data;
    watch_state.steps += *(new_steps);
}

void state_manager_init(void) {
    // Reset every thing on cold boot with unsynced time
    if(watch_state.time_state.valid != 1) {
        ESP_LOGW(TAG, "Time is invalid, wiping state");
        memset(&watch_state, 0, sizeof(watch_state_t));
        memset(&state_registry, 0, sizeof(state_registry_t));
    }
    // Clear expired weather data on hot or cold boot
    if(state_manager_get_epoch_time() > watch_state.weather_state.expires_at) {
        ESP_LOGW(TAG, "Weather is invalid, wiping weather data");
        memset(&watch_state.weather_state, 0, sizeof(weather_sync_t));
        memset(&state_registry.weather, 0, sizeof(state_entry_t));
    }
    // Clear garbage on cold boot
    if(watch_state.alarms.valid != 1 || watch_state.alarms.n_alarms > MAX_WATCH_ALARMS) {
        memset(&watch_state.alarms, 0, sizeof(alarm_sync_t));
        memset(&state_registry.alarms, 0, sizeof(state_entry_t));
        watch_state.alarms.valid = 1; 
    }

    alarm_manager_init(&watch_state.alarms);

    bool success = event_subscribe(EVENT_TIME_SYNC, update_time_state_cb);
    assert(success);
    success = event_subscribe(EVENT_WEATHER_SYNC, update_weather_state_cb);
    assert(success);
    success = event_subscribe(EVENT_NEW_STEPS, update_steps_cb);
    assert(success);
    
    // Request sync anyway
    ble_req_t time_req = {
        .req_code = UPDATE_SYSTEM_TIME,
        .app_id = 0,
        .req_data_len = 0,
        .req_data = NULL 
    };
    event_t ble_ev = {
       .ev = EVENT_BLE_REQUEST,
       .data = &time_req,
       .payload_len = sizeof(ble_req_t)
    };
    success = event_publish(&ble_ev);
    if(!success) {
        ESP_LOGE(TAG, "Time sync ble request failed");
    }
    else {
        state_registry.time.last_req_time = time(NULL);
        state_registry.time.request_pending = true;
    }
    ble_req_t weather_req = {
        .req_code = UPDATE_SYSTEM_WEATHER,
        .app_id = 0,
        .req_data = NULL,
        .req_data_len = 0
    };
    ble_ev.ev = EVENT_BLE_REQUEST;
    ble_ev.data = &weather_req;
    ble_ev.payload_len = sizeof(ble_req_t);
    success = event_publish(&ble_ev); 
    if(!success) {
        ESP_LOGE(TAG, "Weather sync ble request failed");
    }
}

void state_manager_deinit(void) {
    alarm_manager_deinit();    
    event_unsubscribe(EVENT_TIME_SYNC, update_time_state_cb);
    event_unsubscribe(EVENT_WEATHER_SYNC, update_weather_state_cb);
    event_unsubscribe(EVENT_NEW_STEPS, update_steps_cb);
}

uint32_t state_manager_get_epoch_time(void) {
    if (!watch_state.time_state.valid)
        return 0;
    time_t now;
    time(&now);
    return (uint32_t)now;
}

const hourly_weather_t* state_manager_get_weather_today(void) {
    return watch_state.weather_state.hourly_today;
}

uint32_t state_manager_get_step_count(void) {
    return watch_state.steps;
}

void state_manager_check_validity(void) {
    // Unimplemented
    bool fire_time_sync = false;
    uint32_t current_uptime = time(NULL);

    if(watch_state.time_state.valid) {
        if(state_manager_get_epoch_time() - watch_state.time_state.last_sync_time > 60*60) {
            
            if (current_uptime - state_registry.time.last_req_time >= 5*60) {
                state_registry.time.last_req_time = current_uptime;
                state_registry.time.request_pending = true;
                fire_time_sync = true;
            }
        }
    }
    else {
        if(current_uptime - state_registry.time.last_req_time >= 5*60) {
            state_registry.time.last_req_time = current_uptime;
            state_registry.time.request_pending = true;
            fire_time_sync = true;
        }
    }

    if(fire_time_sync) {
        ble_req_t time_req = {
            .req_code = UPDATE_SYSTEM_TIME,
            .app_id = 0,
            .req_data_len = 0,
            .req_data = NULL 
        };
        event_t ble_ev = {
           .ev = EVENT_BLE_REQUEST,
           .data = &time_req,
           .payload_len = sizeof(ble_req_t)
        };
        bool success = event_publish(&ble_ev);
        if(!success) {
            ESP_LOGE(TAG, "Time sync ble request failed");
        }
    }
}
