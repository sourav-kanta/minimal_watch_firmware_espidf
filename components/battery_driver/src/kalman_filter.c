#include <kalman_filter.h>

#define MIN_SAMPLE_FOR_KALMAN_FILTER    5

int kalman_filter_update(kalman_state_t *state, int measurement_mv) {
    if (!state) return measurement_mv;

    if (state->sample_count < MIN_SAMPLE_FOR_KALMAN_FILTER) {
        state->est_mv = (float)measurement_mv;
    } else {
        state->error_est = state->error_est + state->q;
        float kalman_gain = state->error_est / (state->error_est + state->error_meas);
        state->est_mv = state->est_mv + kalman_gain * ((float)measurement_mv - state->est_mv);
        state->error_est = (1.0f - kalman_gain) * state->error_est;
    }

    state->sample_count++;
    return (int)state->est_mv;
}

void kalman_filter_init(kalman_state_t *state, float error_meas, float q) {
    if (!state) return;
    state->est_mv = 0.0f;
    state->error_est = 1.0f;
    state->error_meas = error_meas;
    state->q = q;
    state->sample_count = 0;
}
