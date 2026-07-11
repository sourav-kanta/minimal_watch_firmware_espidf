#include <ble_types.h>
#include <common_types.h>
#include <esp_log.h>
#include <string.h>
#include <ble_fifo.h>

#include <ble_request_handler.h>

static const char* TAG = "Request Handler";

bool handle_ble_request(const ble_req_t *req) {
    if(!req) return false;
    bool success = false;
    switch(req->req_code) {
        case UPDATE_SYSTEM_TIME : {
            ble_msg_t new_msg = {
                .hdr = {
                    .opcode = BLE_OP_TIME_UPDATE,
                    .len = 0,
                    .req_app = req->app_id
                }
            };
            success = add_ble_msg_to_queue(&new_msg, BLE_TX);
            break;
        }
        case UPDATE_SYSTEM_WEATHER : {
            ble_msg_t new_msg = {
                .hdr = {
                    .opcode = BLE_OP_WEATHER_UPDATE,
                    .len = 0,
                    .req_app = req->app_id
                }
            };
            success = add_ble_msg_to_queue(&new_msg, BLE_TX);
            break;
        }
        case DATED_WEATHER_QUERY : {
            ble_msg_t new_msg = {
                .hdr = {
                    .opcode = BLE_OP_DATED_WEATHER_QUERY,
                    .len = req->req_data_len,
                    .req_app = req->app_id
                },
            };
            if(req->req_data_len <= BLE_MAX_PAYLOAD_SIZE) {
                memcpy(new_msg.payload, req->req_data, req->req_data_len);
                success = add_ble_msg_to_queue(&new_msg, BLE_TX);
            }
            else {
                ESP_LOGE(TAG, "Invalid payload length %u", req->req_data_len);
                success = false;
            }
            break;
        }
        default :
            success = false;
            ESP_LOGW(TAG, "Unknown BLE request: %d", req->req_code);
            break;
    }
    return success;
}

