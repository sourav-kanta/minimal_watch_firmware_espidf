#include <stopwatch_app.h>
#include <ui_theme.h>
#include <ui_utils.h>
#include <common_types.h>
#include <app_types.h>
#include <esp_log.h>
#include <lvgl.h>
#include <global_locks.h>

#include "assets/stopwatch_ico.h"

static lv_obj_t* base_container = NULL;
static const char* TAG = "Stopwatch app";

void draw_stopwatch_app_ui(lv_obj_t* parent) {
    assert(parent);
}

void delete_stopwatch_app_ui(void) {

}

static application_t stopwatch_app = {
    .name = "Stopwatch",
    .app_perms = APP_PERM_BLE,
    .ico = &stopwatch_ico,
    .draw_app = draw_stopwatch_app_ui,
    .close_app = delete_stopwatch_app_ui,
};

application_t* get_stopwatch_app(void) {
    return &stopwatch_app;
}
