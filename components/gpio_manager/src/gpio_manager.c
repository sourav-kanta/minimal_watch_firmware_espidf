#include <gpio_manager.h>
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <common_consts.h>
#include <gpio_pins.h>
#include <event_manager.h>
#include <esp_sleep.h>
#include <led_strip.h>
#include <esp_err.h>
#include <esp_log.h>
#include <storage_manager.h>

#define S3_NEOPIXEL_PIN     GPIO_NUM_48
#define LED_STRIP_LENGTH    1
#define DISPLAY_BACKLIGHT_CHANNEL LEDC_CHANNEL_0

static const char* TAG = "GPIO Manager";
static bool initialized = false;
static led_strip_handle_t led_strip = NULL; 
static const ledc_timer_t backlight_timer = 0;
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

void gpio_manager_backlight_set_brightness(int percent) {
    if(!initialized || (percent < 0 || percent > 100)) return;
    if(percent != backlight_percent) {
        save_backlight_brightness(percent);
    }
    backlight_percent = percent;
    int duty = (percent * 4095) / 100;
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, DISPLAY_BACKLIGHT_CHANNEL, duty)); // 50%
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, DISPLAY_BACKLIGHT_CHANNEL));
}

void gpio_manager_power_backlight(void) {
    if(!initialized) return;
    gpio_manager_backlight_set_brightness(backlight_percent);
}

void gpio_manager_backlight_off(void) {
    if(!initialized) return;
    ESP_ERROR_CHECK(ledc_stop(LEDC_LOW_SPEED_MODE, DISPLAY_BACKLIGHT_CHANNEL, 0));
}

void gpio_manager_init(void) {
    if(initialized) return;
    esp_err_t err = gpio_install_isr_service(0);
    if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "GPIO ISR service already installed.");
    }
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

    led_strip_config_t strip_config = {
        .strip_gpio_num = S3_NEOPIXEL_PIN,
        .max_leds = LED_STRIP_LENGTH,
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, 
        .flags.with_dma = false,
    };
    
    err = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to bind led_strip component: %s", esp_err_to_name(err));
        return;
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
    if (led_strip != NULL) {
        led_strip_del(led_strip);
        led_strip = NULL;
    }
    esp_err_t err = gpio_uninstall_isr_service();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "GPIO ISR service uninstalled successfully.");
    } else {
        ESP_LOGE(TAG, "Failed to uninstall ISR service: %s", esp_err_to_name(err));
    }
    initialized = false;
}

void gpio_manager_debug_led_on(void) {
    if (!initialized || led_strip == NULL) return;
    led_strip_set_pixel(led_strip, 0, 0, 32, 0);
    led_strip_refresh(led_strip);
}

void gpio_manager_debug_led_off(void) {
    if (!initialized || led_strip == NULL) return;
    led_strip_clear(led_strip);
}
