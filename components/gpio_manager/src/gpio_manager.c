#include <gpio_manager.h>
#include <driver/gpio.h>
#include <common_consts.h>
#include <gpio_pins.h>
#include <event_manager.h>
#include <esp_sleep.h>
#include <led_strip.h>
#include <esp_err.h>
#include <esp_log.h>

#define S3_NEOPIXEL_PIN     GPIO_NUM_48
#define LED_STRIP_LENGTH    1

static const char* TAG = "GPIO Manager";
static bool initialized = false;
static led_strip_handle_t led_strip = NULL; 

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
    initialized = true;
    gpio_manager_arm_wakeup_interrupt();
    ESP_LOGI(TAG, "Gpio manager initialized");
}

void gpio_manager_deinit(void) {
    esp_err_t err = gpio_uninstall_isr_service();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "GPIO ISR service uninstalled successfully.");
    } else {
        ESP_LOGE(TAG, "Failed to uninstall ISR service: %s", esp_err_to_name(err));
    }
    gpio_manager_disarm_wakeup_interrupt();
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
