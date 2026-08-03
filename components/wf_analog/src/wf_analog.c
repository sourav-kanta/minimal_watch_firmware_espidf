#include <lvgl.h>
#include <wf_analog.h>
#include <ui_types.h>
#include <global_locks.h>
#include <ui_utils.h>
#include "assets/wf2_bg.h"
#include "assets/watch_hr.h"
#include "assets/watch_min.h"
#include "assets/watch_sec.h"

#define DATE_OFFSET_Y (-50)
#define MONTH_OFFSET_Y (-40)
#define INFO_OFFSET_Y (40)

static lv_obj_t *hr_hand = NULL, *min_hand = NULL, *sec_hand = NULL, *date_lbl = NULL,
                *day_lbl = NULL, *month_lbl = NULL, *base_obj = NULL, *weather_lbl = NULL,
                *battery_lbl = NULL;
static const char weekdays[7][4] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
static const char months[][10] = { "January", "February", "March", "April", "May", "June",
                                   "July", "August", "September", "October", "November", 
                                   "December" };
static const lv_color_t text_color = {
    .blue = 0x0b,
    .red = 0xb8,
    .green = 0x86
};

void draw_wf(lv_obj_t* parent) {
    WITH_UI_LOCK() {
        base_obj = lv_obj_create(parent);
        remove_shadow_and_outline(base_obj);
        lv_obj_clear_flag(base_obj, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(base_obj, lv_pct(100), lv_pct(100));
        
        lv_obj_t* bg_img = lv_image_create(base_obj);
        lv_image_set_src(bg_img, &wf2_bg);
        lv_obj_remove_flag(bg_img, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(bg_img, LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_center(bg_img);

        lv_obj_t* info_container = lv_obj_create(base_obj);
        remove_shadow_and_outline(info_container);
        lv_obj_set_style_layout(info_container, LV_LAYOUT_FLEX, LV_STATE_DEFAULT);
        lv_obj_set_flex_flow(info_container, LV_FLEX_FLOW_ROW);
        lv_obj_align(info_container, LV_ALIGN_TOP_MID, 0, INFO_OFFSET_Y);
        lv_obj_set_style_pad_column(info_container, 25, LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(info_container, LV_OPA_TRANSP, LV_STATE_DEFAULT);

        weather_lbl = lv_label_create(info_container);
        lv_obj_set_style_text_font(weather_lbl, &lv_font_montserrat_10, LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(weather_lbl, text_color, LV_STATE_DEFAULT);
        lv_label_set_text(weather_lbl, "24C");

        battery_lbl = lv_label_create(info_container);
        lv_obj_set_style_text_font(battery_lbl, &lv_font_montserrat_10, LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(battery_lbl, text_color, LV_STATE_DEFAULT);
        lv_label_set_text(battery_lbl, "45%");

        lv_obj_set_size(info_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        
        lv_obj_t* date_container = lv_obj_create(base_obj);
        remove_shadow_and_outline(date_container);
        lv_obj_set_style_layout(date_container, LV_LAYOUT_FLEX, LV_STATE_DEFAULT);
        lv_obj_set_flex_flow(date_container, LV_FLEX_FLOW_ROW);
        lv_obj_align(date_container, LV_ALIGN_BOTTOM_MID, 0, DATE_OFFSET_Y);
        lv_obj_set_style_pad_column(date_container, 5, LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(date_container, LV_OPA_TRANSP, LV_STATE_DEFAULT);

        day_lbl = lv_label_create(date_container);
        lv_obj_set_style_text_font(day_lbl, &lv_font_montserrat_10, LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(day_lbl, text_color, LV_STATE_DEFAULT);
        lv_label_set_text(day_lbl, "Wed");

        date_lbl = lv_label_create(date_container);
        lv_obj_set_style_text_font(date_lbl, &lv_font_montserrat_10, LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(date_lbl, text_color, LV_STATE_DEFAULT);
        lv_label_set_text(date_lbl, "07");

        month_lbl = lv_label_create(base_obj);
        lv_obj_align(month_lbl, LV_ALIGN_BOTTOM_MID, 0, MONTH_OFFSET_Y);
        lv_obj_set_style_text_font(month_lbl, &lv_font_montserrat_10, LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(month_lbl, text_color, LV_STATE_DEFAULT);
        lv_label_set_text(month_lbl, "February");

        lv_obj_set_size(date_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        
        lv_obj_move_foreground(date_lbl);
        hr_hand = lv_image_create(base_obj);
        lv_image_set_src(hr_hand, &watch_hr);
        lv_image_set_pivot(hr_hand, hr_hand_info.pivot_x, hr_hand_info.pivot_y);
        lv_obj_set_pos(hr_hand, hr_hand_info.pos_x, hr_hand_info.pos_y);
        lv_obj_move_foreground(hr_hand);

        min_hand = lv_image_create(base_obj);
        lv_image_set_src(min_hand, &watch_min);
        lv_image_set_pivot(min_hand, min_hand_info.pivot_x, min_hand_info.pivot_y);
        lv_obj_set_pos(min_hand, min_hand_info.pos_x, min_hand_info.pos_y);
        lv_obj_move_foreground(min_hand);

        sec_hand = lv_image_create(base_obj);
        lv_image_set_src(sec_hand, &watch_sec);
        lv_image_set_pivot(sec_hand, sec_hand_info.pivot_x, sec_hand_info.pivot_y);
        lv_obj_set_pos(sec_hand, sec_hand_info.pos_x, sec_hand_info.pos_y);
        lv_obj_move_foreground(sec_hand);
    }
        
}

void update_wf(wf_update_payload_t* update_data) {
    if(!update_data) return;
    int hr_angle = ((update_data->time.hr % 12) *3600 / 12) + 
                   (update_data->time.min*10 / 2) +
                   (update_data->time.sec*10 / 120);
    int min_angle = (update_data->time.min * 60) +
                    (update_data->time.sec);
    int sec_angle = (update_data->time.sec*6*10);
    int day_idx = update_data->time.d_week >= 7 ? 0 : update_data->time.d_week;
    int month_idx = (update_data->time.month == 0) || (update_data->time.month > 12) ?
                    0 : update_data->time.month - 1;
    int temp_whole = update_data->weather.temperature / 10;
    int temp_frac = update_data->weather.temperature % 10;
    WITH_UI_LOCK() {
        if(hr_hand) lv_image_set_rotation(hr_hand, hr_angle);
        if(min_hand) lv_image_set_rotation(min_hand, min_angle);
        if(sec_hand) lv_image_set_rotation(sec_hand, sec_angle);
        if(day_lbl) lv_label_set_text(day_lbl, weekdays[day_idx]);
        if(date_lbl) lv_label_set_text_fmt(date_lbl, "%02u", update_data->time.day);
        if(month_lbl) lv_label_set_text(month_lbl, months[month_idx]);
        if(weather_lbl) lv_label_set_text_fmt(weather_lbl, "%d.%1d\xC2\xB0" "C", temp_whole, temp_frac); 
    }
     
}

void delete_wf(void) {
    WITH_UI_LOCK() {
        lv_obj_delete(base_obj);
    }
    hr_hand = NULL;
    min_hand = NULL;
    sec_hand = NULL;
    date_lbl = NULL;
    day_lbl = NULL;
    month_lbl = NULL;
    base_obj = NULL;
    weather_lbl = NULL;
    battery_lbl = NULL;
}


static watchface_t analog_wf = {
    .name = "Analog",
    .draw_watchface = draw_wf,
    .update_watchface = update_wf,
    .del_watchface = delete_wf
};

watchface_t* get_analog_wf(void) {
    return &analog_wf;
}
