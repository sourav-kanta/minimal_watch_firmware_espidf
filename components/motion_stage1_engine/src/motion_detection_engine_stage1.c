#include <motion_detection_engine_stage1.h>

#include <no_motion_detector.h>
#include <step_detection.h>
#include <stage1_types.h>
#include <esp_log.h>
#include <utils.h>
#include <stdbool.h>
#include <stddef.h>

#define SAMPLING_FREQ_ACCEL_ONLY                    21
#define SAMPLING_FREQ_ACCEL_GYRO                    56.05
#define NO_MOTION_WINDOW_THRESHOLD_SEC              10
#define STEP_LEARNING_HISTORY_SIZE                  SAMPLING_FREQ_ACCEL_ONLY*6

static const char* TAG = "Stage1 engine";
static stage1_ctx_t stage1_ctx;
static const stage1_tuning_params_t params = {
    .MAX_VERTICAL_ACCEL_HISTORY_SIZE = STEP_LEARNING_HISTORY_SIZE,
    .MIN_SAMPLES_BEFORE_STEP_INFERENCE = 10,
    .GRAVITY_ALPHA = 0.1f,
    .VERTICAL_ACCEL_ALPHA = 0.2f,
    .ACCEL_STABLE_THRESHOLD = 0.002f,
    .MAX_ACCEL_UNSTABLE_THRESHOLD_DENSITY = 0.005f,
    .MAX_JERK_THRESHOLD_PER_SAMPLE = 0.00005f,
    .MIN_WINDOW_NO_MOTION_THRESHOLD = 15,
    .MIN_SAMPLES_BETWEEN_STEPS = 12,
    .MAX_SAMPLES_BETWEEN_STEPS = 28,
    .STEP_MAX_PEAK_DEVIATION = 0.25f,
    .STEP_PEAK_HYSTERESIS_THRESHOLD = 0.05f,
    .STEP_MIN_PROMINENCE_FOR_PEAK = 0.07f,
    .STEP_PEAK_MEAN_GUESS = 0.2f,
    .STEP_PEAK_MEAN_SAMPLE_INTERVAL_GUESS = 16,
    .STEP_MIN_ACCEPTABLE_CONSECUTIVE_STEPS = 4,
    .STEP_MAX_SAMPLES_BEFORE_STATE_CHANGE = 42,
};


void stage1_process_motion_data(uint8_t* data, size_t samples, imu_stage1_result_t* out_res) {
    assert(out_res);
    assert(samples !=0);
    samples /= stage1_ctx.state == MOTION_MODE_ACCEL_ONLY ? 6 : 12;
    if(samples == 0) return;
    float window_jerk_sum = 0;
    float accel_unstability_avg = 0;
    float vertical_accel_mod = 0;
    // Data starts from index 1 and max samples configured 128
    // IMU on board is rotated -90 degree and soldered on the backside
    // so x=y y=-x (z is fine as imu measures normal)
    for(size_t j = 0; j < samples; j++) {
        if(stage1_ctx.state == MOTION_MODE_ACCEL_ONLY) {
            size_t base = j*6;

            uint8_t raw_y_l = data[base+0];
            uint8_t raw_y_h = data[base+1];
            uint8_t raw_x_l = data[base+2];
            uint8_t raw_x_h = data[base+3];
            uint8_t raw_z_l = data[base+4];
            uint8_t raw_z_h = data[base+5];

            int16_t raw_x = (int16_t)(((uint16_t)raw_y_l) | (((uint16_t)raw_y_h)<<8));
            int16_t raw_y = (-1) * ((int16_t)(((uint16_t)raw_x_l) | (((uint16_t)raw_x_h)<<8)));
            int16_t raw_z = (int16_t)(((uint16_t)raw_z_l) | (((uint16_t)raw_z_h)<<8));

            motion_vec3_t temp;
            motion_vec3_t curr_sample;
            sample_to_g(raw_x, raw_y, raw_z, &curr_sample);
            if(stage1_ctx.gravity_prev.x == 0 && stage1_ctx.gravity_prev.y == 0 && stage1_ctx.gravity_prev.z == 0) {
                stage1_ctx.gravity_prev = curr_sample;
            }
            motion_vec3_t relative_accel;
            vec_subtract(&curr_sample, &stage1_ctx.gravity_prev, &relative_accel);
            vec_scale(&relative_accel, params.GRAVITY_ALPHA, &temp);
            vec_add(&stage1_ctx.gravity_prev, &temp, &stage1_ctx.gravity_prev);
            float energy = vec_mod_square(&relative_accel);
            vec_subtract(&curr_sample, &stage1_ctx.accel_prev, &temp);
            window_jerk_sum += vec_mod_square(&temp);
            accel_unstability_avg += energy > params.ACCEL_STABLE_THRESHOLD ? 1 : 0;
            stage1_ctx.accel_prev = curr_sample;
            // Assuming |g| and |g|^2 are close enough
            vertical_accel_mod = vec_a_component_on_g(&curr_sample, &stage1_ctx.gravity_prev) - 1.0f;
            if(stage1_ctx.vertical_accel_mod_history.elements != 0) {
                // Smooth out the vertical_accel
                float* vertical_accel_prev = (float*) ring_buffer_pointer_at_index(
                                              &stage1_ctx.vertical_accel_mod_history,
                                              stage1_ctx.vertical_accel_mod_history.elements - 1);
                assert(vertical_accel_prev);
                vertical_accel_mod = params.VERTICAL_ACCEL_ALPHA * (*vertical_accel_prev) +
                                     (1.0f - params.VERTICAL_ACCEL_ALPHA) * vertical_accel_mod;
            }
            ring_buffer_insert(&stage1_ctx.vertical_accel_mod_history, &vertical_accel_mod);
        }
    }
    window_jerk_sum /= samples;
    accel_unstability_avg /= samples;
    out_res->no_motion = stage1_detect_no_motion(&stage1_ctx, &params, accel_unstability_avg, window_jerk_sum);
    uint8_t window_steps = stage1_detect_window_steps(&stage1_ctx, &params, samples);
    out_res->steps = stage1_derive_accepted_steps_from_window(&stage1_ctx, &params, window_steps, samples);
}

void stage1_init(void) {
    memset(&stage1_ctx, 0, sizeof(stage1_ctx_t));
    ring_buffer_init(params.MAX_VERTICAL_ACCEL_HISTORY_SIZE, sizeof(float),
                     &stage1_ctx.vertical_accel_mod_history);
}

void stage1_deinit(void) {
    ring_buffer_deinit(&stage1_ctx.vertical_accel_mod_history);
    memset(&stage1_ctx, 0, sizeof(stage1_ctx_t));
}
