#include <wf_manager.h>
#include <esp_log.h>
#include <common_consts.h>
#include <event_manager.h>
#include <tick_manager.h>
#include <common_types.h>
#include <lvgl.h>
#include <common_apis.h>
#include <storage_manager.h>

#include <wf_abstract_dark.h>
#include <wf_analog.h>
#include <wf_retro.h>

static watchface_t* all_wfs[MAX_WATCHFACES];
static const watchface_t* selected_wf = NULL;
static const watchface_t* curr_wf = NULL;
static const char* TAG = "Watchface Manager";
static bool initialized = false;

static int num_wfs=0;

static void register_wf(watchface_t* wf) {
    if(!initialized) return;
    if(num_wfs >= MAX_WATCHFACES) {
        return;
    }
    all_wfs[num_wfs] = wf;
    assert(all_wfs[num_wfs]);
    ++num_wfs;
    ESP_LOGI(TAG, "Adding watchfaces : %s", wf->name);
}

bool watchface_manager_select_wf(uint8_t idx) {
    if(!initialized) return false;
    ESP_LOGI(TAG, "No of watchfaces : %d", num_wfs);
    if(num_wfs == 0 || num_wfs <= idx) {
        ESP_LOGE(TAG, "Invalid watchface selection");
        return false;
    }

    selected_wf = all_wfs[idx];
    bool success = storage_manager_save_key(WATCHFACE_SYSTEM_APP_ID, "WF_IDX", &idx, sizeof(uint8_t));
    if(!success) {
        ESP_LOGE(TAG, "Failed to store the selected watchface");
    }
    return success;
}

static const watchface_t* watchface_manager_get_selected_wf(void) {
    if(!initialized) return NULL;
    if(selected_wf == NULL) {
        uint8_t idx;
        uint8_t data_len = sizeof(uint8_t);
        bool success = storage_manager_retrieve_key(WATCHFACE_SYSTEM_APP_ID, "WF_IDX", &idx, &data_len);
        if(success) {
            assert(data_len == sizeof(uint8_t));
            if(idx >= num_wfs) {
                ESP_LOGE(TAG, "The selected watchface is no longer available. Fallback to default");
                idx = 0;
            }
            selected_wf = all_wfs[idx];
            assert(selected_wf);
        }
        else {
            // No watchface selection, first time use case
            // Use the first one available
            uint8_t idx = 0;
            selected_wf = all_wfs[idx];
            assert(selected_wf);
            // Store selected watchface in flash memory
            watchface_manager_select_wf(idx);
        }
    }
    ESP_LOGI(TAG, "Selecting watchface : %s", selected_wf->name);
    return selected_wf;
}

size_t watchface_manager_get_total_wfs(void) {
    if(!initialized) return 0;
    return num_wfs;
} 

size_t watchface_manager_get_all_wf_names(const char** names) {
    if(!initialized) return 0;
    assert(names);
    for(int i=0; i<num_wfs; i++) {
        names[i] = all_wfs[i]->name;
    }
    return num_wfs;
}

const char* watchface_manager_get_selected_wf_name(void) {
    if(!initialized || selected_wf == NULL) return NULL;
    return selected_wf->name;
}

void watchface_manager_init(void) {
    if(initialized) {
        return;
    }
    
    initialized = true;
    register_wf(get_retro_wf());
    register_wf(get_analog_wf());
    register_wf(get_abstract_dark_wf());
}

void watchface_manager_deinit(void) {
    num_wfs = 0;
    initialized = false;
    curr_wf = NULL;
    selected_wf = NULL;
}

static void dispatch_wf_update(const event_t *event) {
    if(!initialized) return;
    if(!curr_wf) return;
    date_time_t date;
    hourly_weather_t weather[24];
    get_date_time(&date);
    get_weather_day(weather);
    wf_update_payload_t payload = {
        .time = date,
        .weather = weather[date.hr]
    };
    assert(curr_wf);
    assert(curr_wf->update_watchface);
    curr_wf->update_watchface(&payload);
}

void watchface_manager_suspend(void) {
    if(!initialized) return;
    if (!curr_wf)
        return;
    event_unsubscribe(EVENT_WATCHFACE_UPDATE, dispatch_wf_update);
    tick_manager_stop_tick(TICK_WATCHFACE);
}

void watchface_manager_resume(void) {
    if(!initialized) return;
    if (!curr_wf)
        return;
    event_subscribe(EVENT_WATCHFACE_UPDATE, dispatch_wf_update);
    tick_manager_generate_tick(TICK_WATCHFACE);
}

void watchface_manager_start_wf(lv_obj_t *parent) {
    if(!initialized) return;
    curr_wf = watchface_manager_get_selected_wf();
    assert(curr_wf);
    assert(curr_wf->draw_watchface);
    curr_wf->draw_watchface(parent);
    event_subscribe(EVENT_WATCHFACE_UPDATE, dispatch_wf_update);
    tick_manager_generate_tick(TICK_WATCHFACE);
}

void watchface_manager_stop_wf(void) {
    if(!initialized) return;
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
