#include <lvgl.h>
#include <ui_theme.h>
#include <ui_utils.h>
#include <global_locks.h>
#include <listview.h>
#include <esp_log.h>

#include <popup_listview.h>

static const int divider_width = 1;
static const char* TAG = "Popup Listview";

static void draw_popup_horizoantal_divider(lv_obj_t* parent) {
    lv_obj_t* div = lv_obj_create(parent);
    lv_obj_set_size(div, lv_pct(100), divider_width);
    lv_obj_align(div, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(div, COLOR_THEME_FOCUS_BG, LV_STATE_DEFAULT); 
}


static void dismiss_popup(popup_registry_t* registry) {
    assert(registry);
    WITH_UI_LOCK() {
        assert(registry->popup);
        lv_obj_delete(registry->popup);
    }
}

static void popup_event_cb(lv_event_t* event) {
    WITH_UI_LOCK() {
        lv_obj_t* focused = lv_group_get_focused(lv_group_get_default());
        assert(focused);
        lv_obj_t* popup = lv_event_get_user_data(event);
        assert(popup);
        if(lv_event_get_key(event) == LV_KEY_ENTER) {
            // Let enter bubble through
        }
        else if(popup == focused) {
            if(lv_event_get_key(event) == LV_KEY_ESC) {
                popup_registry_t* registry = lv_obj_get_user_data(popup);
                assert(registry);
                dismiss_popup(registry);
            }
            lv_event_stop_bubbling(event);
        }
    }
}

static void delete_popup_cb(lv_event_t* event) {
    lv_obj_t* popup = lv_event_get_target_obj(event);
    popup_registry_t* registry = lv_obj_get_user_data(popup);
    assert(registry);
    if(registry->item_data_array != NULL) {
        ESP_LOGD(TAG, "Freeing item array of popup");
        free(registry->item_data_array);
        registry->item_data_array = NULL;
        registry->item_count = 0;
        registry->popup = NULL;
    }
}

static lv_obj_t* draw_popup(lv_obj_t* parent) {
    lv_obj_t* popup_container = lv_obj_create(parent);
    remove_shadow_and_outline(popup_container);
    lv_obj_set_size(popup_container, lv_pct(60), lv_pct(60));
    lv_obj_align(popup_container, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(popup_container, 5, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(popup_container, 5, LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(popup_container, COLOR_THEME_FOCUS_BG, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(popup_container, COLOR_THEME_SECONDARY, LV_STATE_DEFAULT);
    lv_obj_add_flag(popup_container, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(popup_container, popup_event_cb, LV_EVENT_KEY, (void *)popup_container);
    lv_obj_add_event_cb(popup_container, delete_popup_cb, LV_EVENT_DELETE, NULL);
    make_obj_navigable(popup_container);
    lv_group_focus_obj(popup_container);
    lv_obj_move_foreground(popup_container);
    return popup_container;
}

static void select_item_cb(void *data) {
    assert(data);
    popup_item_data_t *item_data = data;
    int selected_index = item_data->index;
    popup_registry_t* parent_registry = item_data->popup_registry;
    assert(parent_registry);
    if(parent_registry->select_cb) {
        parent_registry->select_cb(selected_index);
    }
    dismiss_popup(parent_registry);
}

void create_popup_listview(lv_obj_t* parent_container, popup_registry_t *popup_registry, 
                           listview_t *list_items, int item_count, void (*select_cb)(int index)) {
    assert(parent_container);
    assert(popup_registry->popup == NULL);
    assert(popup_registry->item_data_array == NULL);
    assert(item_count > 0);
    assert(list_items);
    popup_registry->item_count = item_count;
    popup_registry->item_data_array = calloc(item_count, sizeof(popup_item_data_t));
    if(!popup_registry->item_data_array) {
        ESP_LOGE(TAG, "Failed to allocate memory for popup");
        return;
    }

    WITH_UI_LOCK() {
        lv_obj_t* popup = draw_popup(parent_container);
        popup_registry->popup = popup;
        popup_registry->select_cb = select_cb;
        lv_obj_set_user_data(popup, (void*) popup_registry);
        lv_obj_set_layout(popup, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(popup, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_bg_color(popup, COLOR_THEME_SECONDARY, LV_STATE_DEFAULT);
        lv_obj_add_flag(popup, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_scrollbar_mode(popup, LV_SCROLLBAR_MODE_OFF);
        draw_popup_horizoantal_divider(popup);
        for(int i=0; i<item_count; i++) {
            popup_registry->item_data_array[i].index = i;
            popup_registry->item_data_array[i].popup_registry = popup_registry; 
            list_items[i].data = &popup_registry->item_data_array[i];
            list_items[i].title_click_cb = select_item_cb;
            draw_listview_item(popup, &list_items[i]);
            draw_popup_horizoantal_divider(popup);
        }
    }
}
