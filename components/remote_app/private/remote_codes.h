#ifndef REMOTE_CODES_H
#define REMOTE_CODES_H

#include <stdint.h>

typedef struct {
    uint32_t frequency;
    uint16_t header_mark;
    uint16_t header_space;
    uint16_t mark;
    uint16_t bit0_space;
    uint16_t bit1_space;
    uint16_t inter_frame_mark;
    uint16_t inter_frame_space;
    uint16_t gap_mark;
    uint16_t gap_space;
    uint8_t bursts;
} remote_code_t;

static const remote_code_t bluestar_ac = {
    .frequency = 38*1000,
    .header_mark = 430,
    .header_space = 430,
    .bursts = 2,
    .mark = 53,
    .bit0_space = 53,
    .bit1_space = 158,
    .gap_mark = 55,
    .gap_space = 9640,
    .inter_frame_mark = 55,
    .inter_frame_space = 497,
};

static const uint8_t power_on_payload[] = { 0x4D, 0xB2, 0xFD, 0x02, 0x02, 0xFD };
static const uint8_t power_off_payload[] = { 0x4D, 0xB2, 0xDE, 0x21, 0x07, 0xF8 };
static const uint8_t temp_16_payload[] = { 0x4D, 0xB2, 0xFD, 0x02, 0x00, 0xFF }; 
static const uint8_t temp_17_payload[] = { 0x4D, 0xB2, 0xFD, 0x02, 0x00, 0xFF };
static const uint8_t temp_18_payload[] = { 0x4D, 0xB2, 0xFD, 0x02, 0x08, 0xF7 };
static const uint8_t temp_19_payload[] = { 0x4D, 0xB2, 0xFD, 0x02, 0x0C, 0xF3 }; 
static const uint8_t temp_20_payload[] = { 0x4D, 0xB2, 0xFD, 0x02, 0x04, 0xFB };
static const uint8_t temp_21_payload[] = { 0x4D, 0xB2, 0xFD, 0x02, 0x06, 0xF9 };
static const uint8_t temp_22_payload[] = { 0x4D, 0xB2, 0xFD, 0x02, 0x0E, 0xF1 };
static const uint8_t temp_23_payload[] = { 0x4D, 0xB2, 0xFD, 0x02, 0x0A, 0xF5 };
static const uint8_t temp_24_payload[] = { 0x4D, 0xB2, 0xFD, 0x02, 0x02, 0xFD };
static const uint8_t temp_25_payload[] = { 0x4D, 0xB2, 0xFD, 0x02, 0x03, 0xFC };
static const uint8_t temp_26_payload[] = { 0x4D, 0xB2, 0xFD, 0x02, 0x0B, 0xF4 };
static const uint8_t temp_27_payload[] = { 0x4D, 0xB2, 0xFD, 0x02, 0x09, 0xF6 };
static const uint8_t temp_28_payload[] = { 0x4D, 0xB2, 0xFD, 0x02, 0x01, 0xFE };
static const uint8_t temp_29_payload[] = { 0x4D, 0xB2, 0xFD, 0x02, 0x05, 0xFA };
static const uint8_t temp_30_payload[] = { 0x4D, 0xB2, 0xFD, 0x02, 0x0D, 0xF2 };

static const uint8_t* const temp_payloads[] = {
    temp_16_payload, 
    temp_17_payload,
    temp_18_payload,
    temp_19_payload,
    temp_20_payload,
    temp_21_payload,
    temp_22_payload,
    temp_23_payload,
    temp_24_payload,
    temp_25_payload,
    temp_26_payload,
    temp_27_payload,
    temp_28_payload,
    temp_29_payload,
    temp_30_payload,
};

#endif /* REMOTE_CODES_H */
