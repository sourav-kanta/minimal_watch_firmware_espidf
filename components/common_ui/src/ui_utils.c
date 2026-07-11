#include <lvgl.h>
#include <esp_log.h>
#include <ui_utils.h>

static const char* TAG = "LVGL helper";

void remove_shadow_and_outline(lv_obj_t* obj) {
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(obj, 0,LV_PART_MAIN);
    lv_obj_set_style_pad_row(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, 0, 0);
}

void make_obj_navigable(lv_obj_t* obj) {
    if(lv_group_get_default() == NULL) {
        ESP_LOGE(TAG, "Custom group not added");
        return;
    }
    lv_group_add_obj(lv_group_get_default(), obj);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE);
}

