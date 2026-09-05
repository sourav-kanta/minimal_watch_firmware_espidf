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
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>

#define DISPLAY_BACKLIGHT_CHANNEL           LEDC_CHANNEL_1
#define BATTERY_READ_ADC_UNIT               ADC_UNIT_1
#define BATT_ADC_CHAN                       ADC_CHANNEL_6
#define BATTERY_VOLTAGE_DIV_MULTIPLIER      2

static const char* TAG = "GPIO Manager";
static bool initialized = false;
static const ledc_timer_t backlight_timer = LEDC_TIMER_1;
static uint8_t backlight_percent = 100;
static adc_oneshot_unit_handle_t adc_handle = NULL;
static adc_cali_handle_t adc_cali_handle = NULL;

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

void gpio_manager_enable_battery_read(void) {
    if(!initialized) return;
    gpio_set_level(SENSOR_BATTERY_READ_EN_PIN, 1);
}

void gpio_manager_disable_battery_read(void) {
    if(!initialized) return;
    gpio_set_level(SENSOR_BATTERY_READ_EN_PIN, 0);
}

int gpio_manager_read_battery_mv(void) {
    if(!initialized) return -1;
    assert(adc_handle);
    assert(adc_cali_handle);
    
    int raw_sample = 0;
    int voltage_mv = 0;
    
    esp_err_t err = adc_oneshot_read(adc_handle, BATT_ADC_CHAN, &raw_sample);
    if(err == ESP_OK) {
        err = adc_cali_raw_to_voltage(adc_cali_handle, raw_sample, &voltage_mv);
        if(err != ESP_OK) {
            ESP_LOGE(TAG, "Error calibrating ADC to voltage: %s", esp_err_to_name(err));
            return -1;
        }
    } else {
        ESP_LOGE(TAG, "Error reading ADC count: %s", esp_err_to_name(err));
        return -1;
    }
    
    // Battery voltage divider is tapped at 50%
    return voltage_mv * BATTERY_VOLTAGE_DIV_MULTIPLIER;
}

bool gpio_manager_is_charging(void) {
    if(!initialized) return false;
    return gpio_get_level(SYSTEM_PIN_PG) == 0;
}

static void gpio_lbo_isr_handler_cb(void* arg) {
    /* --------------  Fire event EVENT_BATTERY_DEAD ---------------*/
}

void gpio_manager_init(void) {
    if(initialized) return;
    esp_err_t err = gpio_install_isr_service(0);
    if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "GPIO ISR service already installed.");
    }

    // Implement the deep sleep disconnect holds

    gpio_config_t batt_read_en_pin_conf = {
        .pin_bit_mask = (1ULL << SENSOR_BATTERY_READ_EN_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE 
    };
    err = gpio_config(&batt_read_en_pin_conf);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure battery read enable pin : %s", esp_err_to_name(err));
    }
    ESP_ERROR_CHECK(gpio_set_level(SENSOR_BATTERY_READ_EN_PIN, 0));
    ESP_ERROR_CHECK(gpio_sleep_sel_en(SENSOR_BATTERY_READ_EN_PIN));
    ESP_ERROR_CHECK(gpio_sleep_set_direction(SENSOR_BATTERY_READ_EN_PIN, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_sleep_set_pull_mode(SENSOR_BATTERY_READ_EN_PIN, GPIO_PULLDOWN_ENABLE));

    adc_oneshot_unit_init_cfg_t batt_adc_config = {
        .unit_id = BATTERY_READ_ADC_UNIT, 
    };

    ESP_LOGI(TAG, "Configuring ADC");
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&batt_adc_config, &adc_handle));
    adc_oneshot_chan_cfg_t batt_adc_chan_config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12, 
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, BATT_ADC_CHAN, &batt_adc_chan_config));
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = BATTERY_READ_ADC_UNIT,
        .chan = BATT_ADC_CHAN,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle));

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

    ESP_ERROR_CHECK(gpio_set_level(SYSTEM_TPS_PS_PIN, 1));
    ESP_ERROR_CHECK(gpio_sleep_sel_en(SYSTEM_TPS_PS_PIN));
    ESP_ERROR_CHECK(gpio_sleep_set_direction(SYSTEM_TPS_PS_PIN, GPIO_MODE_INPUT));
    ESP_ERROR_CHECK(gpio_sleep_set_pull_mode(SYSTEM_TPS_PS_PIN, GPIO_FLOATING));

    gpio_config_t pg_pin_conf = {
        .pin_bit_mask = (1ULL << SYSTEM_PIN_PG),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    err = gpio_config(&pg_pin_conf);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure PG pin : %s", esp_err_to_name(err));
    }
    ESP_ERROR_CHECK(gpio_sleep_sel_en(SYSTEM_PIN_PG));
    ESP_ERROR_CHECK(gpio_sleep_set_direction(SYSTEM_PIN_PG, GPIO_MODE_INPUT));
    ESP_ERROR_CHECK(gpio_sleep_set_pull_mode(SYSTEM_PIN_PG, GPIO_FLOATING));

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
        ESP_LOGE(TAG, "Failed to configure ENCODER_KEY_OK gpio as sleep source : %s", esp_err_to_name(err));
    }

    gpio_config_t lbo_pin_conf = {
        .pin_bit_mask = (1ULL << SYSTEM_PIN_LBO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    err = gpio_config(&lbo_pin_conf);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure PG pin : %s", esp_err_to_name(err));
    }
    
    // Wakeup the MCU, power manager catches the reason to find cause as gpio and LBO low and 
    // intiates system level deep sleep. Disabled for now during usb testing
    // ESP_ERROR_CHECK(gpio_sleep_sel_en(SYSTEM_PIN_LBO));
    // ESP_ERROR_CHECK(gpio_sleep_set_direction(SYSTEM_PIN_LBO, GPIO_MODE_INPUT));
    // ESP_ERROR_CHECK(gpio_sleep_set_pull_mode(SYSTEM_PIN_LBO, GPIO_FLOATING));
    // ESP_ERROR_CHECK(gpio_wakeup_enable(SYSTEM_PIN_LBO, GPIO_INTR_LOW_LEVEL));
    // ESP_ERROR_CHECK(gpio_isr_handler_add(SYSTEM_PIN_LBO, gpio_lbo_isr_handler_cb, NULL));

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
    
    if(adc_handle) {
        ESP_ERROR_CHECK(adc_oneshot_del_unit(adc_handle));
        adc_handle = NULL;
    }
    if (adc_cali_handle) {
        ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(adc_cali_handle));
        adc_cali_handle = NULL;
    }

    // Implement the deep sleep holds
    
    initialized = false;
}

void gpio_manager_enter_active_mode(void) {
    if (!initialized) return;
}

void gpio_manager_enter_background_mode(void) {
    if (!initialized) return;
}
