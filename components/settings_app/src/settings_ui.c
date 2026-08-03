#include <settings_app.h>
#include <ui_theme.h>
#include <ui_utils.h>
#include <common_types.h>
#include <app_types.h>
#include <esp_log.h>
#include <app_utils.h>
#include <lvgl.h>
#include <global_locks.h>
#include <listview.h>
#include <popup_listview.h>
#include <wf_manager.h>
#include <common_consts.h>
#include <gpio_manager.h>

#include "assets/settings_ico.h"

static lv_obj_t* base_container = NULL;
static lv_obj_t* parent_container = NULL;
static const int divider_width = 1;
static const char* TAG = "Settings app";
static popup_registry_t timeout_popup_registry;
static popup_registry_t brightness_popup_registry;
static popup_registry_t wf_popup_registry;
static size_t wf_count = 0;

static void open_watchface_selection_popup(void *data);
static void open_timeout_selection_popup(void *data);
static void open_brightness_selection_popup(void *data);

static listview_t list_items[] = { 
    {
        .title = "Watchface",
        .value = "Retro",
        .data = NULL,
        .title_click_cb = open_watchface_selection_popup
    },
    {
        .title = "Sleep timeout",
        .value = "5s",
        .data = NULL,
        .title_click_cb = open_timeout_selection_popup
    },
    {
        .title = "Brightness",
        .value = "100%",
        .data = NULL,
        .title_click_cb = open_brightness_selection_popup
    },
    {
        .title = "Unpair Phone",
        .value = NULL,
        .data = NULL,
        .title_click_cb = NULL
    },
};

static void draw_horizoantal_divider(lv_obj_t* parent) {
    lv_obj_t* div = lv_obj_create(parent);
    lv_obj_set_size(div, lv_pct(100), divider_width);
    lv_obj_align(div, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(div, COLOR_THEME_FOCUS_BG, LV_STATE_DEFAULT); 
}

static listview_t *wf_items = NULL;

static void select_watchface(int index) {
    ESP_LOGD(TAG, "Selected watchface index : %d", index);
    bool success = watchface_manager_select_wf(index);
    const char* selected_wf = watchface_manager_get_selected_wf_name();
    // Set value label
    lv_obj_t* value_lbl = list_items[0].value_label;
    if(success && value_lbl != NULL && selected_wf != NULL) {
        WITH_UI_LOCK() {
            lv_label_set_text(value_lbl, selected_wf);
        }
    }
}

static void open_watchface_selection_popup(void *data) {
    assert(parent_container);
    create_popup_listview(parent_container, &wf_popup_registry, wf_items, 
                          wf_count, select_watchface);
}

static listview_t timeout_items[] = {
    {
        .title = "5s",
        .value = NULL,
    },
    {
        .title = "7s",
        .value = NULL,
    },
    {
        .title = "10s",
        .value = NULL,
    },
    {
        .title = "None",
        .value = NULL,
    },
};

static void select_timeout(int index) {
    ESP_LOGE(TAG, "Selected timeout index : %d", index);
}

static void open_timeout_selection_popup(void *data) {
    int item_count = sizeof(timeout_items) / sizeof(timeout_items[0]);
    assert(parent_container);
    create_popup_listview(parent_container, &timeout_popup_registry, timeout_items, 
                          item_count, select_timeout); 
}

static listview_t brightness_items[] = {
    {
        .title = "10%",
        .value = NULL,
    },
    {
        .title = "25%",
        .value = NULL,
    },
    {
        .title = "40%",
        .value = NULL,
    },
    {
        .title = "60%",
        .value = NULL,
    },
    {
        .title = "80%",
        .value = NULL,
    },
    {
        .title = "100%",
        .value = NULL,
    },
};

static void select_brightness(int index) {
    ESP_LOGD(TAG, "Selected brightness index : %d", index);
    int brightness_percent[] = { 10, 25,40, 60,80, 100 };
    assert(index<(sizeof(brightness_percent)/sizeof(brightness_percent[0])));
    assert(index<(sizeof(brightness_items)/sizeof(brightness_items[0])));
    gpio_manager_backlight_set_brightness(brightness_percent[index]);
    lv_obj_t* value_lbl = list_items[2].value_label;
    if(value_lbl != NULL) {
        WITH_UI_LOCK() {
            lv_label_set_text(value_lbl, brightness_items[index].title);
        }
    }
}

static void open_brightness_selection_popup(void *data) {
    assert(parent_container);
    int item_count = sizeof(brightness_items) / sizeof(brightness_items[0]);
    create_popup_listview(parent_container, &brightness_popup_registry, brightness_items, 
                          item_count, select_brightness); 
}

void draw_settings_app_ui(lv_obj_t* parent) {
    assert(parent);
    const char* selected_wf = watchface_manager_get_selected_wf_name();
    if(selected_wf != NULL) {
        list_items[0].value = selected_wf;
    }
    
    const char* all_wf_names[MAX_WATCHFACES];
    wf_count = watchface_manager_get_all_wf_names(all_wf_names);
    if (wf_count == 0) {
        ESP_LOGE(TAG, "No watchfaces available");
        return;
    }

    wf_items = calloc(wf_count, sizeof(listview_t));
    if (wf_items == NULL) {
        ESP_LOGE(TAG, "Failed to allocate watchface items");
        return;
    }
    for(size_t i = 0; i<wf_count; i++) {
        wf_items[i].title = all_wf_names[i];
    }

    int backlight_percent = gpio_manager_backlight_get_brightness();

    WITH_UI_LOCK() {
        parent_container = parent;
        base_container = lv_obj_create(parent);
        remove_shadow_and_outline(base_container);
        lv_obj_set_style_pad_all(base_container, 5, LV_STATE_DEFAULT);
        lv_obj_set_size(base_container, lv_pct(100), lv_pct(100));
        lv_obj_set_layout(base_container, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(base_container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_bg_color(base_container, COLOR_THEME_SECONDARY, LV_STATE_DEFAULT);
        lv_obj_add_flag(base_container, LV_OBJ_FLAG_EVENT_BUBBLE);

        draw_horizoantal_divider(base_container);

        int item_count = sizeof(list_items)/sizeof(list_items[0]);
        for(int i=0 ; i<item_count; i++) {
            draw_listview_item(base_container, &list_items[i]);
            draw_horizoantal_divider(base_container);
        }
        if(list_items[2].value_label) {
            lv_label_set_text_fmt(list_items[2].value_label, "%d%%", backlight_percent);
        }
    }
}

void delete_settings_app_ui(void) {
    if(wf_items != NULL) {
        free(wf_items);
        wf_items = NULL;
        ESP_LOGD(TAG, "Freed watchface list items");
    }

    lv_obj_delete(base_container);
    base_container = NULL;
    parent_container = NULL;
    wf_count = 0;
}

static application_t settings_app = {
    .name = "Settings",
    .ico = &settings_ico,
    .draw_app = draw_settings_app_ui,
    .close_app = delete_settings_app_ui,
};

application_t* get_settings_app(void) {
    return &settings_app;
}
