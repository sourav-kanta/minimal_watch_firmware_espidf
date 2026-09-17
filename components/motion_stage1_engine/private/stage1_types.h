#ifndef STAGE1_TYPES_H
#define STAGE1_TYPES_H

#include <motion_types.h>

#define STAGE1_SAMPLE_RATE_HZ                 21.0f

typedef struct {
    void* data;
    uint8_t elem_size;
    uint8_t max_size;
    uint8_t elements;
    uint8_t read_idx;
    uint8_t write_idx;
} ring_buffer_t;

typedef enum {
    STEP_STATE_FIND_PEAK,
    STEP_STATE_FIND_VALLEY,
    STEP_STATE_INVALID
} step_state_t;

typedef struct {
    motion_state_t state;
    motion_vec3_t accel_prev;
    motion_vec3_t gravity_prev;
    ring_buffer_t vertical_accel_mod_history;
    uint8_t no_motion_window_count;
    uint8_t pending_steps;
    uint16_t samples_since_valid_step;
    bool is_walking;
} stage1_ctx_t;

typedef struct {
    uint8_t MAX_VERTICAL_ACCEL_HISTORY_SIZE;
    uint8_t MIN_SAMPLES_BEFORE_STEP_INFERENCE;
    uint8_t MIN_SAMPLES_BETWEEN_STEPS;
    uint8_t MAX_SAMPLES_BETWEEN_STEPS;
    uint8_t STEP_PEAK_MEAN_SAMPLE_INTERVAL_GUESS;
    float STEP_PEAK_MEAN_GUESS;
    float STEP_MIN_PROMINENCE_FOR_PEAK;
    float STEP_MAX_PEAK_DEVIATION;
    float STEP_PEAK_HYSTERESIS_THRESHOLD;
    float VERTICAL_ACCEL_ALPHA;
    float GRAVITY_ALPHA;
    float ACCEL_STABLE_THRESHOLD;
    float MAX_ACCEL_UNSTABLE_THRESHOLD_DENSITY;
    float MAX_JERK_THRESHOLD_PER_SAMPLE;
    int MIN_WINDOW_NO_MOTION_THRESHOLD;
    int STEP_MIN_ACCEPTABLE_CONSECUTIVE_STEPS;
    int STEP_MAX_SAMPLES_BEFORE_STATE_CHANGE;
} stage1_tuning_params_t;

#endif /* STAGE1_TYPES_H */
