#include <wf_manager.h>
#include <esp_log.h>
#include <common_consts.h>
#include <event_manager.h>
#include <tick_manager.h>
#include <common_types.h>
#include <lvgl.h>
#include <common_apis.h>

#include <wf_abstract_dark.h>
#include <wf_analog.h>

static watchface_t* all_wfs[MAX_WATCHFACES];
static watchface_t* selected_wf = NULL;
static watchface_t* curr_wf = NULL;
static const char* TAG = "Watchface Manager";
static bool initialized = false;

static int num_wfs=0;

static void register_wf(watchface_t* wf) {
    if(num_wfs >= MAX_WATCHFACES) {
        return;
    }
    all_wfs[num_wfs] = wf;
    ++num_wfs;
    ESP_LOGI(TAG, "Adding watchfaces : %s", wf->name);
}

watchface_t* select_wf() {
    
    ESP_LOGI(TAG, "No of watchfaces : %d", num_wfs);
    if(num_wfs == 0) {
        ESP_LOGE(TAG, "No watchfaces present");
    }

    // @todo get from settings
    selected_wf = all_wfs[0];
    return selected_wf;
}

void watchface_manager_init(void) {
    if(initialized) {
        return;
    }
    
    initialized = true;
    register_wf(get_analog_wf());
    register_wf(get_abstract_dark_wf());

    select_wf();
}

void watchface_manager_deinit(void) {
    num_wfs = 0;
    initialized = false;
}

static void dispatch_wf_update(const event_t *event) {
    if(!curr_wf) return;
    date_time_t date;
    hourly_weather_t weather[24];
    get_date_time(&date);
    get_weather_day(weather);
    wf_update_payload_t payload = {
        .time = date,
        .weather = weather[date.hr]
    };
    curr_wf->update_watchface(&payload);
}

void watchface_manager_suspend(void) {
    if (!curr_wf)
        return;
    event_unsubscribe(EVENT_WATCHFACE_UPDATE, dispatch_wf_update);
    tick_manager_stop_tick(TICK_WATCHFACE);
}

void watchface_manager_resume(void) {
    if (!curr_wf)
        return;
    event_subscribe(EVENT_WATCHFACE_UPDATE, dispatch_wf_update);
    tick_manager_generate_tick(TICK_WATCHFACE);
}

void watchface_manager_start_wf(lv_obj_t *parent) {
    if(!selected_wf) return;
    curr_wf = selected_wf;
    ESP_LOGI(TAG, "Selected watchface : %s",
            curr_wf->name);
    curr_wf->draw_watchface(parent);
    event_subscribe(EVENT_WATCHFACE_UPDATE, dispatch_wf_update);
    tick_manager_generate_tick(TICK_WATCHFACE);
}

void watchface_manager_stop_wf(void) {
    if(!curr_wf) {
        ESP_LOGE(TAG, "Unknown watchface to stop");
        return;
    }
    if(curr_wf->del_watchface!=NULL)
        curr_wf->del_watchface();
    event_unsubscribe(EVENT_WATCHFACE_UPDATE, dispatch_wf_update);
    tick_manager_stop_tick(TICK_WATCHFACE);
    curr_wf = NULL;
}
