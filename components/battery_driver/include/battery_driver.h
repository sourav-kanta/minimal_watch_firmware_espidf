#ifndef BATTERY_DRIVER_H
#define BATTERY_DRIVER_H

#include <stdint.h>

void battery_driver_init(void);
uint8_t battery_driver_read_battery_percentage(void);
void battery_driver_deinit(void);

#endif /* BATTERY_DRIVER_H */
