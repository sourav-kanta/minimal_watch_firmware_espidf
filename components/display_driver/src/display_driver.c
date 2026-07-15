#include <esp_lcd_panel_ops.h>
#include <st7735_driver.h>
#include <esp_lcd_panel_io.h>
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <common_types.h>
#include <gpio_pins.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_system.h>
#include <common_consts.h>
#include <display_fsm.h>

#include <display_driver.h>

static const char *TAG = "DISPLAY";

static flush_cb_t flush_callback = NULL; 
static esp_lcd_panel_handle_t panel_handle = NULL;
static esp_lcd_panel_io_handle_t io_handle = NULL;

static bool spi_transfer_done_cb(esp_lcd_panel_io_handle_t panel_io, 
                                  esp_lcd_panel_io_event_data_t *edata, 
                                  void *user_ctx) 
{
    if(flush_callback) {
        flush_callback();
    }
    return false; 
}

void init_display(flush_cb_t callback) {
    flush_callback = callback;
    spi_bus_config_t buscfg = {
        .sclk_io_num = DISPLAY_PIN_NUM_CLK,
        .mosi_io_num = DISPLAY_PIN_NUM_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = DISPLAY_LVGL_BUFFER_SIZE,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(DISPLAY_LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = DISPLAY_PIN_NUM_DC,
        .cs_gpio_num = DISPLAY_PIN_NUM_CS,
        .pclk_hz = 20 * 1000 * 1000,
        .spi_mode = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .trans_queue_depth = 10,
        .on_color_trans_done = spi_transfer_done_cb
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(DISPLAY_LCD_HOST, &io_config, &io_handle));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = DISPLAY_PIN_NUM_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7735(io_handle, &panel_config, &panel_handle));
    
    esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = spi_transfer_done_cb
    };
    ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(io_handle, &cbs, NULL));
    
    if(panel_handle) {
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, true));
    }
    else {
        ESP_LOGE(TAG, "Panel handle is NULL, panic!");
        esp_system_abort("Unable to init display");        
    }
    init_fsm();
    ESP_LOGI(TAG, "Display initialized successfully");
}

void deinit_display(void)
{ 
    if (panel_handle) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_lcd_panel_disp_on_off(panel_handle, false));
    }

    if (panel_handle) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_lcd_panel_del(panel_handle));
        panel_handle = NULL;
    }

    if (io_handle) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_lcd_panel_io_del(io_handle));
        io_handle = NULL;
    }
    
    ESP_ERROR_CHECK_WITHOUT_ABORT(spi_bus_free(DISPLAY_LCD_HOST));
    flush_callback = NULL;
    ESP_LOGI(TAG, "Display deinitialized");
}

void display_draw_bitmap(int x1, int y1, int x2, int y2, const void* pixels) {
    if(panel_handle) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_lcd_panel_draw_bitmap(panel_handle, x1, y1, x2, y2, pixels));
    }
    else {
        ESP_LOGE(TAG, "Panel handle is NULL, skip draw");
    }
} 

bool display_sleep() {
    bool success = true;
    display_state_t rollback_state = fsm_get_current_display_state();
    
    if(fsm_display_state_transition(DISPLAY_STATE_SLEEP)) {
        if (panel_handle) {
            success = (esp_lcd_panel_disp_sleep(panel_handle, true) == ESP_OK);
        }
        else {
            ESP_LOGE(TAG, "Unable to put display to sleep");
            success = false;
        }
    }
    else {
        ESP_LOGE(TAG, "Invalid state transition");
        success = false;
    }
    
    if(!success) {
        fsm_display_state_transition(rollback_state);
    }
    return success;
}

static bool display_wakeup() {
    if (panel_handle) {
        return (esp_lcd_panel_disp_sleep(panel_handle, false) == ESP_OK);
    }
    else {
        ESP_LOGE(TAG, "Unable to wake up display, invalid handle");
        return false;
    }
}

bool display_on() {
    bool success = true;
    display_state_t rollback_state = fsm_get_current_display_state();
    
    if(rollback_state == DISPLAY_STATE_SLEEP) {
        if(fsm_display_state_transition(DISPLAY_STATE_ON)) {
            success = display_wakeup();
        }
    }
    else if(rollback_state == DISPLAY_STATE_OFF) {
        if(fsm_display_state_transition(DISPLAY_STATE_ON)) { 
            if (panel_handle) {
                success = (esp_lcd_panel_disp_on_off(panel_handle, true) == ESP_OK);
            }
            else {
                ESP_LOGE(TAG, "Unable to turn on display, invalid handle");
                success = false;
            }
        }
    }
    else {
        ESP_LOGE(TAG, "Unable to wake up display");
        success = false;
    }
    
    if(!success) {
        fsm_display_state_transition(rollback_state);
    }
    return success;
}

bool display_off() {
    bool success = true;
    display_state_t rollback_state = fsm_get_current_display_state();
    
    if(fsm_display_state_transition(DISPLAY_STATE_OFF)) {
        if (panel_handle) {
            success = (esp_lcd_panel_disp_on_off(panel_handle, false) == ESP_OK);
        }
        else {
            ESP_LOGE(TAG, "Unable to turn off display, invalid handle");
            success = false;
        }
    }
    else {
        ESP_LOGE(TAG, "Unable to turn off display, invalid transition");
        success = false;
    }
    
    if(!success) {
        fsm_display_state_transition(rollback_state);
    }
    return success;
        
}

