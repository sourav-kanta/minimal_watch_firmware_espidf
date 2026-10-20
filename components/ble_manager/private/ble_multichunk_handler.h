#ifndef BLE_MULTICHUNK_HANDLER_H
#define BLE_MULTICHUNK_HANDLER_H

#include <ble_types.h>
#include <stdint.h>

typedef void (*response_handler_cb_t) (const ble_multichunk_msg_t* msg);

void parse_new_chunk(const ble_msg_t* chunk, response_handler_cb_t handler);

#endif /* BLE_MULTICHUNK_HANDLER_H */
