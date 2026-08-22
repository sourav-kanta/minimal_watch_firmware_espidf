#ifndef GPIO_PINS_H
#define GPIO_PINS_H

#define BOARD_STATUS_RGB_LED_IO  48

#define DISPLAY_LCD_HOST                    SPI2_HOST
#define DISPLAY_PIN_NUM_MOSI                11
#define DISPLAY_PIN_NUM_CLK                 10
#define DISPLAY_PIN_NUM_CS                  9
#define DISPLAY_PIN_NUM_DC                  12
#define DISPLAY_PIN_NUM_RST                 13
#define DISPLAY_PIN_BACKLIGHT               46 
#define ENCODER_PIN_A                       6
#define ENCODER_PIN_B                       4
#define ENCODER_KEY_OK                      5

#define SYSTEM_PIN_PG                       17
#define SYSTEM_PIN_WAKEUP                   1
#define SYSTEM_PIN_LBO                      2
#define SYSTEM_TPS_PS_PIN                   14

#define IR_BLASTER_GATE_PIN                 3
#define PIEZO_GATE_PIN                      8

#define SENSOR_BATTERY_READ_PIN             7
#define SENSOR_BATTERY_READ_EN_PIN          40
#define SENSOR_I2C_SDA_PIN                  21
#define SENSOR_I2C_SCL_PIN                  48
#define SENSOR_IMU_CS                       38
#define SENSOR_IMU_SCL                      39
#define SENSOR_IMU_MOSI                     41
#define SENSOR_IMU_MISO                     42
#define SENSOR_GSR_READ                     18
#define SENSOR_IMU_HOST                     SPI3_HOST

#endif /* GPIO_PINS_H */
