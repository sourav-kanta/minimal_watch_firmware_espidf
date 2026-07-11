#include <lvgl.h>
#include <navigation.h>
#include <esp_log.h>
#include <ui_utils.h>
#include <global_locks.h>
#include <app_manager.h>
#include <common_consts.h>

static const char* TAG = "App picker Ui";
static lv_obj_t* base_picker_obj;

static void handle_app_screen_keys_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e); 
        lv_obj_t* curr_focused = lv_group_get_focused(lv_group_get_default());
        if(key == LV_KEY_ESC) {
            if(!curr_focused) return;
            if(curr_focused == base_picker_obj) {
                lv_event_stop_bubbling(e);
            }
        }
        if(key == LV_KEY_RIGHT) {
            ESP_LOGI(TAG, "Finding next sibling");
            if(!curr_focused) return;
            if(curr_focused == base_picker_obj) {
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
            if(curr_focused == base_picker_obj) {
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


void draw_app_picker_page(lv_obj_t* parent) {
    WITH_UI_LOCK() {
        base_picker_obj = lv_obj_create(parent);
        remove_shadow_and_outline(base_picker_obj);
        make_obj_navigable(base_picker_obj);
        lv_group_remove_obj(base_picker_obj);
        lv_obj_set_size(base_picker_obj, DISPLAY_LCD_H_RES, DISPLAY_LCD_V_RES);
        lv_obj_set_style_pad_all(base_picker_obj, 0, LV_PART_MAIN);
        lv_obj_add_event_cb(base_picker_obj, handle_app_screen_keys_cb, LV_EVENT_KEY, NULL);
        show_app_picker_ui(base_picker_obj);
    }
}

void clean_app_picker_page(void) {
    del_app_picker_ui();
    WITH_UI_LOCK() {
        if(base_picker_obj)
            lv_obj_delete(base_picker_obj);
    }
    base_picker_obj = NULL;
}
