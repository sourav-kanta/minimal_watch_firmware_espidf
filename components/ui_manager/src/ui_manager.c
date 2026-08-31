#include <ui_manager.h>
#include <lvgl_bridge.h>
#include <ui_base.h>
#include <wf_manager.h>
#include <event_manager.h>
#include <common_types.h>
#include <runtime_manager.h>
#include <esp_assert.h>
#include <string.h>
#include <esp_log.h>
#include <stdatomic.h>

static bool initialized = false;
static const char* TAG = "UI Manager";
static atomic_int wakelock_counter = 0;

void ui_manager_acquire_wakelock(void) {
    atomic_fetch_add(&wakelock_counter, 1);
    ESP_LOGD(TAG, "Acquired wakelock");
}

void ui_manager_release_wakelock(void) {
    if(atomic_fetch_add(&wakelock_counter, -1) <= 0) {
        ESP_LOGE(TAG, "Invalid wakelock release. Panic!!!!!");
    }
    else {
        ESP_LOGD(TAG, "Released wakelock");
    }
}

bool ui_manager_is_wake_locked(void) {
    return atomic_load(&wakelock_counter) > 0;
}

ESP_STATIC_ASSERT(sizeof(ui_base_screen_event_t) <= MAX_WORKER_ARG_PAYLOAD, "Worker struct overflow");

static void handle_ui_event_cb(void* arg, runtime_abort_flag_t* flag) {
    if(arg != NULL) {
        ui_manager_acquire_wakelock();
        ui_base_screen_event_t* ui_ev = (ui_base_screen_event_t*) arg;
        handle_base_screen_event(ui_ev);
    }    
}

static void alarm_triggred_event_cb(const event_t* event) {
    if(event == NULL || event->data == NULL) {
        return;
    }
    assert(event->payload_len == sizeof(alarm_t));
    runtime_work_item_t alarm_work = {
        .handler = handle_ui_event_cb,
        .type = WORK_TYPE_SYSTEM,
    };
    ui_base_screen_event_t ui_event;
    ui_event.event_type = BASE_SCREEN_EVENT_ALARM;
    ui_event.on_finish_event_callback = ui_manager_release_wakelock;
    memcpy(&ui_event.data.alarm_data, event->data, sizeof(alarm_t));
    memcpy(alarm_work.arg_payload, &ui_event, sizeof(ui_event));
    if(!schedule_system_work(&alarm_work)) {
        ESP_LOGE(TAG, "Failed to schedule alarm work");
    }   
}

void ui_on(void) {
    ESP_LOGI(TAG, "Turning on UI"); 
    wakelock_counter = 0;
    lvgl_params_t lvgl_param = { .wakelock_check_func = ui_manager_is_wake_locked }; 
    init_lvgl(&lvgl_param);
    draw_base_screen();
}

void ui_off(void) {
    ESP_LOGI(TAG, "Turning off UI"); 
    clean_base_screen();
    deinit_lvgl();
    wakelock_counter = 0;
}

void ui_sleep(void) {
    ESP_LOGI(TAG, "Putting UI to sleep"); 
    suspend_base_screen();
    suspend_lvgl();
}

void ui_resume(void) {
    ESP_LOGI(TAG, "Waking UI"); 
    resume_lvgl();
    resume_base_screen();
}

void ui_manager_init(void) {
    if(initialized)
        return;
    watchface_manager_init();
    initialized = true;
    event_subscribe(EVENT_ALARM_TRIGGERED, alarm_triggred_event_cb);
    ESP_LOGI(TAG, "Initialized successfully"); 
}

void ui_manager_deinit(void) {
    event_unsubscribe(EVENT_ALARM_TRIGGERED, alarm_triggred_event_cb);
    ui_off();
    watchface_manager_deinit();
    initialized = false;    
    ESP_LOGI(TAG, "Deinitialized successfully"); 
}

