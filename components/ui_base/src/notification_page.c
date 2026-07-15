#include <notification_manager.h>
#include <lvgl.h>
#include <ui_utils.h>
#include <global_locks.h>
#include <navigation.h>
#include <esp_log.h>
#include <notification_ui.h>

static const char* TAG = "Notification Ui";
static lv_obj_t* notification_shell;

static void handle_notification_screen_keys_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e); 
        lv_obj_t* curr_focused = lv_group_get_focused(lv_group_get_default());
        if(key == LV_KEY_ESC) {
            if(!curr_focused) return;
            if(curr_focused == notification_shell) {
                lv_event_stop_bubbling(e);
            }
        }
        if(key == LV_KEY_RIGHT) {
            ESP_LOGI(TAG, "Finding next sibling");
            if(!curr_focused) return;
            if(curr_focused == notification_shell) {
                return;
            }
            lv_obj_t* next_sibling = find_next_focusable_sibling(curr_focused);
            if(next_sibling) {
                WITH_UI_LOCK() {
                    lv_group_focus_obj(next_sibling);
                }
            }
            else 
                ESP_LOGE(TAG, "Cant find next sibling");
            lv_event_stop_bubbling(e); 
        }
        if(key == LV_KEY_LEFT) {
            ESP_LOGI(TAG, "Finding prev sibling");
            if(!curr_focused) return;
            if(curr_focused == notification_shell) {
                return;
            }
            lv_obj_t* prev_sibling = find_prev_focusable_sibling(curr_focused);
            if(prev_sibling) {
                WITH_UI_LOCK() {
                    lv_group_focus_obj(prev_sibling);
                }
            }
            else 
                ESP_LOGE(TAG, "Cant find previous sibling");
            lv_event_stop_bubbling(e); 
        }
    }
}


void draw_notification_page(lv_obj_t* parent) {
    WITH_UI_LOCK() {
        notification_shell = lv_obj_create(parent);
        remove_shadow_and_outline(notification_shell);
        make_obj_navigable(notification_shell);
        lv_group_focus_obj(notification_shell);
        lv_obj_set_size(notification_shell, DISPLAY_LCD_H_RES, DISPLAY_LCD_V_RES);
        lv_obj_set_style_pad_all(notification_shell, 0, LV_PART_MAIN);
        lv_obj_add_event_cb(notification_shell, handle_notification_screen_keys_cb, LV_EVENT_KEY, NULL);
        notification_manager_draw_notification_page(notification_shell);
    }
}

void clean_notification_page(void) {
    notification_manager_delete_ui();
    WITH_UI_LOCK() {
        if(notification_shell)
            lv_obj_delete(notification_shell);
    }
    notification_shell = NULL;
}

