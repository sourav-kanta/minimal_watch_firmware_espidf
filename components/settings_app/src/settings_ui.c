#include <settings_app.h>
#include <ui_theme.h>
#include <ui_utils.h>
#include <common_types.h>
#include <app_types.h>
#include <esp_log.h>
#include <app_utils.h>
#include <lvgl.h>

#include "assets/settings_ico.h"

static lv_obj_t* base_container = NULL;

void draw_settings_app_ui(lv_obj_t* parent) {
    base_container = lv_obj_create(parent);
    remove_shadow_and_outline(base_container);
}

void delete_settings_app_ui(void) {
    lv_obj_delete(base_container);
    base_container = NULL;
}

static application_t settings_app = {
    .name = "Weather",
    .ico = &settings_ico,
    .draw_app = draw_settings_app_ui,
    .close_app = delete_settings_app_ui,
};

application_t* get_settings_app(void) {
    return &settings_app;
}
