#include <draw_tab_page.h>

#include <lvgl.h>
#include <ui_utils.h>
#include <global_locks.h>
#include <navigation.h>
#include <esp_log.h>

static const char* TAG = "Tab Shell";
static lv_obj_t* shell = NULL;

static void handle_tab_screen_keys_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e); 
        lv_obj_t* curr_focused = lv_group_get_focused(lv_group_get_default());
        if(key == LV_KEY_ESC) {
            if(!curr_focused) return;
            if(curr_focused == shell) {
                lv_event_stop_bubbling(e);
            }
        }
        if(key == LV_KEY_RIGHT) {
            ESP_LOGI(TAG, "Finding next sibling");
            if(!curr_focused) return;
            if(curr_focused == shell) {
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
            if(curr_focused == shell) {
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


void draw_tab_page(lv_obj_t* parent, ui_tab_handlers_t* tab) {
    assert(shell == NULL);
    WITH_UI_LOCK() {
        shell = lv_obj_create(parent);
        remove_shadow_and_outline(shell);
        make_obj_navigable(shell);
        lv_group_focus_obj(shell);
        lv_obj_set_size(shell, DISPLAY_LCD_H_RES, DISPLAY_LCD_V_RES);
        lv_obj_set_style_pad_all(shell, 0, LV_PART_MAIN);
        lv_obj_add_event_cb(shell, handle_tab_screen_keys_cb, LV_EVENT_KEY, NULL);
        tab->on_draw(shell);
    }
}

void clean_tab_page(ui_tab_handlers_t* tab) {
    tab->on_close();
    WITH_UI_LOCK() {
        if(shell)
            lv_obj_delete(shell);
    }
    shell = NULL;
}

