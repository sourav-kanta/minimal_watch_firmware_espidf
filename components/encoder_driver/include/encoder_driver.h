#ifndef ENCODER_DRIVER_H
#define ENCODER_DRIVER_H

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

typedef enum {
    INPUT_LEFT,
    INPUT_RIGHT,
    INPUT_OK,
    INPUT_ESC,
    INPUT_INVALID
} input_key_t;

typedef enum {
    INPUT_STATE_PRESSED,
    INPUT_STATE_RELEASED,
    INPUT_STATE_INVALID
}input_key_state_t;

typedef struct {
    input_key_t key;
    input_key_state_t state;    
} encoder_input_t;

void encoder_init(QueueHandle_t);
void encoder_deinit(void);
QueueHandle_t encoder_get_queue(void);

#endif /* ENCODER_DRIVER_H */
