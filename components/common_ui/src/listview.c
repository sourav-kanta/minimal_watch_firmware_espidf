#include <lvgl.h>
#include <ui_utils.h>
#include <ui_theme.h>
#include <esp_log.h>

static const char* TAG = "Listview";

static void onclick_label_cb(lv_event_t* event) {
    if(lv_event_get_key(event) == LV_KEY_ENTER) {
        listview_t* item = lv_event_get_user_data(event);
        assert(item);
        if(item->title_click_cb) {
            item->title_click_cb(item->data);
        }
        lv_event_stop_bubbling(event);
    }
}

void draw_listview_item(lv_obj_t* base_obj, listview_t* item) {
    assert(base_obj);
    assert(item);
    assert(item->title);
    lv_obj_t* card = lv_obj_create(base_obj);
    lv_obj_set_style_bg_opa(card, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    remove_shadow_and_outline(card);
    lv_obj_set_size(card, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(card, 2, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_ver(card, 5, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_STATE_FOCUSED);
    lv_obj_set_style_bg_color(card, COLOR_THEME_FOCUS_BG, LV_STATE_FOCUSED);
    lv_obj_add_flag(card, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_t* title = lv_label_create(card);
    make_obj_navigable(title);
    lv_obj_add_event_cb(title, onclick_label_cb, LV_EVENT_KEY, (void*)item);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_10, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(title, COLOR_THEME_TEXT_PRIMARY, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(title, COLOR_THEME_TEXT_HIGLIGHT, LV_STATE_FOCUSED);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);
    lv_label_set_text(title, item->title);
    item->value_label = NULL;
    if(item->value) {
        lv_obj_t* value = lv_label_create(card);
        item->value_label = value;
        lv_obj_set_style_text_font(value, &lv_font_montserrat_10, LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(value, COLOR_THEME_TEXT_PRIMARY, LV_STATE_DEFAULT);
        lv_obj_align(value, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_label_set_text(value, item->value);
    }
}
