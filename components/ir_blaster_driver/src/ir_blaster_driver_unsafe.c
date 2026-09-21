#include <ir_driver_unsafe.h>
#include <gpio_pins.h>
#include <driver/rmt_tx.h>
#include <esp_log.h>
#include <string.h>
#include <esp_err.h>
#include <esp_heap_caps.h>

static const char* TAG = "RMT driver";
static rmt_channel_handle_t tx_chan = NULL;

static inline uint32_t mark_space_to_symbol_val(uint16_t mark_duration, uint16_t space_duration) {
    uint32_t ret = ((uint32_t) mark_duration & 0x7FFF) |
                   ((uint32_t) 0x1 << 15) |
                   (((uint32_t)space_duration &0x7FFF) << 16) |
                   ((uint32_t) 0x0 << 31);
    return ret;
}

static size_t generate_rmt_symbol(const ir_blaster_data_t *data, rmt_symbol_word_t** out_symbols) {
    size_t payload_bit_count = data->payload_len * 8;
    // Header | Payload | Interframe gap | ....... | Header | Payload | Final Gap
    size_t symbol_count = (1+payload_bit_count) * data->burst_packets +
                          data->burst_packets;
    *out_symbols = heap_caps_malloc(
            symbol_count * sizeof(rmt_symbol_word_t), MALLOC_CAP_DMA);
    if(!*out_symbols) {
        ESP_LOGE(TAG, "Error allocating buffer for RMT symbols. Check MEM!");
        *out_symbols = NULL;
        return 0;
    }
    size_t symbol_idx = 0;
    for(int frame=0; frame<data->burst_packets; frame++) {
        (*out_symbols)[symbol_idx++] = (rmt_symbol_word_t) {
            .val = mark_space_to_symbol_val(data->header_mark, data->header_space),
        };
        for(size_t i=0; i<data->payload_len; i++) {
            for(size_t bit=0; bit<8; bit++) {
                uint8_t data_bit = data->payload_data[i] & (1 << bit);
                if(data_bit) {
                    (*out_symbols)[symbol_idx++] = (rmt_symbol_word_t) { 
                        .val = mark_space_to_symbol_val(data->bit1_mark, data->bit1_space),
                    };
                }
                else {
                    (*out_symbols)[symbol_idx++] = (rmt_symbol_word_t) { 
                        .val = mark_space_to_symbol_val(data->bit0_mark, data->bit0_space),
                    };
                }
            }
        }
        if(frame + 1 < data->burst_packets) {
            (*out_symbols)[symbol_idx++] = (rmt_symbol_word_t) {
                .val = mark_space_to_symbol_val(data->inter_frame_mark, data->inter_frame_space)
            };
        }
    }
    (*out_symbols)[symbol_idx++] = (rmt_symbol_word_t) {
        .val = mark_space_to_symbol_val(data->gap_mark, data->gap_space)
    };
    return symbol_count;
}

bool ir_blaster_driver_unsafe_send_data(ir_blaster_data_t* msg) {
    if(!tx_chan || !msg || !msg->payload_data || !msg->frequency) {
        return false;
    }
    rmt_symbol_word_t* symbols = NULL;
    size_t symbol_count = generate_rmt_symbol(msg, &symbols);
    if(!symbols) return false;
    rmt_copy_encoder_config_t copy_encoder = {};
    rmt_encoder_handle_t encoder_handle = NULL;
    ESP_ERROR_CHECK_WITHOUT_ABORT(rmt_new_copy_encoder(&copy_encoder, &encoder_handle));
    rmt_transmit_config_t transmit_config = {
        .loop_count = 0,
        .flags = {
            .eot_level = 0,
            .queue_nonblocking = 1, // Return if overloaded
        },
    };
    rmt_carrier_config_t carrier_config = {
        .duty_cycle = 0.5,
        .frequency_hz = msg->frequency,
        .flags = {
            .always_on = false,
            .polarity_active_low = false,
        },
    };
    ESP_ERROR_CHECK_WITHOUT_ABORT(rmt_apply_carrier(tx_chan, &carrier_config));
    ESP_ERROR_CHECK_WITHOUT_ABORT(rmt_enable(tx_chan));
    ESP_ERROR_CHECK_WITHOUT_ABORT(rmt_transmit(tx_chan, encoder_handle, symbols, 
                                               symbol_count*sizeof(rmt_symbol_word_t), &transmit_config));
    ESP_ERROR_CHECK_WITHOUT_ABORT(rmt_tx_wait_all_done(tx_chan, -1));
    ESP_ERROR_CHECK_WITHOUT_ABORT(rmt_del_encoder(encoder_handle));
    ESP_ERROR_CHECK_WITHOUT_ABORT(rmt_disable(tx_chan));
    ESP_ERROR_CHECK_WITHOUT_ABORT(rmt_apply_carrier(tx_chan, NULL));
    heap_caps_free(symbols);
    symbols = NULL;
    return true;
}

void ir_blaster_driver_unsafe_init(void) {
    rmt_tx_channel_config_t chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = IR_BLASTER_GATE_PIN,
        .mem_block_symbols = 64,
        .resolution_hz = 100*1000,
        .trans_queue_depth = 1,
        .flags = { 
            .invert_out = false, 
            .with_dma = true, 
            .allow_pd = false, 
            .init_level = 0
        },
    };

    ESP_ERROR_CHECK(rmt_new_tx_channel(&chan_config, &tx_chan));
}

void ir_blaster_driver_unsafe_deinit(void) {
    if(tx_chan) {
        // Defensive disable
        rmt_disable(tx_chan);
        ESP_ERROR_CHECK(rmt_del_channel(tx_chan));
        tx_chan = NULL;
    }
    else {
        ESP_LOGE(TAG, "RMT channel corrupted");
    }
}
