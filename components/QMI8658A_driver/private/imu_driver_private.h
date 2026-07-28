#ifndef IMU_DRIVER_PRIVATE_H
#define IMU_DRIVER_PRIVATE_H

#include <stddef.h>
#include <stdint.h>
#include <imu_driver.h>

typedef enum {
    IMU_LOW_POWER,
    IMU_ACCEL_ONLY,
    IMU_GYRO_ONLY,
    IMU_ACCEL_AND_GYRO,
    IMU_NO_POWER     
} imu_state_t;


typedef imu_err_t (*qmi_read_fptr_t) (void *intf_ptr, uint8_t reg_addr, uint8_t *data, size_t len);
typedef imu_err_t (*qmi_read_fifo_fptr_t) (void *intf_ptr, uint8_t reg_addr, uint8_t *data, size_t len);
typedef imu_err_t (*qmi_write_fptr_t)(void *intf_ptr, uint8_t reg_addr, const uint8_t *data, size_t len);

typedef struct {
    qmi_read_fptr_t read;
    qmi_read_fifo_fptr_t read_fifo;
    qmi_write_fptr_t write;
    void *intf_ptr;
} qmi8658_bus_t;

typedef struct {
    // MSB for read(1) write(0) followed by 7 bit reg address
    uint8_t reg_addr;
    uint8_t data;
    uint16_t delay_ms;
} qmi8658_cmd_t;

#endif /* IMU_DRIVER_PRIVATE_H */
