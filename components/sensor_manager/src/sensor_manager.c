#include <sensor_manager.h>
#include <imu_driver.h>
#include <bmp180_driver.h>
#include <event_manager.h>
#include <runtime_manager.h>
#include <gpio_pins.h>
#include <esp_attr.h>
#include <esp_log.h>
#include <sensor_manager.h>
#include <battery_driver.h>

static const char* TAG = "Sensor manager";
static DRAM_ATTR uint8_t imu_fifo[800] __attribute__((aligned(32)));

static void process_imu_data(void *arg, runtime_abort_flag_t* flag) {
    int temp_scaled_100;
    imu_err_t imu_err = imu_read_temperature(&temp_scaled_100);
    if(imu_err != IMU_OK) {
        ESP_LOGE(TAG, "Imu read temp failed");
    }
    ESP_LOGI(TAG, "IMU Temp = %d", temp_scaled_100); 
    
    size_t samples = sizeof(imu_fifo);
    imu_err = imu_read_fifo_buffer(imu_fifo, &samples);
    if(imu_err != IMU_OK) {
        ESP_LOGE(TAG, "Fifo read failed");
    }
    
    if(*flag == true) {
        ESP_LOGW(TAG, "Used up time quota for work window. Skipping IMU work");
        memset(imu_fifo, 0, sizeof(imu_fifo));
        return;
    }

    // 2 bytes each and 3 words for a single read
    samples = (samples / 3) / 2; 
    ESP_LOGI(TAG, "Read %u FIFO samples", samples);
    
    // Data starts from index 1 and max samples configured 128
    // IMU on board is rotated -90 degree and soldered on the backside
    // so x=y y=-x (z is fine as imu measures normal)
    for(int i=samples*6; i>=1; i=i-6) {
        uint8_t raw_z_h = imu_fifo[i];
        uint8_t raw_z_l = imu_fifo[i-1];
        uint8_t raw_y_h = imu_fifo[i-4];
        uint8_t raw_y_l = imu_fifo[i-5];
        uint8_t raw_x_h = imu_fifo[i-2];
        uint8_t raw_x_l = imu_fifo[i-3];

        int16_t raw_z = (int16_t)(((uint16_t)raw_z_l) | (((uint16_t)raw_z_h)<<8)); 
        int16_t raw_x = (int16_t)(((uint16_t)raw_x_l) | (((uint16_t)raw_x_h)<<8)); 
        int16_t raw_y = (int16_t)(((uint16_t)raw_y_l) | (((uint16_t)raw_y_h)<<8));
        
        // Each sample scaled 100 and converted from 8g to g
        int sample_x = (int)(raw_x*100)/4096; 
        int sample_y = -1 * (int)(raw_y*100)/4096; 
        int sample_z = (int)(raw_z*100)/4096; 
        ESP_LOGD(TAG, "X : %d, Y : %d, Z : %d", sample_x, sample_y, sample_z); 
    }
    memset(imu_fifo, 0, sizeof(imu_fifo));
} 

static void process_bmp_data(void *arg, runtime_abort_flag_t* flag) {
    // ------------- Test BMP driver -------------
    int bmp_temp_scaled_10;
    int altitude_scaled_10;

    // bool success = bmp180_read_temp(&bmp_temp_scaled_10);
    // if(!success) ESP_LOGE(TAG, "Failed to read BMP180 temp");
    // ESP_LOGI(TAG, "BMP180 Temp = %d", bmp_temp_scaled_10);
    
    bool success = bmp180_read_temp_and_altitude(&bmp_temp_scaled_10, &altitude_scaled_10, 
                                            BMP180_SENSOR_MODE_HIGH_RES);
    if(!success) ESP_LOGE(TAG, "Failed to read BMP180 altitude");

    ESP_LOGI(TAG, "BMP180 Temp = %d, Altitude = %d", bmp_temp_scaled_10, altitude_scaled_10);
    // Fire event to update system altitude
    
    // ------------------ End Test -------------------------
}

static void process_battery_data(void *arg, runtime_abort_flag_t* flag) {
    uint8_t batt_pct = battery_driver_read_battery_percentage();
    ESP_LOGI(TAG, "Battery percentage : %u", batt_pct);
    // Fire event to update system battery percentage
}

static void schedule_sensor_work(const event_t* event) {
    runtime_work_item_t imu_work = {
        .handler = process_imu_data,
        .type = WORK_TYPE_SYSTEM,
    };
    bool success = schedule_system_work(&imu_work);
    if(!success) {
        ESP_LOGE(TAG, "Failed scheduling IMU work. Skipping");
    }
    
    /* ------------------- Move these two to SENSOR_TICK (1min interval) -----------*/

    runtime_work_item_t bmp_work = {
        .handler = process_bmp_data,
        .type = WORK_TYPE_SYSTEM,
    };
    success = schedule_system_work(&bmp_work);
    if(!success) {
        ESP_LOGE(TAG, "Failed scheduling BMP work. Skipping");
    }
    
    runtime_work_item_t batt_work = {
        .handler = process_battery_data,
        .type = WORK_TYPE_SYSTEM,
    };
    success = schedule_system_work(&batt_work);
    if(!success) {
        ESP_LOGE(TAG, "Failed scheduling Battery work. Skipping");
    }
}

void sensor_manager_arm_wakeup_interrupt(void) {
    imu_setup_wake_on_motion();
}

void sensor_manager_disarm_wakup_interrupt(void) {
    imu_disable_wake_on_motion();
}

void sensor_manager_init(void) {
    bmp180_init();
    battery_driver_init();
    imu_err_t imu_err = IMU_OK;
    imu_params_t params = {
        .CS_PIN = SENSOR_IMU_CS,
        .MISO_PIN = SENSOR_IMU_MISO,
        .MOSI_PIN = SENSOR_IMU_MOSI,
        .CLK_PIN = SENSOR_IMU_SCL,
        .INT_PIN = SYSTEM_PIN_WAKEUP,
        .freq = 10*1000*1000
    };
    imu_err = imu_init(&params);
    if(imu_err != IMU_OK) {
        ESP_LOGE(TAG, "Imu init failed");
    }
    event_subscribe(EVENT_WORK_TICK, schedule_sensor_work);
}

void sensor_manager_deinit(void) { 
    event_unsubscribe(EVENT_WORK_TICK, schedule_sensor_work);
    imu_err_t err = imu_enter_low_power_mode();
    if(err != IMU_OK) {
        ESP_LOGE(TAG, "Failed to switch to lowpower mode");
    }
    battery_driver_deinit();
    bmp180_deinit();
}
