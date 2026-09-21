#ifndef IR_TYPES_H
#define IR_TYPES_H

#include <stdint.h>

typedef struct {
    uint8_t burst_packets;
    uint8_t payload_len;
    uint16_t header_mark;
    uint16_t header_space;
    uint16_t bit0_mark;
    uint16_t bit0_space;
    uint16_t bit1_mark;
    uint16_t bit1_space;
    uint16_t inter_frame_mark;
    uint16_t inter_frame_space;
    uint16_t gap_mark;
    uint16_t gap_space;
    uint32_t frequency;
    uint8_t* payload_data;
} ir_blaster_data_t;

#endif /* IR_TYPES_H */
