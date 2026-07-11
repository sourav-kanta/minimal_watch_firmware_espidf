#ifndef BLE_FIFO_H
#define BLE_FIFO_H

#include <ble_types.h>

bool ble_fifo_init(void);
bool add_ble_msg_to_queue(const ble_msg_t*, const ble_comm_type_t);
bool get_next_ble_msg(ble_msg_t*, const ble_comm_type_t);
bool ble_fifo_deinit(void);

#endif /* BLE_FIFO_H */
