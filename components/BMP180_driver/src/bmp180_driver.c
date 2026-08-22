#include <bmp180_driver.h>
#include <bmp180_internal.h>
#include <bmp180_reg_defs.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <storage_manager.h>
#include <common_consts.h>
#include <math.h>

#define CAL_REG_WORD_AC1_IDX         0
#define CAL_REG_WORD_AC2_IDX         2
#define CAL_REG_WORD_AC3_IDX         4
#define CAL_REG_WORD_AC4_IDX         6
#define CAL_REG_WORD_AC5_IDX         8
#define CAL_REG_WORD_AC6_IDX         10
#define CAL_REG_WORD_B1_IDX          12
#define CAL_REG_WORD_B2_IDX          14
#define CAL_REG_WORD_MB_IDX          16
#define CAL_REG_WORD_MC_IDX          18
#define CAL_REG_WORD_MD_IDX          20

typedef struct {
    int16_t AC1;
    int16_t AC2;
    int16_t AC3;
    uint16_t AC4;
    uint16_t AC5;
    uint16_t AC6;
    int16_t B1;
    int16_t B2;
    int16_t MB;
    int16_t MC;
    int16_t MD;
} calibration_regs_t;

static const char* TAG = "BMP180 Driver";
static bmp180_bus_t esp32s3_bmp_driver;
static calibration_regs_t calibration_regs;


bool bmp180_read_temp(int *temp_scaled_10) {
    if(temp_scaled_10 == NULL) {
        ESP_LOGE(TAG, "Invalid config. Skip read");
        return false;
    }
    
    // Calculate compensated pressure
    uint8_t cmd = BMP180_CMD_READ_TEMP;
    esp32s3_bmp_driver.write_reg(esp32s3_bmp_driver.handle, BMP180_REG_CONTROL, &cmd, sizeof(cmd));
    TickType_t wait_ticks = pdMS_TO_TICKS(5) == 0 ? 1 : pdMS_TO_TICKS(5);
    vTaskDelay(wait_ticks);
    uint8_t raw_bytes[3];
    if (!esp32s3_bmp_driver.read_reg(esp32s3_bmp_driver.handle, BMP180_REG_DATA_MSB, raw_bytes, 2)) {
        return false;
    }
    int32_t UT = (int32_t)(((uint16_t)raw_bytes[0] << 8) | raw_bytes[1]);
    int32_t X1 = ((UT - (int32_t)calibration_regs.AC6) * (int32_t)calibration_regs.AC5) >> 15;
    int32_t X2 = ((int32_t)calibration_regs.MC << 11) / (X1 + (int32_t)calibration_regs.MD);
    int32_t B5 = X1 + X2;
    *temp_scaled_10 = (B5 + 8) >> 4;

    return true; 
}

bool bmp180_read_temp_and_altitude(int *temp_scaled_10, int *alt_meters_scaled_10, bmp180_sensor_mode_t mode) {
    if(temp_scaled_10 == NULL || alt_meters_scaled_10 == NULL) {
        ESP_LOGE(TAG, "Invalid config. Skip read");
        return false;
    }
    
    // Calculate compensated pressure
    uint8_t cmd = BMP180_CMD_READ_TEMP;
    esp32s3_bmp_driver.write_reg(esp32s3_bmp_driver.handle, BMP180_REG_CONTROL, &cmd, sizeof(cmd));
    TickType_t wait_ticks = pdMS_TO_TICKS(5) == 0 ? 1 : pdMS_TO_TICKS(5);
    vTaskDelay(wait_ticks);
    uint8_t raw_bytes[3];
    if (!esp32s3_bmp_driver.read_reg(esp32s3_bmp_driver.handle, BMP180_REG_DATA_MSB, raw_bytes, 2)) {
        return false;
    }
    int32_t UT = (int32_t)(((uint16_t)raw_bytes[0] << 8) | raw_bytes[1]);
    int32_t X1 = ((UT - (int32_t)calibration_regs.AC6) * (int32_t)calibration_regs.AC5) >> 15;
    int32_t X2 = ((int32_t)calibration_regs.MC << 11) / (X1 + (int32_t)calibration_regs.MD);
    int32_t B5 = X1 + X2;
    *temp_scaled_10 = (B5 + 8) >> 4;

    // Calculate compensated pressure
    uint8_t oss = (uint8_t)mode;
    if (oss >= BMP180_SENSOR_MODE_INVALID) oss = 0;
    uint8_t delay_ms[4];
    delay_ms[BMP180_SENSOR_MODE_ULTRA_LOW_POWER] = 5;
    delay_ms[BMP180_SENSOR_MODE_STANDARD] = 8;
    delay_ms[BMP180_SENSOR_MODE_HIGH_RES] = 14;
    delay_ms[BMP180_SENSOR_MODE_ULTRA_HIGH_RES] = 26;

    cmd = BMP180_CMD_READ_PRESS | (oss << BMP180_OSS_SHIFT);
    if (!esp32s3_bmp_driver.write_reg(esp32s3_bmp_driver.handle, BMP180_REG_CONTROL, &cmd, 1)) {
        return false;
    }
    wait_ticks = pdMS_TO_TICKS(delay_ms[oss]) == 0 ? 1 : pdMS_TO_TICKS(delay_ms[oss]);
    vTaskDelay(wait_ticks);
    if (!esp32s3_bmp_driver.read_reg(esp32s3_bmp_driver.handle, BMP180_REG_DATA_MSB, raw_bytes, 3)) {
        return false;
    }
    int32_t UP = (int32_t)((((uint32_t)raw_bytes[0] << 16) |
                             ((uint32_t)raw_bytes[1] << 8) |
                             (uint32_t)raw_bytes[2]) >> (8 - oss));
    int32_t B6 = B5 - 4000;
    X1 = ((int32_t)calibration_regs.B2 * ((B6 * B6) >> 12)) >> 11;
    X2 = ((int32_t)calibration_regs.AC2 * B6) >> 11;
    int32_t X3 = X1 + X2;
    int32_t B3 = (((((int32_t)calibration_regs.AC1 * 4) + X3) << oss) + 2) >> 2;
    X1 = ((int32_t)calibration_regs.AC3 * B6) >> 13;
    X2 = ((int32_t)calibration_regs.B1 * ((B6 * B6) >> 12)) >> 16;
    X3 = ((X1 + X2) + 2) >> 2;
    uint32_t B4 = ((uint32_t)calibration_regs.AC4 * (uint32_t)(X3 + 32768)) >> 15;
    uint32_t B7 = ((uint32_t)UP - B3) * (50000 >> oss);
    int32_t p;
    if (B7 < 0x80000000) {
        p = (B7 * 2) / B4;
    } else {
        p = (B7 / B4) * 2;
    }
    X1 = (p >> 8) * (p >> 8);
    X1 = (X1 * 3038) >> 16;
    X2 = (-7357 * p) >> 16;
    p = p + ((X1 + X2 + 3791) >> 4);
    float sea_level_pa = 101325.0f;
    *alt_meters_scaled_10 = (int)((44330.0f * (1.0f - powf((float)p / sea_level_pa, 0.1902949f)))*10);
    
    return true;    
}

static void populate_calibration_regs(uint8_t* data) {
    calibration_regs.AC1 = (int16_t)(((uint16_t)data[CAL_REG_WORD_AC1_IDX] << 8) | data[CAL_REG_WORD_AC1_IDX + 1]);
    calibration_regs.AC2 = (int16_t)(((uint16_t)data[CAL_REG_WORD_AC2_IDX] << 8) | data[CAL_REG_WORD_AC2_IDX + 1]);
    calibration_regs.AC3 = (int16_t)(((uint16_t)data[CAL_REG_WORD_AC3_IDX] << 8) | data[CAL_REG_WORD_AC3_IDX + 1]);
    calibration_regs.AC4 = (uint16_t)(((uint16_t)data[CAL_REG_WORD_AC4_IDX] << 8) | data[CAL_REG_WORD_AC4_IDX + 1]);
    calibration_regs.AC5 = (uint16_t)(((uint16_t)data[CAL_REG_WORD_AC5_IDX] << 8) | data[CAL_REG_WORD_AC5_IDX + 1]);
    calibration_regs.AC6 = (uint16_t)(((uint16_t)data[CAL_REG_WORD_AC6_IDX] << 8) | data[CAL_REG_WORD_AC6_IDX + 1]);
    calibration_regs.B1  = (int16_t)(((uint16_t)data[CAL_REG_WORD_B1_IDX]  << 8) | data[CAL_REG_WORD_B1_IDX + 1]);
    calibration_regs.B2  = (int16_t)(((uint16_t)data[CAL_REG_WORD_B2_IDX]  << 8) | data[CAL_REG_WORD_B2_IDX + 1]);
    calibration_regs.MB  = (int16_t)(((uint16_t)data[CAL_REG_WORD_MB_IDX]  << 8) | data[CAL_REG_WORD_MB_IDX + 1]);
    calibration_regs.MC  = (int16_t)(((uint16_t)data[CAL_REG_WORD_MC_IDX]  << 8) | data[CAL_REG_WORD_MC_IDX + 1]);
    calibration_regs.MD  = (int16_t)(((uint16_t)data[CAL_REG_WORD_MD_IDX]  << 8) | data[CAL_REG_WORD_MD_IDX + 1]);
}

static void read_and_store_calibration_data_if_required(void) {
    uint8_t cal_reg[22];
    uint8_t len = sizeof(cal_reg);
    if(storage_manager_retrieve_key(CORE_SYSTEM_APP_ID, "BMP180", cal_reg, &len) == true) {
        assert(len == 22);
        ESP_LOGI(TAG, "Calibration data stored and available");
        populate_calibration_regs(cal_reg);
        return;
    }

    esp32s3_bmp_driver.read_reg(esp32s3_bmp_driver.handle, BMP180_REG_AC1_H, cal_reg, sizeof(cal_reg)); 
    populate_calibration_regs(cal_reg);

    bool success = storage_manager_save_key(CORE_SYSTEM_APP_ID, "BMP180", cal_reg, sizeof(cal_reg));
    if(!success) {
        ESP_LOGE(TAG, "Failed to store calibration values");
    }
}

void bmp180_init(void) {
    bmp180_configure_i2c(&esp32s3_bmp_driver);
    uint8_t dev = 0;
    esp32s3_bmp_driver.read_reg(esp32s3_bmp_driver.handle, BMP180_REG_CHIP_ID, &dev, sizeof(dev));
    if(dev == BMP180_CHIP_ID_VALUE) {
        ESP_LOGI(TAG, "Initialized BMP180 driver successfully");
        read_and_store_calibration_data_if_required();
    }
    else {
        ESP_LOGI(TAG, "Error in initializing BMP180 driver");
    }
}

void bmp180_deinit(void) {
    bmp180_release_i2c(&esp32s3_bmp_driver);
    memset(&esp32s3_bmp_driver, 0, sizeof(esp32s3_bmp_driver));
    ESP_LOGI(TAG, "Deinitialized BMP180 driver");
}
