#include <event_screens.h>
#include <lvgl.h> 
#include <ui_theme.h>
#include <global_locks.h>
#include <ui_utils.h>
#include <ui_base.h>
#include <wakelock_manager.h>

static const lv_color_t COLOR_GOLD = { .red = 0xD4, .green = 0xAF, .blue = 0x37 };
static lv_obj_t* background = NULL;
static const uint32_t alarm_screen_timeout_sec = 30;
static lv_timer_t* expiry_timer = NULL;

static void alarm_screen_exit_cleanup(void) {
    if(!background) return; 
    WITH_UI_LOCK() {
        lv_obj_delete_async(background);
        background = NULL;
        if(expiry_timer) lv_timer_delete(expiry_timer);
        wakelock_manager_release_wakelock();
    }
    ui_base_resume_base_screen();
    expiry_timer = NULL;
}

static void timer_expiry_cb(lv_timer_t* timer) {
    alarm_screen_exit_cleanup();
}

static void background_key_event_cb(lv_event_t* event) {
    // Discard ESC, LEFT, RIGHT key and make the background captive
    if(lv_event_get_target(event) == lv_event_get_current_target_obj(event)) {
        if(lv_event_get_key(event) != LV_KEY_ENTER) {
            // Reject all except enter
            lv_event_stop_bubbling(event);
        } 
    }
    else {
        // consume left and right for children
        if(lv_event_get_key(event) == LV_KEY_LEFT || lv_event_get_key(event) == LV_KEY_RIGHT) {
            lv_event_stop_bubbling(event);
        }
    }
}

static void dismiss_btn_click_cb(lv_event_t* event) {
    if(lv_event_get_key(event) == LV_KEY_ENTER) {
        if(expiry_timer) {
            alarm_screen_exit_cleanup();
            lv_event_stop_bubbling(event);
        } 
    }
}

void show_alarm_page(lv_obj_t* parent, date_time_t* dt) {
    if(background) { 
        // Alarm screen already showing
        return;
    }

    WITH_UI_LOCK() {
        wakelock_manager_acquire_wakelock();
        background = lv_obj_create(parent);
        lv_obj_set_size(background, lv_pct(100), lv_pct(100));
        lv_obj_set_style_pad_all(background, 5, LV_STATE_DEFAULT);
        make_obj_navigable(background);
        lv_group_focus_obj(background);
        lv_obj_move_foreground(background);
        lv_obj_add_event_cb(background, background_key_event_cb, LV_EVENT_KEY, NULL);

        lv_obj_t* time_lbl = lv_label_create(background);
        lv_obj_set_style_text_font(time_lbl, &lv_font_montserrat_24, LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(time_lbl, COLOR_THEME_PRIMARY, LV_STATE_DEFAULT);
        lv_obj_align(time_lbl, LV_ALIGN_CENTER, 0, 15);
        lv_label_set_text_fmt(time_lbl, "%02u:%02u", dt->hr, dt->min); 

        lv_obj_t* dismiss_btn = lv_button_create(background);
        make_obj_navigable(dismiss_btn);
        lv_obj_set_size(dismiss_btn, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_align(dismiss_btn, LV_ALIGN_BOTTOM_MID, 0, -5);
        lv_obj_set_style_margin_hor(dismiss_btn, 15, LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(dismiss_btn, 5, LV_STATE_DEFAULT);
        lv_obj_set_style_radius(dismiss_btn, 5, LV_STATE_DEFAULT);
        lv_obj_t* btn_lbl = lv_label_create(dismiss_btn);
        lv_obj_align(btn_lbl, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_10, LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(dismiss_btn, COLOR_GOLD, LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(dismiss_btn, COLOR_THEME_TEXT_HIGLIGHT, LV_STATE_FOCUSED);
        lv_obj_set_style_text_color(btn_lbl, COLOR_THEME_PRIMARY, LV_STATE_DEFAULT);
        lv_label_set_text(btn_lbl, "Dismiss");
        lv_obj_add_event_cb(dismiss_btn, dismiss_btn_click_cb, LV_EVENT_KEY, NULL);

        expiry_timer = lv_timer_create(timer_expiry_cb, alarm_screen_timeout_sec*1000, NULL);
        assert(expiry_timer);
    }
}

void show_incoming_call_page(lv_obj_t* parent, const char* number, const char* name) {
    WITH_UI_LOCK() {
    
    }
}

void show_incoming_notification_toast(lv_obj_t* parent, const char* title) {
    WITH_UI_LOCK() {
    
    }
}

