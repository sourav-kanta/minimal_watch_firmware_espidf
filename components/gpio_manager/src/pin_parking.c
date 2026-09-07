#include <pin_parking.h>

#include <gpio_pins.h>
#include <driver/gpio.h>
#include <esp_sleep.h>
#include <esp_log.h>

static const char* TAG = "Pin Parking";

static const int input_pullup[] = {
    DISPLAY_PIN_NUM_CS,
    DISPLAY_PIN_NUM_RST,
    SENSOR_IMU_CS,
};

static const int input_pulldown[] = {
    DISPLAY_PIN_NUM_MOSI,
    DISPLAY_PIN_NUM_CLK,
    DISPLAY_PIN_NUM_DC,
    DISPLAY_PIN_BACKLIGHT,
    SENSOR_IMU_SCL,
    SENSOR_IMU_MOSI,
    SENSOR_IMU_MISO,
    SENSOR_GSR_READ,
    SENSOR_BATTERY_READ_EN_PIN,
};

static const int input_floating[] = {
    ENCODER_PIN_A,
    ENCODER_PIN_B,
    SYSTEM_PIN_PG,
    SYSTEM_TPS_PS_PIN,
    IR_BLASTER_GATE_PIN,
    PIEZO_GATE_PIN,
    SENSOR_I2C_SDA_PIN,
    SENSOR_I2C_SCL_PIN,    
    SENSOR_BATTERY_READ_PIN,
    ENCODER_KEY_OK,
    SYSTEM_PIN_LBO,
    SYSTEM_PIN_WAKEUP
};

void configure_light_sleep_pin_parking(void) {
    for (int i = 0; i < sizeof(input_pullup) / sizeof(input_pullup[0]); i++) {
        ESP_ERROR_CHECK(gpio_sleep_sel_en(input_pullup[i]));
        ESP_ERROR_CHECK(gpio_sleep_set_direction(input_pullup[i], GPIO_MODE_INPUT));
        ESP_ERROR_CHECK(gpio_sleep_set_pull_mode(input_pullup[i], GPIO_PULLUP_ENABLE));
    }

    for (int i = 0; i < sizeof(input_pulldown) / sizeof(input_pulldown[0]); i++) {
        ESP_ERROR_CHECK(gpio_sleep_sel_en(input_pulldown[i]));
        ESP_ERROR_CHECK(gpio_sleep_set_direction(input_pulldown[i], GPIO_MODE_INPUT));
        ESP_ERROR_CHECK(gpio_sleep_set_pull_mode(input_pulldown[i], GPIO_PULLDOWN_ENABLE));
    }

    for (int i = 0; i < sizeof(input_floating) / sizeof(input_floating[0]); i++) {
        ESP_ERROR_CHECK(gpio_sleep_sel_en(input_floating[i]));
        ESP_ERROR_CHECK(gpio_sleep_set_direction(input_floating[i], GPIO_MODE_INPUT));
        ESP_ERROR_CHECK(gpio_sleep_set_pull_mode(input_floating[i], GPIO_FLOATING));
    }

    ESP_LOGI(TAG, "Light sleep IO parking configured.");
}

void configure_deep_sleep_pin_parking(void) {
    const int hold_low_pins[] = {SENSOR_BATTERY_READ_EN_PIN, DISPLAY_PIN_BACKLIGHT};
    for(int i = 0; i < sizeof(hold_low_pins)/sizeof(hold_low_pins[0]); i++) {
        ESP_ERROR_CHECK(gpio_set_direction(hold_low_pins[i], GPIO_MODE_OUTPUT));
        ESP_ERROR_CHECK(gpio_set_level(hold_low_pins[i], 0));
        ESP_ERROR_CHECK(gpio_hold_en(hold_low_pins[i]));
    }

    const int hold_high_pins[] = {DISPLAY_PIN_NUM_CS, SENSOR_IMU_CS, DISPLAY_PIN_NUM_RST};
    for(int i = 0; i < sizeof(hold_high_pins)/sizeof(hold_high_pins[0]); i++) {
        ESP_ERROR_CHECK(gpio_set_direction(hold_high_pins[i], GPIO_MODE_OUTPUT));
        ESP_ERROR_CHECK(gpio_set_level(hold_high_pins[i], 1));
        ESP_ERROR_CHECK(gpio_hold_en(hold_high_pins[i]));
    }

    gpio_deep_sleep_hold_en();
    
    ESP_LOGI(TAG, "Deep sleep pin holds engaged.");
}

void configure_active_mode_pin_unparking(void) {
    gpio_deep_sleep_hold_dis();

    ESP_ERROR_CHECK(gpio_hold_dis(DISPLAY_PIN_BACKLIGHT));
    ESP_ERROR_CHECK(gpio_hold_dis(SENSOR_BATTERY_READ_EN_PIN));
    ESP_ERROR_CHECK(gpio_hold_dis(DISPLAY_PIN_NUM_CS));
    ESP_ERROR_CHECK(gpio_hold_dis(SENSOR_IMU_CS));
    ESP_ERROR_CHECK(gpio_hold_dis(DISPLAY_PIN_NUM_RST));

    ESP_LOGI(TAG, "Deep sleep pin holds released for active mode.");
}
