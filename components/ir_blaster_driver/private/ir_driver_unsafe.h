#ifndef IR_DRIVER_UNSAFE_H
#define IR_DRIVER_UNSAFE_H

#include <ir_types.h>

void ir_blaster_driver_unsafe_init(void);
void ir_blaster_driver_unsafe_deinit(void);
bool ir_blaster_driver_unsafe_send_data(ir_blaster_data_t* data);

#endif /* IR_DRIVER_UNSAFE_H */
