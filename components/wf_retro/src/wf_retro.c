#include <lvgl.h>
#include <ui_types.h>
#include <global_locks.h>
#include <ui_utils.h>
#include "assets/bell.h"
#include "assets/calories.h"

static const char weekdays[7][4] = { "MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN" };
static const char months[][4] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                   "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };

lv_obj_t * container = NULL;
lv_obj_t * day_label = NULL;
lv_obj_t * time_label = NULL;
lv_obj_t * sec_label = NULL;
lv_obj_t * alarm_label = NULL;
lv_obj_t * alt_label = NULL;
lv_obj_t * temp_label = NULL;
lv_obj_t * steps_label = NULL;
lv_obj_t * month_lbl = NULL;
lv_obj_t * year_lbl = NULL;
static const int h_dash_height = 2;
static const int v_dash_height = 1;

static const lv_color_t mute_black = { .red = 0x60, .blue = 0x60, .green = 0x60 };
static const lv_color_t bg_cream = { .red = 0xf0, .blue = 0xe4, .green = 0xc5 };
static const lv_color_t black_color = { .red = 0x00, .blue = 0x00, .green = 0x00 };

static lv_obj_t* create_h_divider(lv_obj_t *parent) {
    lv_obj_t *div = lv_obj_create(parent);
    remove_shadow_and_outline(div);
    lv_obj_set_size(div, lv_pct(100), h_dash_height);
    lv_obj_set_style_bg_color(div, mute_black, LV_STATE_DEFAULT);
    lv_obj_set_style_margin_hor(div, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(div, 0, LV_STATE_DEFAULT);
    return div;
}

static lv_obj_t* create_v_divider(lv_obj_t * parent) {
    lv_obj_t *v_div = lv_obj_create(parent);
    remove_shadow_and_outline(v_div);
    lv_obj_set_size(v_div, v_dash_height, lv_pct(80));
    lv_obj_set_style_bg_color(v_div, mute_black, LV_STATE_DEFAULT);
    lv_obj_set_style_margin_hor(v_div, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(v_div, 0, LV_STATE_DEFAULT);
    return v_div;
}

void draw_retro_wf(lv_obj_t* parent) {
    WITH_UI_LOCK() {
        container = lv_obj_create(parent);
        lv_obj_set_style_bg_color(container, bg_cream, LV_STATE_DEFAULT);
        lv_obj_set_size(container, lv_pct(100), lv_pct(100));
        lv_obj_set_layout(container, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
        remove_shadow_and_outline(container);
        lv_obj_set_style_pad_hor(container, 8, LV_STATE_DEFAULT);
        lv_obj_set_style_pad_ver(container, 8, LV_STATE_DEFAULT);
        lv_obj_set_style_pad_top(container, 4, LV_STATE_DEFAULT);
        lv_obj_set_style_pad_row(container, 2, LV_STATE_DEFAULT);
        lv_obj_remove_flag(container, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* status_container = lv_obj_create(container);
        remove_shadow_and_outline(status_container);
        lv_obj_set_style_bg_opa(status_container, LV_OPA_TRANSP, LV_STATE_DEFAULT);
        lv_obj_set_size(status_container, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(status_container, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(status_container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);

        day_label = lv_label_create(status_container);
        lv_label_set_text(day_label, "WED");
        lv_obj_set_style_text_font(day_label, &lv_font_montserrat_10, LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(day_label, black_color, LV_STATE_DEFAULT);

        lv_obj_t *bat_label = lv_label_create(status_container);
        lv_label_set_text(bat_label, "[IIII]");
        lv_obj_set_style_text_font(bat_label, &lv_font_montserrat_10, LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(bat_label, black_color, LV_STATE_DEFAULT);

        create_h_divider(container);

        lv_obj_t* time_container = lv_obj_create(container);
        remove_shadow_and_outline(time_container);
        lv_obj_set_style_bg_opa(time_container, LV_OPA_TRANSP, LV_STATE_DEFAULT);
        lv_obj_set_size(time_container, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_style_pad_column(time_container, 8, LV_STATE_DEFAULT);

        lv_obj_t* fixed_container = lv_obj_create(time_container);
        remove_shadow_and_outline(fixed_container);
        lv_obj_set_style_bg_opa(fixed_container, LV_OPA_TRANSP, LV_STATE_DEFAULT);
        lv_obj_set_size(fixed_container, lv_pct(78), LV_SIZE_CONTENT);
        lv_obj_align(fixed_container, LV_ALIGN_LEFT_MID, 0, 0);

        time_label = lv_label_create(fixed_container);
        lv_label_set_text(time_label, "10:13");
        lv_obj_set_style_text_font(time_label, &lv_font_montserrat_30, LV_STATE_DEFAULT);
        lv_obj_align(time_label, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_set_style_text_color(time_label, black_color, LV_STATE_DEFAULT);

        sec_label = lv_label_create(time_container);
        lv_label_set_text(sec_label, "13");
        lv_obj_set_style_text_font(sec_label, &lv_font_montserrat_14, LV_STATE_DEFAULT);
        lv_obj_align(sec_label, LV_ALIGN_BOTTOM_RIGHT, -5, -4);
        lv_obj_set_style_text_color(sec_label, black_color, LV_STATE_DEFAULT);

        create_h_divider(container);

        lv_obj_t* middle_container = lv_obj_create(container);
        remove_shadow_and_outline(middle_container);
        lv_obj_set_style_bg_opa(middle_container, LV_OPA_TRANSP, LV_STATE_DEFAULT);
        lv_obj_set_size(middle_container, lv_pct(100), 36);
        lv_obj_set_flex_flow(middle_container, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(middle_container, LV_FLEX_ALIGN_SPACE_AROUND,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(middle_container, 0, LV_STATE_DEFAULT);
        lv_obj_remove_flag(middle_container, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* alarm_box = lv_obj_create(middle_container);
        remove_shadow_and_outline(alarm_box);
        lv_obj_set_style_bg_opa(alarm_box, LV_OPA_TRANSP, LV_STATE_DEFAULT);
        lv_obj_set_size(alarm_box, lv_pct(28), lv_pct(100));
        lv_obj_set_style_pad_column(alarm_box, 4, LV_STATE_DEFAULT);
        lv_obj_remove_flag(alarm_box, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* bell_img = lv_image_create(alarm_box);
        remove_shadow_and_outline(bell_img);
        lv_image_set_src(bell_img, &bell);
        lv_obj_align(bell_img, LV_ALIGN_CENTER, 0, 0);

        alarm_label = lv_label_create(alarm_box);
        lv_label_set_text(alarm_label, "3");
        lv_obj_set_style_text_font(alarm_label, &lv_font_montserrat_10, LV_STATE_DEFAULT);
        lv_obj_align(alarm_label, LV_ALIGN_TOP_RIGHT, -4, 4);
        lv_obj_set_style_text_color(alarm_label, black_color, LV_STATE_DEFAULT);

        create_v_divider(middle_container);

        lv_obj_t* date_box = lv_obj_create(middle_container);
        remove_shadow_and_outline(date_box);
        lv_obj_set_style_bg_opa(date_box, LV_OPA_TRANSP, LV_STATE_DEFAULT);
        lv_obj_set_size(date_box, lv_pct(40), lv_pct(100));
        lv_obj_set_flex_flow(date_box, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(date_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(date_box, 0, LV_STATE_DEFAULT);

        month_lbl = lv_label_create(date_box);
        lv_label_set_text(month_lbl, "DEC");
        lv_obj_set_style_text_font(month_lbl, &lv_font_montserrat_14, LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(month_lbl, black_color, LV_STATE_DEFAULT);

        year_lbl = lv_label_create(date_box);
        lv_label_set_text(year_lbl, "26");
        lv_obj_set_style_text_font(year_lbl, &lv_font_montserrat_14, LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(year_lbl, black_color, LV_STATE_DEFAULT);

        create_v_divider(middle_container);

        lv_obj_t* flame_box = lv_obj_create(middle_container);
        remove_shadow_and_outline(flame_box);
        lv_obj_set_style_bg_opa(flame_box, LV_OPA_TRANSP, LV_STATE_DEFAULT);
        lv_obj_set_size(flame_box, lv_pct(32), lv_pct(100));
        lv_obj_remove_flag(flame_box, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* calories_img = lv_image_create(flame_box);
        remove_shadow_and_outline(calories_img);
        lv_image_set_src(calories_img, &calories);
        lv_obj_align(calories_img, LV_ALIGN_CENTER, 0, 0);

        create_h_divider(container);

        lv_obj_t* env_container = lv_obj_create(container);
        remove_shadow_and_outline(env_container);
        lv_obj_set_style_bg_opa(env_container, LV_OPA_TRANSP, LV_STATE_DEFAULT);
        lv_obj_set_size(env_container, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(env_container, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(env_container, LV_FLEX_ALIGN_SPACE_AROUND,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(env_container, 4, LV_STATE_DEFAULT);

        alt_label = lv_label_create(env_container);
        lv_label_set_text(alt_label, "A:61m");
        lv_obj_set_style_text_font(alt_label, &lv_font_montserrat_14, LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(alt_label, black_color, LV_STATE_DEFAULT);

        create_v_divider(env_container);

        temp_label = lv_label_create(env_container);
        lv_label_set_text(temp_label, "T:22°C");
        lv_obj_set_style_text_font(temp_label, &lv_font_montserrat_14, LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(temp_label, black_color, LV_STATE_DEFAULT);

        create_h_divider(container);

        lv_obj_t* steps_container = lv_obj_create(container);
        remove_shadow_and_outline(steps_container);
        lv_obj_set_style_bg_opa(steps_container, LV_OPA_TRANSP, LV_STATE_DEFAULT);
        lv_obj_set_size(steps_container, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_style_pad_all(steps_container, 4, LV_STATE_DEFAULT);

        steps_label = lv_label_create(steps_container);
        lv_label_set_text(steps_label, "STEPS:648");
        lv_obj_set_style_text_font(steps_label, &lv_font_montserrat_14, LV_STATE_DEFAULT);
        lv_obj_align(steps_label, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_set_style_text_color(steps_label, black_color, LV_STATE_DEFAULT);
    }
}

void update_retro_wf(wf_update_payload_t* update_data) {
    if(!update_data) return;
    WITH_UI_LOCK() {
            date_time_t time = update_data->time;
            if(time_label) lv_label_set_text_fmt(time_label, "%02d:%02d", time.hr, time.min);
            if(sec_label) lv_label_set_text_fmt(sec_label, "%02d", time.sec);
            if(month_lbl) lv_label_set_text(month_lbl, months[time.month]);
            if(year_lbl) lv_label_set_text_fmt(year_lbl, "%02d", time.year%100);
            if(day_label) lv_label_set_text(day_label, weekdays[time.d_week]);
    }
     
}

void delete_retro_wf(void) {
    WITH_UI_LOCK() {
        lv_obj_delete(container);
        container = NULL;
        day_label = NULL;
        time_label = NULL;
        sec_label = NULL;
        alarm_label = NULL;
        alt_label = NULL;
        temp_label = NULL;
        steps_label = NULL;
        month_lbl = NULL;
        year_lbl = NULL;
    }
}


static watchface_t retro_wf = {
    .draw_watchface = draw_retro_wf,
    .update_watchface = update_retro_wf,
    .del_watchface = delete_retro_wf
};

watchface_t* get_retro_wf(void) {
    return &retro_wf;
}
