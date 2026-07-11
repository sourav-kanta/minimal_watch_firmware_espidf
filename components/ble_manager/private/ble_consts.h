#ifndef BLE_CONSTS_H
#define BLE_CONSTS_H

#include <stdint.h>

#define BLE_MAX_PAYLOAD_SIZE            250
#define BLE_FIFO_SIZE                   20

#define BLE_MAGIC                       0xA5
#define BLE_HDR_SIZE                    3
#define BLE_MIN_CONN_INTERVAL           0x0320
#define BLE_MAX_CONN_INTERVAL           0x0348
#define BLE_MIN_ADV_INTERVAL            0x0640
#define BLE_MAX_ADV_INTERVAL            0x0648
#define BLE_CONN_LATENCY                0x0
#define BLE_CONN_SUPERVISION_TIMEOUT    0x0190 

#define BLE_MSG_TIME_SIZE               4 
#define BLE_MSG_WEATHER_SIZE            148 
#define BLE_MSG_MIN_NOTIFICATION_SIZE   5 

// Service UUID
#define BLE_UUID_SMARTWATCH_SERVICE \
    0xF0, 0xDE, 0xBC, 0x9A, \
    0x78, 0x56, \
    0x34, 0x12, \
    0x78, 0x56, \
    0x34, 0x12, 0x78, 0x56, 0x34, 0x12

// RX UUID
#define BLE_UUID_RX_CHRC \
    0x87, 0x09, 0x21, 0x43, \
    0x65, 0x87, \
    0x21, 0x43, \
    0x65, 0x87, \
    0x21, 0x43, 0x21, 0x43, 0x65, 0x87

// TX UUID
#define BLE_UUID_TX_CHRC \
    0x88, 0x09, 0x21, 0x43, \
    0x65, 0x87, \
    0x21, 0x43, \
    0x65, 0x87, \
    0x21, 0x43, 0x21, 0x43, 0x65, 0x87

#endif /* BLE_CONSTS_H */
