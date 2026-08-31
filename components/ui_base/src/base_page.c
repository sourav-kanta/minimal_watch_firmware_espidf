#include <lvgl.h>
#include <esp_log.h>
#include <base_types.h>
#include <common_types.h>
#include <global_locks.h>
#include <navigation.h>
#include <ui_utils.h>
#include <wf_manager.h>
#include <app_picker_ui.h>
#include <notification_ui.h>
#include <event_screens.h>
#include <ui_base.h>
#include <time.h>
#include <esp_log.h>

static const char* TAG = "Root UI";
static lv_obj_t *root_screen;
static lv_obj_t *home_tab, *wf_page, *notify_page, *app_page;
static ui_state_t curr_watch_state = UISTATE_INVALID;

void close_curr_page(void) {
    switch(curr_watch_state) {
        case WATCHFACE :
            WITH_UI_LOCK() {
                lv_obj_clean(wf_page);
            }
            watchface_manager_stop_wf();
            ESP_LOGI(TAG, "Deleting Watchface UI");
            break;
        case APP :
            ESP_LOGI(TAG, "Deleting App manager UI");
            clean_app_picker_page();            
            break;
        case NOTIFY :
            clean_notification_page();
            lv_obj_clean(notify_page);
            break;
        default :
            ESP_LOGE(TAG, "Unknown page to close");
            break;
    }
}


static void show_page(ui_state_t page) {
    switch(page) {
        case NOTIFY :
            ESP_LOGI(TAG, "Open notification page");
            draw_notification_page(notify_page);
            break;
        case WATCHFACE :
            watchface_manager_start_wf(wf_page);
            break;
        case APP :
            draw_app_picker_page(app_page);
            break;
        default :
            ESP_LOGE(TAG, "Unknown page to show");
            break;
    }

}

static void handle_right_action_on_root(void) {
    if(curr_watch_state !=(UISTATE_INVALID -1)) {
        close_curr_page();
        curr_watch_state++;
        uint32_t act = lv_tabview_get_tab_active(home_tab);
        lv_tabview_set_active(home_tab, act + 1, LV_ANIM_ON);
        show_page(curr_watch_state);
    }
}

static void handle_left_action_on_root(void) {
    if(curr_watch_state != 0) {
        close_curr_page();
        curr_watch_state--;
        uint32_t act = lv_tabview_get_tab_active(home_tab);
        lv_tabview_set_active(home_tab, act - 1, LV_ANIM_ON);
        show_page(curr_watch_state);
    }
}

static void handle_root_scr_actions(lv_event_t *ev) {
    lv_event_code_t code = lv_event_get_code(ev);
    uint32_t key = LV_KEY_HOME; // None key 
    ESP_LOGI(TAG, "callback received %d", code);

    if(code == LV_EVENT_KEY) {
        key = lv_event_get_key(ev);
    }

    if(key == LV_KEY_LEFT) {
        ESP_LOGI(TAG, "Left action");
        handle_left_action_on_root();
    }
    if(key == LV_KEY_RIGHT) {
        ESP_LOGI(TAG, "Right action");
        handle_right_action_on_root();
    }


    if(key == LV_KEY_ENTER) {
        ESP_LOGI(TAG, "Ok button");
        lv_event_stop_bubbling(ev);
        lv_obj_t * focused = lv_group_get_focused(lv_group_get_default());
        if(focused == NULL) return;
        lv_obj_t* target = find_first_focusable_child_dfs(focused);
        if(target) {
            ESP_LOGI(TAG, "BFS found nearest focusable child");
            lv_group_focus_obj(target);
            return;
        }
        else 
            ESP_LOGE(TAG, "Unknown child to navigate to");

    }
    if(key == LV_KEY_ESC) {
        ESP_LOGI(TAG, "Back button");
        lv_obj_t * focused = lv_group_get_focused(lv_group_get_default());
        if(focused == NULL) return;
        lv_event_stop_bubbling(ev);
        lv_obj_t* parent = find_first_focusable_parent_dfs(focused);
        if(parent) 
            lv_group_focus_obj(parent);
        else 
            ESP_LOGE(TAG, "Unknown parent to navigate to");

    }
}


void draw_base_screen(void) {
    ESP_LOGI(TAG, "LVGL Version: %d.%d.%d", 
            LVGL_VERSION_MAJOR,
            LVGL_VERSION_MINOR,
            LVGL_VERSION_PATCH);
    WITH_UI_LOCK() {
        root_screen = lv_scr_act(); 
        home_tab = lv_tabview_create(root_screen);
        // Hide the tab_bar
        lv_tabview_set_tab_bar_size(home_tab, 0);
        lv_obj_t * tab_bar = lv_tabview_get_tab_bar(home_tab);
        lv_obj_remove_flag(tab_bar, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(tab_bar, LV_OBJ_FLAG_CLICK_FOCUSABLE);
        //Make the table view respond to keys
        lv_obj_t* tabview_content = lv_tabview_get_content(home_tab);
        lv_obj_add_flag(tabview_content, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_remove_flag(tabview_content, LV_OBJ_FLAG_SCROLLABLE);
        // Add the pages and make them navigable
        notify_page = lv_tabview_add_tab(home_tab, "Notifications");
        remove_shadow_and_outline(notify_page);
        wf_page = lv_tabview_add_tab(home_tab, "Watchface");
        remove_shadow_and_outline(wf_page);
        app_page = lv_tabview_add_tab(home_tab, "Apps");
        remove_shadow_and_outline(app_page);
        lv_tabview_set_active(home_tab, WATCHFACE, LV_ANIM_ON);
        lv_obj_add_event_cb(tabview_content, 
                            handle_root_scr_actions,
                            LV_EVENT_KEY,
                            NULL);
        make_obj_navigable(tabview_content);
        lv_obj_add_flag(app_page, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_add_flag(wf_page, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_add_flag(notify_page, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_group_focus_obj(tabview_content);
        curr_watch_state = WATCHFACE;
    }
    show_page(curr_watch_state);
    ESP_LOGI(TAG, "Base screen draw complete");
}

void clean_base_screen(void) {
    close_curr_page();
    WITH_UI_LOCK() {
        if(wf_page) 
            lv_obj_clean(wf_page);
        if(app_page)
            lv_obj_clean(app_page);
        if(notify_page)
            lv_obj_clean(notify_page);
    }
}

void suspend_base_screen(void) {
    switch(curr_watch_state) {
        case WATCHFACE :
            watchface_manager_suspend();
            break;
        case NOTIFY :
            break;
        case APP :
            // Implement onStop
            break;
        default :
            break;
    }
}

void resume_base_screen(void) {
    switch(curr_watch_state) {
        case WATCHFACE :
            watchface_manager_resume();
            break;
        case NOTIFY :
            break;
        case APP :
            // Implement onResume
            break;
        default :
            break;
    }
}

void handle_base_screen_event(ui_base_screen_event_t* ui_event) {
    lv_obj_t* parent = NULL;
    switch(curr_watch_state) {
        case WATCHFACE :
            parent = wf_page;
            break;
        case NOTIFY :
            parent = notify_page;
            break;
        case APP :
            parent = app_page;
            break;
        default :
            break;
    }
    if(parent == NULL) return;
    if(!ui_event) {
        ESP_LOGE(TAG, "Invalid event");
        return;
    }
    switch(ui_event->event_type) {
        case BASE_SCREEN_EVENT_ALARM : {
            suspend_base_screen();
            date_time_t dt;
            time_t time_val = (time_t) ui_event->data.alarm_data.epoch;
            struct tm curr_time;
            if (gmtime_r(&time_val, &curr_time) != NULL) {
                dt.day = curr_time.tm_mday;
                dt.month = curr_time.tm_mon + 1;
                dt.year = curr_time.tm_year + 1900;
                dt.hr = curr_time.tm_hour;
                dt.min = curr_time.tm_min;
                dt.sec = curr_time.tm_sec;
                dt.d_week = curr_time.tm_wday;
            }        
            show_alarm_page(parent, &dt, ui_event->on_finish_event_callback);
        }
        break;
        default :
            ESP_LOGE(TAG, "Unknown event type"); 
    }
    
}
