#include <tick_manager.h>
#include <common_types.h>
#include <event_manager.h>
#include <esp_timer.h>
#include <tick_consts.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <esp_log.h>
#include <esp_err.h>

static esp_timer_handle_t tick_timers[TICK_INVALID];
static unsigned int dep_ticks_counter[DEPENDENT_TICK_INVALID] = {0};
static const char* TAG = "Tick Manager";
static int64_t last_work_tick_us = -1;

static void tick_event_cb(void* arg) {
    tick_type_t type = (tick_type_t)(uintptr_t) arg;
    assert(type < TICK_INVALID);
    event_t event = {
        .payload_len = 0,
        .data = NULL
    };
    if(type == TICK_WATCHFACE) {
        event.ev = EVENT_WATCHFACE_UPDATE;
        event_publish(&event);
    }
    else if(type == TICK_WORK) {
        last_work_tick_us = esp_timer_get_time();
        event.ev = EVENT_WORK_TICK;
        event_publish(&event);
        for(int i=0; i<DEPENDENT_TICK_INVALID; i++) {
            dep_ticks_counter[i]++;
            if(dep_ticks_counter[i] >= dep_tick_registry[i].interval_work_ticks) {
                dep_ticks_counter[i] = 0;
                event.ev = dep_tick_registry[i].event_mapping;
                event_publish(&event);
            }
        }
    }
}

void tick_manager_init(void) {
    memset(tick_timers, 0, sizeof(tick_timers));
    memset(dep_ticks_counter, 0, sizeof(dep_ticks_counter));
    for(int i=0; i<TICK_INVALID; i++) {
        esp_timer_create_args_t timer_args = {
            .callback = tick_event_cb,
            .dispatch_method = ESP_TIMER_TASK,
            .name = timer_names[i],
            .arg = (void*)(uintptr_t)i,
        };
        esp_timer_create(&timer_args, &tick_timers[i]);
    }
    last_work_tick_us = -1;
}

void tick_manager_deinit(void) {
    for(int i=0; i<TICK_INVALID; i++) {
        if(tick_timers[i]) {
            esp_timer_stop(tick_timers[i]);
            esp_timer_delete(tick_timers[i]);
            tick_timers[i] = NULL;
        }
    }
}

bool tick_manager_shift_work_tick(unsigned int delta_us) {
    if(!tick_timers[TICK_WORK]) {
        ESP_LOGE(TAG, "Work tick timer is invalid, discarding new shift");
        return false;
    }
    else {
        if(last_work_tick_us < 0) {
            ESP_LOGE(TAG, "No work ticks yet, skipping shift");
            return false;
        }
        uint64_t next_work_tick_us = last_work_tick_us +
                                     tick_intervals_ms[TICK_WORK] * 1000ULL;
        esp_err_t err = esp_timer_restart_at(tick_timers[TICK_WORK],
                                             tick_intervals_ms[TICK_WORK]*1000ULL,
                                             next_work_tick_us + delta_us);
        if(err != ESP_OK) {
            ESP_LOGE(TAG, "Error restarting work tick timer : %s", esp_err_to_name(err));
            return false;
        }
    }
    return true;
}

void tick_manager_generate_tick(tick_type_t type) {
    if(type >= TICK_INVALID) return;
    if(tick_timers[type]) {
        esp_timer_start_periodic(tick_timers[type], tick_intervals_ms[type]*1000ULL);
    }
}

void tick_manager_stop_tick(tick_type_t type) {
    if(type >= TICK_INVALID) return;
    if(tick_timers[type]) {
        esp_timer_stop(tick_timers[type]);
    }
    if(type == TICK_WORK) {
        last_work_tick_us = -1;
    }
}

unsigned int tick_manager_get_tick_interval_ms(tick_type_t type) {
    if(type >= TICK_INVALID) return 0;
    return tick_intervals_ms[type];
}
