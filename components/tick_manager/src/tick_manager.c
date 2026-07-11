#include <tick_manager.h>
#include <common_types.h>
#include <event_manager.h>
#include <esp_timer.h>
#include <tick_consts.h>

static esp_timer_handle_t wf_update_timer = NULL;
static esp_timer_handle_t work_tick_timer = NULL;

static void wf_update_cb(void* arg) {
    event_t event = {
        .ev = EVENT_WATCHFACE_UPDATE,
        .payload_len = 0,
        .data = NULL
    };
    event_publish(&event);
}

static void work_timer_cb(void* arg) {
    event_t event = {
        .ev = EVENT_WORK_TICK,
        .payload_len = 0,
        .data = NULL
    };
    event_publish(&event);
}

void tick_manager_init(void) {
    esp_timer_create_args_t wf_timer_args = {
        .callback = wf_update_cb,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "Watchface Update Timer"
    };
    esp_timer_create(&wf_timer_args, &wf_update_timer);
    esp_timer_create_args_t work_timer_args = {
        .callback = work_timer_cb,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "Watchface Update Timer"
    };
    esp_timer_create(&work_timer_args, &work_tick_timer);
}

void tick_manager_deinit(void) {
    if(wf_update_timer) {
        esp_timer_stop(wf_update_timer);
        esp_timer_delete(wf_update_timer);
        wf_update_timer = NULL;
        esp_timer_stop(work_tick_timer);
        esp_timer_delete(work_tick_timer);
        work_tick_timer = NULL;
    }
}

void tick_manager_generate_tick(tick_type_t type) {
    switch(type) {
        case TICK_WATCHFACE :
            if(wf_update_timer)
                esp_timer_start_periodic(wf_update_timer, WF_UPDATE_TICK_MS*1000);
            break;
        case TICK_WORK :
            if(work_tick_timer) {
                esp_timer_start_periodic(work_tick_timer, WORK_TICK_MS*1000);
            }
            break;
        default :
            break;
    }
}

void tick_manager_stop_tick(tick_type_t type) {
    switch(type) {
        case TICK_WATCHFACE :
            if(wf_update_timer) {
                esp_timer_stop(wf_update_timer);
            }
            break;
        case TICK_WORK :
            if(work_tick_timer) {
                esp_timer_stop(work_tick_timer);
            }
            break;
        default :
            break;
    }
}
