#ifndef BMP180_INTERNAL_H
#define BMP180_INTERNAL_H

#include <stdint.h>

typedef bool (*i2c_read_register_t)(void* intf, uint8_t reg, uint8_t* data, size_t size);
typedef bool (*i2c_write_register_t)(void* intf, uint8_t reg, const uint8_t* data, size_t size);

typedef struct {
    void *handle;
    void *bus_handle;
    i2c_read_register_t read_reg;
    i2c_write_register_t write_reg;
} bmp180_bus_t;

void bmp180_configure_i2c(bmp180_bus_t* intf);
void bmp180_release_i2c(bmp180_bus_t* intf);

#endif /* BMP180_INTERNAL_H */
