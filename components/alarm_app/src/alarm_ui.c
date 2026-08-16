#include <alarm_app.h>
#include <ui_theme.h>
#include <ui_utils.h>
#include <common_types.h>
#include <app_types.h>
#include <esp_log.h>
#include <lvgl.h>
#include <global_locks.h>

#include "assets/alarm_ico.h"

static lv_obj_t* base_container = NULL;
static const char* TAG = "Alarm app";

void draw_alarm_app_ui(lv_obj_t* parent) {
    assert(parent);
}

void delete_alarm_app_ui(void) {

}

static application_t alarm_app = {
    .name = "Alarm",
    .app_perms = APP_PERM_BLE,
    .ico = &alarm_ico,
    .draw_app = draw_alarm_app_ui,
    .close_app = delete_alarm_app_ui,
};

application_t* get_alarm_app(void) {
    return &alarm_app;
}
