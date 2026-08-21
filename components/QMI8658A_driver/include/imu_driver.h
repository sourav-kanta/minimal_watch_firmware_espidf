#ifndef IMU_DRIVER_H
#define IMU_DRIVER_H

#include <stdint.h>

typedef struct {
    uint8_t CS_PIN;
    uint8_t MISO_PIN;
    uint8_t MOSI_PIN;
    uint8_t CLK_PIN;
    uint8_t INT_PIN;
    uint32_t freq;
} imu_params_t;

typedef enum {
    IMU_OK,
    IMU_INVALID_CONFIG,
    IMU_FAILED
} imu_err_t;

imu_err_t imu_init(const imu_params_t* param);
imu_err_t imu_reset(void);

imu_err_t imu_enable_accelerometer(void);
imu_err_t imu_disable_accelerometer(void);
imu_err_t imu_enable_gyro(void);
imu_err_t imu_disable_gyro(void);

imu_err_t imu_enter_low_power_accel_only_mode(void);
imu_err_t imu_enter_high_accuracy_mode(void);
imu_err_t imu_enter_low_power_mode(void);

imu_err_t imu_enable_pedometer(void);
imu_err_t imu_disable_pedometer(void);
imu_err_t imu_reset_pedometer(void);
imu_err_t imu_read_pedometer_steps(uint32_t* steps);

imu_err_t imu_read_temperature(int* temp_scaled_100);

imu_err_t imu_read_fifo_buffer(uint8_t* out_buf, size_t *length);
imu_err_t imu_reset_fifo_buffer(void);

imu_err_t imu_setup_wake_on_motion(void);
imu_err_t imu_detect_motion(bool*);
imu_err_t imu_disable_wake_on_motion(void);
imu_err_t imu_setup_detect_no_motion(void);
imu_err_t imu_detect_no_motion(bool*);
imu_err_t imu_disable_detect_no_motion(void);

#endif /* IMU_DRIVER_H */
