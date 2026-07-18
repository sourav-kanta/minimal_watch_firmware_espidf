#include <app_manager.h>
#include <lvgl.h>
#include <esp_log.h>
#include <common_consts.h>
#include <app_utils.h>
#include <picker_ui.h>
#include <runtime_types.h> 
#include <runtime_manager.h>
#include <event_manager.h>
#include <weather_app.h>
#include <brickbreaker_game.h>

static const char* TAG = "App Manager";
static application_t* all_apps[MAX_APPS];
static const application_t* curr_app = NULL;
static uint8_t num_apps = 0;
static bool is_app_running = false;
static bool initialized = false;

static void add_app(application_t* app) {
    if (!app || num_apps >= MAX_APPS) {
        ESP_LOGE(TAG, "Unable to register app, skipping");
        return;
    }
    app->app_id = num_apps + 1;
    all_apps[num_apps++] = app;
}

static void dispatch_app_update(void* arg, runtime_abort_flag_t* flag) {
    app_update_t* update = (app_update_t*)arg;
    if(!curr_app) {
        ESP_LOGE(TAG, "No application currently active.");
        return;
    }

    if(get_system_app_id(curr_app) != update->req_app) {
        ESP_LOGE(TAG, "App no longer in focus. Discarding response");
        return;
    }

    if(curr_app->handle_event) {
        (curr_app->handle_event)(update);
    }
}

static void receive_app_update(const event_t *event) {
    if(!event || !initialized) {
        ESP_LOGE(TAG, "Invalid app update");
        return;
    }
    runtime_work_item_t work = {
        .handler = dispatch_app_update,
        .type = WORK_TYPE_USER,
    };
    memcpy(work.arg_payload, event->data, sizeof(app_update_t));
    schedule_user_work(&work); 
}

void app_manager_init(void) {
    if(initialized) return;
    num_apps = 0;
    curr_app = NULL;
    is_app_running = false;
    initialized = true;
    add_app(get_weather_app());
    add_app(get_brickbreaker_game());
    event_subscribe(EVENT_APP_WORK_SCHEDULE, receive_app_update);
}

void app_manager_deinit(void) {
    event_unsubscribe(EVENT_APP_WORK_SCHEDULE, receive_app_update);
    num_apps = 0;
    if(curr_app) {
        close_curr_app();
        curr_app = NULL;
    }
    del_app_picker_ui();
    is_app_running = false;
    memset(all_apps, 0, sizeof(all_apps));
    initialized = false;
}

void open_app(const application_t* app, lv_obj_t* parent) {
    
    if(!app || !parent || !initialized) {
        ESP_LOGE(TAG, "Invalid app or parent object or manager uninitailized, skipping");
        return;
    }

    if(is_app_running)
        close_curr_app();
    ESP_LOGI(TAG, "Opening app %s", app->name);
    curr_app = app;
    is_app_running = true;
    app->draw_app(parent);
}

void close_curr_app(void) {
    if (!is_app_running || !curr_app || !initialized) {
        ESP_LOGE(TAG, "No running app");
        return;
    }
    
    curr_app->close_app();
    ESP_LOGI(TAG, "Closing app %s", curr_app->name);
    
    curr_app = NULL;
    is_app_running = false;
}

void show_app_picker_ui(lv_obj_t* parent) {
    if(!parent || !initialized) {
        ESP_LOGE(TAG, "Invalid parent to draw on");
        return;
    }

    ESP_LOGI(TAG, "Drawing the app manager UI");
    draw_app_picker_ui(parent, all_apps, num_apps);
}

void clean_app_picker_ui(void) {
    del_app_picker_ui();
} 

bool check_if_app_running(void) {
    return initialized && is_app_running;
}

const application_t* get_current_app(void) {
    return initialized? curr_app : NULL;
}


