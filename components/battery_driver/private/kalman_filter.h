#ifndef KALMAN_FILTER_H
#define KALMAN_FILTER_H

#include <stdint.h>

typedef struct {
    float est_mv;
    float error_est;
    float error_meas;
    float q;
    uint32_t sample_count;
} kalman_state_t;

void kalman_filter_init(kalman_state_t *state, float error_meas, float q);
int kalman_filter_update(kalman_state_t *state, int measurement_mv);

#endif /* KALMAN_FILTER_H */
