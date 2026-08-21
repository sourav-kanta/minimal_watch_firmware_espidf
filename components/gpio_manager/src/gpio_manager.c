#include <gpio_manager.h>
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <common_consts.h>
#include <gpio_pins.h>
#include <event_manager.h>
#include <esp_sleep.h>
#include <esp_err.h>
#include <esp_log.h>
#include <storage_manager.h>

#define DISPLAY_BACKLIGHT_CHANNEL LEDC_CHANNEL_1

static const char* TAG = "GPIO Manager";
static bool initialized = false;
static const ledc_timer_t backlight_timer = LEDC_TIMER_1;
static uint8_t backlight_percent = 100;

static void gpio_manager_arm_wakeup_interrupt(void) {
    if(!initialized) return;
    esp_err_t err = gpio_wakeup_enable(ENCODER_KEY_OK, GPIO_INTR_LOW_LEVEL);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable gpio as sleep source : %s", esp_err_to_name(err));
    }
}

static void gpio_manager_disarm_wakeup_interrupt(void) {
    if(!initialized) return;
    esp_err_t err = gpio_wakeup_disable(ENCODER_KEY_OK);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disable gpio as sleep source : %s", esp_err_to_name(err));
    }
}

static void save_backlight_brightness(int percent) {
    uint8_t pct = percent;
    bool success = storage_manager_save_key(CORE_SYSTEM_APP_ID, "BACKLIGHT", &pct, sizeof(uint8_t));
    if(!success) {
        ESP_LOGE(TAG, "Unable to store brightness");
    }
}

static int retrieve_backlight_brightness(void) {
    uint8_t percent;
    uint8_t data_len = sizeof(uint8_t);
    bool success = storage_manager_retrieve_key(CORE_SYSTEM_APP_ID, "BACKLIGHT", &percent, &data_len);
    if(success) {
        assert(data_len == sizeof(uint8_t));
        if(percent > 100) {
            ESP_LOGE(TAG, "Invalid brightness percentage. Default to 100%");
            return 100;
        }
        else {
            return percent;
        }
    }
    else {
        // No key found, first time boot
        backlight_percent = 100;
        save_backlight_brightness(100);
    }
    return backlight_percent;
}

int gpio_manager_backlight_get_brightness(void) {
    if(!initialized) return 100;
    return backlight_percent;
}

bool gpio_manager_backlight_set_brightness(int percent) {
    if(!initialized || (percent < 0 || percent > 100)) return false;
    if(percent != backlight_percent) {
        save_backlight_brightness(percent);
    }
    ESP_LOGI(TAG, "Setting brightness to : %d%%", percent);
    backlight_percent = percent;
    int duty = (percent * 4095) / 100;
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, DISPLAY_BACKLIGHT_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, DISPLAY_BACKLIGHT_CHANNEL));
    return true;
}

void gpio_manager_power_backlight(void) {
    if(!initialized) return;
    gpio_manager_backlight_set_brightness(backlight_percent);
}

void gpio_manager_backlight_off(void) {
    if(!initialized) return;
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, DISPLAY_BACKLIGHT_CHANNEL, 0)); 
    ESP_ERROR_CHECK(ledc_stop(LEDC_LOW_SPEED_MODE, DISPLAY_BACKLIGHT_CHANNEL, 0));
}

void gpio_manager_init(void) {
    if(initialized) return;
    esp_err_t err = gpio_install_isr_service(0);
    if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "GPIO ISR service already installed.");
    }

    gpio_config_t ps_pin_conf = {
        .pin_bit_mask = (1ULL << SYSTEM_TPS_PS_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE 
    };
    err = gpio_config(&ps_pin_conf);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure TPS PS pin : %s", esp_err_to_name(err));
    }
    gpio_set_level(SYSTEM_TPS_PS_PIN, 1);

    gpio_config_t wakeup_gpio_conf = {
        .pin_bit_mask = (1ULL << ENCODER_KEY_OK),
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE
    };
    err = gpio_config(&wakeup_gpio_conf);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure encoder ok button : %s", esp_err_to_name(err));
    }
    err = esp_sleep_enable_gpio_wakeup();
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable gpio sleep wakeup : %s", esp_err_to_name(err));
    }
    
    err = gpio_sleep_sel_en(ENCODER_KEY_OK);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure gpio as sleep source : %s", esp_err_to_name(err));
    }

    backlight_percent = retrieve_backlight_brightness();
    ledc_timer_config_t display_bl_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_12_BIT,
        .freq_hz = 1*1000,
        .clk_cfg = LEDC_AUTO_CLK,
        .timer_num = backlight_timer
    };
    ESP_ERROR_CHECK(ledc_timer_config(&display_bl_config));

    ledc_channel_config_t backlight_channel = {
        .channel = DISPLAY_BACKLIGHT_CHANNEL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = backlight_timer,
        .gpio_num = DISPLAY_PIN_BACKLIGHT,
        .duty = 0,
        .hpoint = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&backlight_channel)); 

    initialized = true;
    gpio_manager_arm_wakeup_interrupt();
    ESP_LOGI(TAG, "Gpio manager initialized");
}

void gpio_manager_deinit(void) {
    if(!initialized) return;
    gpio_manager_disarm_wakeup_interrupt();
    esp_err_t err = gpio_uninstall_isr_service();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "GPIO ISR service uninstalled successfully.");
    } else {
        ESP_LOGE(TAG, "Failed to uninstall ISR service: %s", esp_err_to_name(err));
    }
    initialized = false;
}

void gpio_manager_enter_active_mode(void) {
    if (!initialized) return;
}

void gpio_manager_enter_background_mode(void) {
    if (!initialized) return;
}
