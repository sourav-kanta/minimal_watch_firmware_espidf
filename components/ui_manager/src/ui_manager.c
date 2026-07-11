#include <ui_manager.h>
#include <lvgl_bridge.h>
#include <ui_base.h>
#include <wf_manager.h>

static bool initialized = false;

void ui_on(void) {
    init_lvgl();
    draw_base_screen();
}

void ui_off(void) {
    clean_base_screen();
    deinit_lvgl();
}

void ui_sleep(void) {
    suspend_base_screen();
    suspend_lvgl();
}

void ui_resume(void) {
    resume_lvgl();
    resume_base_screen();
}

void ui_manager_init(void) {
    if(initialized)
        return;
    watchface_manager_init();
    initialized = true;
}

void ui_manager_deinit(void) {
    ui_off();
    watchface_manager_deinit();
    initialized = false;    
}

