#include <ui_base.h>

#include <lvgl.h>
#include <esp_log.h>
#include <common_types.h>
#include <global_locks.h>
#include <navigation.h>
#include <ui_utils.h>
#include <event_screens.h>
#include <time.h>
#include <draw_tab_page.h>

#define UI_BASE_MAX_TABS 3

static const char* TAG = "Root UI";
static lv_obj_t *root_screen;
static lv_obj_t *home_tab=NULL;
static ui_state_t curr_watch_state = UISTATE_INVALID;
static lv_obj_t* tabs[UI_BASE_MAX_TABS];
static ui_tab_handlers_t ui_tab_handlers[UI_BASE_MAX_TABS];

static void close_curr_page(void) {
    assert(curr_watch_state >= NOTIFY && curr_watch_state < UISTATE_INVALID);
    assert(ui_tab_handlers[curr_watch_state].on_close);
    clean_tab_page(&ui_tab_handlers[curr_watch_state]);
    assert(tabs[curr_watch_state]);
    WITH_UI_LOCK() {
        lv_obj_clean(tabs[curr_watch_state]);
    }
}


static void show_page(ui_state_t page) {
    assert(page >= NOTIFY && page < UISTATE_INVALID);
    assert(ui_tab_handlers[page].on_draw);
    draw_tab_page(tabs[page], &ui_tab_handlers[page]);
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

void ui_base_register_tab(ui_state_t page, ui_tab_handlers_t* tab_handler) {
    assert(tab_handler);
    assert(page >= NOTIFY && page < UISTATE_INVALID);
    memcpy(&ui_tab_handlers[page], tab_handler, sizeof(ui_tab_handlers_t));
}

void ui_base_draw_base_screen(void) {
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
        tabs[NOTIFY] = lv_tabview_add_tab(home_tab, "Notifications");
        remove_shadow_and_outline(tabs[NOTIFY]);
        tabs[WATCHFACE] = lv_tabview_add_tab(home_tab, "Watchface");
        remove_shadow_and_outline(tabs[WATCHFACE]);
        tabs[APP] = lv_tabview_add_tab(home_tab, "Apps");
        remove_shadow_and_outline(tabs[APP]);
        lv_tabview_set_active(home_tab, WATCHFACE, LV_ANIM_ON);
        lv_obj_add_event_cb(tabview_content, 
                            handle_root_scr_actions,
                            LV_EVENT_KEY,
                            NULL);
        make_obj_navigable(tabview_content);
        lv_obj_add_flag(tabs[NOTIFY], LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_add_flag(tabs[WATCHFACE], LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_add_flag(tabs[APP], LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_group_focus_obj(tabview_content);
        curr_watch_state = WATCHFACE;
    }
    show_page(curr_watch_state);
    ESP_LOGI(TAG, "Base screen draw complete");
}

void ui_base_clean_base_screen(void) {
    close_curr_page();
    WITH_UI_LOCK() {
        for(int i=0; i<UI_BASE_MAX_TABS; i++) {
            if(tabs[i]) {
                lv_obj_clean(tabs[i]);
            }
        }
    }
}

void ui_base_suspend_base_screen(void) {
    assert(curr_watch_state >= NOTIFY && curr_watch_state < UISTATE_INVALID);
    if(ui_tab_handlers[curr_watch_state].on_suspend) {
        ui_tab_handlers[curr_watch_state].on_suspend();
    }
}

void ui_base_resume_base_screen(void) {
    assert(curr_watch_state >= NOTIFY && curr_watch_state < UISTATE_INVALID);
    if(ui_tab_handlers[curr_watch_state].on_resume) {
        ui_tab_handlers[curr_watch_state].on_resume();
    }
}

void ui_base_handle_base_screen_event(ui_base_screen_event_t* ui_event) {
    assert(curr_watch_state >= NOTIFY && curr_watch_state < UISTATE_INVALID);
    lv_obj_t* parent = tabs[curr_watch_state];
    if(parent == NULL) return;
    if(!ui_event) {
        ESP_LOGE(TAG, "Invalid event");
        return;
    }
    switch(ui_event->event_type) {
        case BASE_SCREEN_EVENT_ALARM : {
            ui_base_suspend_base_screen();
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
            show_alarm_page(parent, &dt);
        }
        break;
        default :
            ESP_LOGE(TAG, "Unknown event type"); 
    }
    
}
