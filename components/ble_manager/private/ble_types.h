#ifndef BLE_TYPES_H
#define BLE_TYPES_H

#include <ble_consts.h>
#include <stdint.h>

typedef uint8_t ble_opcode_t;

#define BLE_OP_TIME_UPDATE          ((ble_opcode_t)0x01)
#define BLE_OP_WEATHER_UPDATE       ((ble_opcode_t)0x02)
#define BLE_OP_DATED_WEATHER_QUERY  ((ble_opcode_t)0x03)
#define BLE_OP_NOTIFICATION_SEND    ((ble_opcode_t)0x04)

typedef enum {
    BLE_TX,
    BLE_RX
} ble_comm_type_t;

typedef struct {
    ble_opcode_t opcode;
    uint8_t req_app;
    uint8_t len;
} ble_msg_hdr_t;

typedef struct {
    ble_msg_hdr_t hdr;
    uint8_t payload[BLE_MAX_PAYLOAD_SIZE];
} ble_msg_t;


#endif /* BLE_TYPES_H */
