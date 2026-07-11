#ifndef BLE_PACKET_HANDLER_H
#define BLE_PACKET_HANDLER_H

#include <stdint.h>

bool ble_build_next_packet(uint8_t*, uint16_t, uint16_t*);
bool dump_ble_packet(const uint8_t*, uint16_t);
void ble_retrieve_packet(const uint8_t*, uint16_t);
void rx_reset_context(void);
void tx_reset_context(void);


#endif /* BLE_PACKET_HANDLER_H */
