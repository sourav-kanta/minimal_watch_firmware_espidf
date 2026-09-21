#ifndef IR_BLASTER_DRIVER_H
#define IR_BLASTER_DRIVER_H

#include <ir_types.h>

void ir_blaster_driver_init(void);
void ir_blaster_driver_deinit(void);
bool ir_blaster_driver_send_data(ir_blaster_data_t* data);

#endif /* IR_BLASTER_DRIVER_H */
