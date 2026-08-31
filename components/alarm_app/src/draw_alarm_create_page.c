#include <draw_alarm_create_page.h>
#include <lvgl.h>
#include <global_locks.h>
#include <ui_theme.h>
#include <common_types.h>
#include <ui_utils.h>
#include <day_time_picker_widget.h>
#include <esp_log.h>
#include <common_apis.h>
#include <alarm_app.h>

static const char* TAG = "Create Alarm";
static lv_obj_t* background = NULL;
static lv_obj_t* err_lbl = NULL;
static lv_obj_t* date_labels[8];
static lv_obj_t* time_labels[4];

static void close_alarm_creator(void) {
    assert(background);
    WITH_UI_LOCK() {
        lv_obj_delete(background);
    }
    background = NULL;
    err_lbl = NULL;
    memset(date_labels, 0, sizeof(date_labels));
    memset(time_labels, 0, sizeof(time_labels));
}

static void on_key_press_cb(lv_event_t* event) {
    if(lv_event_get_target_obj(event) == lv_event_get_current_target_obj(event)) {
        // Ignore left and right keys
        if(lv_event_get_key(event) == LV_KEY_LEFT || lv_event_get_key(event) == LV_KEY_RIGHT) {
            lv_event_stop_bubbling(event);
        }

        // close page on esc
        if(lv_event_get_key(event) == LV_KEY_ESC) {
            close_alarm_creator();
            WITH_UI_LOCK() {
                lv_event_stop_bubbling(event);
            }
        }
    }
}

static void on_btn_clicked_cb(lv_event_t* event) {
    if(lv_event_get_key(event) == LV_KEY_ENTER) {
        char time_str[6];
        day_time_picker_widget_get_selected_value("**:**", time_labels, time_str);
        time_str[5] = '\0';
        char date_str[11];
        day_time_picker_widget_get_selected_value("**/**/****", date_labels, date_str);
        date_str[10] = '\0';
        ESP_LOGI(TAG, "Selected time : %s", time_str);
        ESP_LOGI(TAG, "Selected date : %s", date_str);
        // sscanf reads into 4 bytes
        unsigned int hr, min, day, mth, year;
        int parsed_time = sscanf(time_str, "%2u:%2u", &hr, &min);
        int parsed_date = sscanf(date_str, "%2u/%2u/%4u", &day, &mth, &year);
        assert(parsed_time == 2 && parsed_date == 3);

        date_time_t alarm_date = {
            .hr = hr,
            .min = min,
            .sec = 0,
            .day = day,
            .month = mth,
            .year = year
        };
        if(validate_date_time(&alarm_date)) {
            alarm_t new_alarm = {
                .epoch = get_epoch_from_date_time(&alarm_date),
                .sync_with_phone = false,   // TODO, add a checkbox
                .type = ALARM_TYPE_WATCH    // TODO, depends on sync phone
            };
            if(create_new_alarm(get_alarm_app(), &new_alarm)) {
                // Close alarm creator
                ESP_LOGI(TAG, "Successfully created alarm");
                close_alarm_creator();
                invalidate_and_repopulate_alarm_list();
            }
            else {
                assert(err_lbl);
                WITH_UI_LOCK() {
                    lv_obj_remove_flag(err_lbl, LV_OBJ_FLAG_HIDDEN);
                }
            }
        }
        else {
            // Unhide error message
            assert(err_lbl);
            WITH_UI_LOCK() {
                lv_obj_remove_flag(err_lbl, LV_OBJ_FLAG_HIDDEN);
            }
        }
        lv_event_stop_bubbling(event);
    }
}

void draw_modify_alarm_page(lv_obj_t* parent, alarm_t *alarm) {
    assert(parent);
    picker_params_t time_params = { 
        .font = &lv_font_montserrat_30,
    };
    picker_params_t date_params = { 
        .font = &lv_font_montserrat_14,
    };
    char time_pattern[6];
    char date_pattern[11];
    date_time_t dt;
    if(alarm == NULL) {
        // Create new alarm
        get_date_time(&dt);
    }
    else {
        // Modify existing alarm
        get_date_time_from_epoch(alarm->epoch, &dt);
    }
    snprintf(time_pattern, sizeof(time_pattern), "%02u:%02u", (dt.hr%24), (dt.min%60));
    snprintf(date_pattern,  sizeof(date_pattern), "%02u/%02u/%04u", 
             (dt.day%100), (dt.month%100), (dt.year%10000)); 
    
    WITH_UI_LOCK() {
        assert(background == NULL);
        background = lv_obj_create(parent);
        remove_shadow_and_outline(background);
        lv_obj_set_size(background, lv_pct(100), lv_pct(100));
        lv_obj_move_foreground(background);
        make_obj_navigable(background);
        lv_obj_set_style_pad_all(background, 5, LV_STATE_DEFAULT);
        lv_group_focus_obj(background);
        lv_obj_set_style_bg_color(background, COLOR_THEME_SECONDARY, LV_STATE_DEFAULT);
        lv_obj_add_event_cb(background, on_key_press_cb, LV_EVENT_KEY, NULL);

        time_params.parent = background;
        time_params.pattern = time_pattern;
        lv_obj_t* time_widget = day_time_picker_widget_draw(&time_params, time_labels);            
        lv_obj_set_align(time_widget, LV_ALIGN_TOP_MID);
        
        date_params.parent = background;
        date_params.pattern = date_pattern;
        lv_obj_t* date_widget = day_time_picker_widget_draw(&date_params, date_labels);            
        lv_obj_align_to(date_widget, time_widget, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);

        lv_obj_t* btn = lv_button_create(background);
        lv_obj_set_style_pad_all(btn, 5, LV_STATE_DEFAULT);
        lv_obj_set_style_pad_hor(btn, 20, LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(btn, COLOR_THEME_TERTIARY, LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(btn, COLOR_THEME_TEXT_HIGLIGHT, LV_STATE_FOCUSED);
        lv_obj_set_style_radius(btn, 5, LV_STATE_DEFAULT);
        make_obj_navigable(btn);
        lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_add_event_cb(btn, on_btn_clicked_cb, LV_EVENT_KEY, NULL);

        lv_obj_t* btn_lbl = lv_label_create(btn);
        lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_10, LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(btn_lbl, COLOR_THEME_TEXT_PRIMARY, LV_STATE_DEFAULT);
        lv_label_set_text(btn_lbl, "Create Alarm");

        err_lbl = lv_label_create(background);
        lv_obj_set_style_text_font(err_lbl, &lv_font_montserrat_10, LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(err_lbl, COLOR_THEME_TEXT_ERROR, LV_STATE_DEFAULT);
        lv_label_set_text(err_lbl, "Invalid date or time");
        lv_obj_align_to(err_lbl, btn, LV_ALIGN_OUT_TOP_MID, 0, -20);
        lv_obj_add_flag(err_lbl, LV_OBJ_FLAG_HIDDEN);
    }
}
