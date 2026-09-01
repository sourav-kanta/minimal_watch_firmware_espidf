#include <lvgl_bridge.h>

#include <common_types.h>
#include <common_consts.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_attr.h>
#include <display_driver.h>
#include <encoder_driver.h>
#include <lvgl_handler_thread.h>
#include <lvgl_input_translator.h>
#include <wakelock_manager.h>

static const char* TAG = "LVGL_SETUP";

static lv_display_t* disp = NULL;
static DRAM_ATTR uint8_t disp_buf1[DISPLAY_LVGL_BUFFER_SIZE] __attribute__((aligned(32)));
static DRAM_ATTR uint8_t disp_buf2[DISPLAY_LVGL_BUFFER_SIZE] __attribute__((aligned(32)));
static QueueHandle_t xinputQueue = NULL;
static const int MAX_INPUTS = 10;

static void lvgl_display_flush_cb(lv_display_t * display, const lv_area_t * area, uint8_t * px_map) {
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    display_draw_bitmap(offsetx1, offsety1, offsetx2+1, offsety2+1, px_map);
}

static void flush_done_cb(void) {
    lv_display_flush_ready(disp);
}

void init_lvgl(void) {
    lv_init();
    init_display(flush_done_cb);
    disp = lv_display_create(DISPLAY_LCD_H_RES, DISPLAY_LCD_V_RES);
    if (!disp) {
        ESP_LOGE(TAG, "Failed creating LVGL display, panic!");
        esp_system_abort("LVGL display allocation failed");
    }
    lv_display_set_buffers(disp, disp_buf1, disp_buf2, 
                            DISPLAY_LVGL_BUFFER_SIZE, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, lvgl_display_flush_cb);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565_SWAPPED);
    lv_display_set_offset(disp, 2, 1);
    ESP_LOGI(TAG, "LVGL initialized with DMA buffers");

    xinputQueue = xQueueCreate(MAX_INPUTS, sizeof(encoder_input_t));
    if(xinputQueue == NULL) {
        esp_system_abort("Failed creating encoder queue");
    }
    encoder_init(xinputQueue);
    setup_keyboard(xinputQueue);
    wakelock_manager_init();
    display_on();
    if(lvgl_thread_exists()) {
        ESP_LOGE(TAG, "Critical : LVGL is already running");
    }
    else {
        start_lvgl_thread();
    }
}

void deinit_lvgl(void) {
    stop_lvgl_thread();
    if(disp) {
        lv_display_delete(disp);
        disp = NULL;
    }
    assert(!wakelock_manager_is_wake_locked());
    wakelock_manager_deinit();
    delete_keyboard();
    encoder_deinit();
    lv_deinit();
    if(xinputQueue) {
        vQueueDelete(xinputQueue);
        xinputQueue = NULL;
    }
    display_off();
    deinit_display();
    ESP_LOGI(TAG, "LVGL deinitialized.");
}

void suspend_lvgl(void) {
    assert(!wakelock_manager_is_wake_locked());
    if(!lvgl_thread_exists()) {
        ESP_LOGE(TAG, "Critical : LVGL is not currently running");
    }
    stop_lvgl_thread();
    encoder_deinit();
    suspend_keyboard();
    display_sleep();
}

void resume_lvgl(void) {
    display_on();
    resume_keyboard();
    encoder_init(xinputQueue);
    if(lvgl_thread_exists()) {
        ESP_LOGE(TAG, "Critical : LVGL is already running");
    }
    else {
        start_lvgl_thread();
    }
}

bool lvgl_bridge_update_inactivity_timeout(uint32_t to) {
    if(to < DISPLAY_MIN_USER_INPUT_TIMEOUT) {
        ESP_LOGE(TAG, "Invalid inactivity timeout");
        return false;
    }
    return update_input_timeout(to);
}

uint32_t lvgl_bridge_get_inactivity_timeout(void) {
    return get_input_timeout(); 
}

lv_group_t* get_keypad_group() {
    return get_key_group();
}
