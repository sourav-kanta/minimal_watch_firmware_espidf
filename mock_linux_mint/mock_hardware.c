#include <stdbool.h>
#include "esp_err.h"
#include "esp_log.h"
#include <driver/ledc.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_adc/adc_oneshot.h>
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_qemu_rgb.h"

esp_lcd_panel_handle_t qemu_mock_panel_handle = NULL;
void (*callback) ();
extern void init_fsm(void);
static  uint32_t argb_buffer[128*160];

void __wrap_init_display(void (*cb)(void)) {
    ESP_LOGI("QEMU_MOCK", "Intercepting high-level init_display() function...");
    esp_lcd_rgb_qemu_config_t qemu_screen_config = {
        .width = 128,  
        .height = 160  
    };

    esp_err_t ret = esp_lcd_new_rgb_qemu(&qemu_screen_config, &qemu_mock_panel_handle);
    
    if (ret == ESP_OK && qemu_mock_panel_handle) {
        printf("[QEMU DISPLAY] Virtual display matrix generated successfully!\n");
        esp_lcd_panel_reset(qemu_mock_panel_handle);
        esp_lcd_panel_init(qemu_mock_panel_handle);
    } else {
        ESP_LOGE("QEMU_MOCK", "Failed to allocate virtual framebuffer memory.");
        return;
    }
    callback = cb;
    init_fsm();
}

void __wrap_display_draw_bitmap(int x1, int y1, int x2, int y2, const void* pixels) {
    if (!qemu_mock_panel_handle)
        return;
    
    const uint16_t *src = pixels;

    int width = x2 - x1;
    int height = y2 - y1;

    for(int y = 0; y < height; y++) {
        for(int x = 0; x < width; x++) {
            uint16_t swapped = src[y * width + x];
    
            // LVGL RGB565_SWAPPED -> normal RGB565
            uint16_t rgb565 =
                (swapped >> 8) |
                (swapped << 8);
    
            uint8_t r = ((rgb565 >> 11) & 0x1F) << 3;
            uint8_t g = ((rgb565 >> 5) & 0x3F) << 2;
            uint8_t b = (rgb565 & 0x1F) << 3;
    
            argb_buffer[y * width + x] =
                0xFF000000 |
                (r << 16) |
                (g << 8) |
                b;
        }
    }

    esp_lcd_panel_draw_bitmap(qemu_mock_panel_handle, x1-2, y1-1, x2-2, y2-1, argb_buffer);
    callback();
}

bool __wrap_display_on(void) {
    printf("[QEMU DISPLAY] Screen Power -> ON\n");
    if (qemu_mock_panel_handle) {
        esp_lcd_panel_disp_on_off(qemu_mock_panel_handle, true);
    }
    return true; 
}

bool __wrap_display_off(void) {
    printf("[QEMU DISPLAY] Screen Power -> OFF\n");
    if (qemu_mock_panel_handle) {
        esp_lcd_panel_disp_on_off(qemu_mock_panel_handle, false);
    }
    return true;
}

void __wrap_deinit_display(void) {
    printf("[QEMU MOCK] Intercepting deinit_display() function...\n");

    __wrap_display_off();
    if (qemu_mock_panel_handle) {
        esp_lcd_panel_del(qemu_mock_panel_handle);
        qemu_mock_panel_handle = NULL;
    }

    callback = NULL;
    ESP_LOGI("QEMU_MOCK", "Virtual display deinitialized successfully");
}

bool __wrap_display_sleep(void) {
    printf("[QEMU DISPLAY] Screen Power -> OFF\n");
    __wrap_display_off();
    return true;
}

esp_err_t __wrap_led_strip_new_rmt_device(const void* strip_conf, const void* rmt_conf, void** ret_strip) {
    printf("[QEMU MOCK] LED Strip RMT device bypassed.\n");
    return ESP_OK;
}

esp_err_t __wrap_gpio_sleep_sel_en(int gpio_num) {
    return ESP_OK;
}

esp_err_t __wrap_esp_sleep_enable_gpio_wakeup(void) {
    return ESP_OK;
}

esp_err_t __wrap_ledc_set_duty(ledc_mode_t speed_mode, ledc_channel_t channel, uint32_t duty) {
    (void)speed_mode;
    (void)channel;
    (void)duty;
    return ESP_OK;
}

esp_err_t __wrap_ledc_update_duty(ledc_mode_t speed_mode, ledc_channel_t channel) {
    (void)speed_mode;
    (void)channel;
    return ESP_OK;
}

void __wrap_sensor_manager_init(void) {
    printf("[QEMU MOCK] Sensor manager bypassed.\n");
    return;
}

void __wrap_sensor_manager_deinit(void) {
    printf("[QEMU MOCK] Sensor manager bypassed.\n");
    return;
}

esp_err_t __wrap_adc_cali_create_scheme_curve_fitting(const adc_cali_curve_fitting_config_t *config, adc_cali_handle_t *ret_handle) {
    *ret_handle = (adc_cali_handle_t)0xDEADBEEF; 
    return ESP_OK;
}

esp_err_t __wrap_adc_cali_delete_scheme_curve_fitting(const adc_cali_handle_t* ret) {
    return ESP_OK;
}

int __wrap_gpio_manager_read_battery_mv(void) {
    return 0;
}

uint32_t __wrap_adc_calc_hw_calibration_code(int adc_n, int atten) {
    return 0; 
}
