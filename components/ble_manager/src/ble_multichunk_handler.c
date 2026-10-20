#include <ble_multichunk_handler.h>
#include <ble_types.h>
#include <stdint.h>
#include <string.h>
#include <esp_log.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <esp_rom_crc.h>

static const char* TAG = "Multichunk parser";

typedef enum {
    PARSER_STATE_READ_HEADER,
    PARSER_STATE_READ_CHUNKS,
    PARSER_STATE_READ_CRC,
    PARSER_STATE_INVALID
} parser_state_t;

typedef struct {
    ble_opcode_t opcode;
    uint8_t total_chunks;
    uint16_t total_size;
} multichunk_header_t;

typedef struct {
    parser_state_t state;
    multichunk_header_t hdr;
    uint8_t curr_chunk_id;
    uint8_t req_app;
    uint16_t read_size;
    uint32_t crc;
    uint8_t* out_data;
} multichunk_parser_t;

static multichunk_parser_t parser;

static void reset_multichunk_parser(void) {
    if(parser.out_data) {
        free(parser.out_data);
    }
    memset(&parser, 0, sizeof(parser));
}

/*
 * Format for multichunk packet payloads
 *
 * [MAGIC] | [OPCODE] | [CHUNKS] | [TOTAL_SIZE]
 *
 * [DATA] -> [CHUNK_ID] [CHUNK_1] [CHUNK_ID] [CHUNK2] .....
 *
 * [CRC]
 *
 * Widths :
 * MAGIC -> 1 byte
 * OPCODE -> 1 byte
 * CHUNKS -> 1 byte
 * TOTAL_SIZE -> 2 bytes (bounded by 2048)
 * DATA-> TOTAL_SIZE bytes
 * CRC -> 4 bytes
 *
*/
static bool read_multichunk_header(multichunk_parser_t* parser, const ble_msg_t* msg) {
   assert(parser->state == PARSER_STATE_READ_HEADER);
   uint8_t offset = 0;
   if(msg->hdr.len <= 6 || msg->hdr.len > BLE_MAX_PAYLOAD_SIZE) {
       // Cant hold a valid header + chunk id
       return false;
   }
   if(msg->payload[offset++] != BLE_MULTICHUNK_MAGIC) {
       // Invalid multichunk start
       return false;
   }
   parser->hdr.opcode = msg->payload[offset++];
   parser->hdr.total_chunks = msg->payload[offset++];
   parser->hdr.total_size = (uint16_t) (msg->payload[offset]<<8) |
                            (uint16_t) msg->payload[offset+1];
   if(parser->hdr.total_chunks == 0 || parser->hdr.total_size == 0)
       return false;
   if(parser->hdr.total_size > BLE_MAX_MULTICHUNK_SIZE) return false;
   return true;
}

void parse_new_chunk(const ble_msg_t* chunk, response_handler_cb_t handler) {
    if(chunk == NULL) {
        reset_multichunk_parser();
        return;
    }
    assert(handler != NULL);
    uint8_t chunk_length = chunk->hdr.len;
    uint8_t chunk_offset = 0;
    if(parser.state == PARSER_STATE_READ_HEADER) {
        if(!read_multichunk_header(&parser, chunk)){
            ESP_LOGE(TAG, "Invalid multichunk start, rejecting!");
            // Drop the current message
            reset_multichunk_parser();
            return;
        }
        else {
            if(parser.out_data) {
                ESP_LOGE(TAG, "Non null parser data, attempting free");
                free(parser.out_data);
            }
            parser.req_app = chunk->hdr.req_app;
            parser.state = PARSER_STATE_READ_CHUNKS;
            parser.out_data = (uint8_t*) malloc(parser.hdr.total_size);
            if(!parser.out_data) {
                ESP_LOGE(TAG, "Unable to allocate mem for message");
                reset_multichunk_parser();
                return;
            }
        }
    }
    if(parser.state == PARSER_STATE_READ_CHUNKS) {
        if(parser.curr_chunk_id == 0) {
            // Chunks start after 5 bytes of header
            chunk_offset = 5;
            chunk_length -= chunk_offset + 1;
            if(chunk->hdr.len <= 6) {
                // Too short to be a valid chunk
                reset_multichunk_parser();
                return;
            }
        }
        else {
            // For chunk id
            chunk_length -= 1;
            if(chunk->hdr.len <= 1) {
                // Too short to be a valid chunk
                reset_multichunk_parser();
                return;
            }
        }
        if (parser.read_size > parser.hdr.total_size) {
            reset_multichunk_parser();
            return;
        }
        chunk_length = parser.hdr.total_size - parser.read_size < chunk_length ?
                       parser.hdr.total_size - parser.read_size :
                       chunk_length;
        if(chunk->payload[chunk_offset++] != parser.curr_chunk_id) {
            // Invalid chunk drop message
            reset_multichunk_parser();
            return;
        }
        memcpy(&parser.out_data[parser.read_size],
               &chunk->payload[chunk_offset],
               chunk_length);
        chunk_offset += chunk_length;
        parser.read_size += chunk_length;
        parser.curr_chunk_id++;
        if (parser.read_size == parser.hdr.total_size &&
            parser.curr_chunk_id != parser.hdr.total_chunks) {
            // Data consumed but chunks mismatch
            reset_multichunk_parser();
            return;
        }
        if (parser.curr_chunk_id == parser.hdr.total_chunks &&
            parser.read_size != parser.hdr.total_size) {
            // Chunks received but data size mismatch
            reset_multichunk_parser();
            return;
        }
        if(parser.curr_chunk_id == parser.hdr.total_chunks &&
           parser.read_size == parser.hdr.total_size) {
            // Finished message
            parser.state = PARSER_STATE_READ_CRC;
            parser.read_size = 0;
        }
    }
    if(parser.state == PARSER_STATE_READ_CRC) {
        // Even if crc is split across messages we should be able
        // to recover it
        while(parser.read_size != 4 && chunk_offset < chunk->hdr.len) {
            parser.crc |= (uint32_t)chunk->payload[chunk_offset++] <<
                          (3U-parser.read_size) * 8U;
            parser.read_size++;
        }
        if(parser.read_size == 4) {
            if (chunk_offset != chunk->hdr.len) {
                reset_multichunk_parser();
                return;
            }
            // Check crc and reset on mismatch
            uint32_t calc_crc = esp_rom_crc32_le(0, parser.out_data, parser.hdr.total_size);
            if(calc_crc != parser.crc) {
                // CRC mismatch drop current multichunk and messgae
                reset_multichunk_parser();
                return;
            }
            // Call response handler and pass out_data
            // its no longer our responsibility, user has
            // to free after use
            ble_multichunk_msg_t msg = {
                .opcode = parser.hdr.opcode,
                .req_app = parser.req_app,
                .len = parser.hdr.total_size,
                .data = parser.out_data,
            };
            handler(&msg);
            parser.out_data = NULL;
            reset_multichunk_parser();
            return;
        }
    }
}
