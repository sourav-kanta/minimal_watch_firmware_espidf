#ifndef IMU_DRIVER_HAL_H
#define IMU_DRIVER_HAL_H

#include <imu_driver_private.h>
#include <imu_driver.h>

void esp32_imu_init(const imu_params_t*, qmi8658_bus_t*);
void esp32_imu_deinit(void);

#endif /* IMU_DRIVER_HAL_H */
