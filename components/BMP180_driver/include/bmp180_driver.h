#ifndef BMP180_DRIVER_H
#define BMP180_DRIVER_H

typedef enum {
    BMP180_SENSOR_MODE_ULTRA_LOW_POWER,
    BMP180_SENSOR_MODE_STANDARD,
    BMP180_SENSOR_MODE_HIGH_RES,
    BMP180_SENSOR_MODE_ULTRA_HIGH_RES,
    BMP180_SENSOR_MODE_INVALID
} bmp180_sensor_mode_t;

void bmp180_init(void);
void bmp180_deinit(void);
void bmp180_update_sea_level_pa(float pa);
bool bmp180_read_temp(int *temp_scaled_10);
bool bmp180_read_temp_and_altitude(int* temp_scaled_10, int *alt_meters_scaled_10, bmp180_sensor_mode_t mode);

#endif /* BMP180_DRIVER_H */
