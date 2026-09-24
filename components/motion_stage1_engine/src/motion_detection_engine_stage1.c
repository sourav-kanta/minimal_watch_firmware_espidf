#include <motion_detection_engine_stage1.h>
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

static bool stage1_detect_no_motion(float accel_unstability, float jerk_average) {
    float confidence_multiplier = 1.0f;
    float accel_stability_weight = 0.4f;
    float jerk_weight = 0.6f;
    if(stage1_ctx.no_motion_window_count < params.MIN_WINDOW_NO_MOTION_THRESHOLD) {
        stage1_ctx.no_motion_window_count++;
        confidence_multiplier = 0;
    }
    // Decreases prop to (excess)^4
    float accel_confidence = accel_unstability < params.MAX_ACCEL_UNSTABLE_THRESHOLD_DENSITY ? 
                    1.0f/(1+accel_unstability) : 
                    0.4f/squaref(squaref((1+accel_unstability - params.MAX_ACCEL_UNSTABLE_THRESHOLD_DENSITY)));
    // Decrease prop to (excess)^4
    float jerk_confidence = jerk_average < params.MAX_JERK_THRESHOLD_PER_SAMPLE ? 
                    1.0f : 0.4f/squaref(squaref(1 + jerk_average - params.MAX_JERK_THRESHOLD_PER_SAMPLE));
    float confidence = accel_stability_weight * accel_confidence + jerk_weight * jerk_confidence;
    ESP_LOGD(TAG, "Accel confidence : %f", accel_confidence);
    ESP_LOGD(TAG, "Jerk confidence : %f", jerk_confidence);
    ESP_LOGD(TAG, "Confidence : %f", confidence);
    if(confidence < 0.8f) {
        stage1_ctx.no_motion_window_count = 0;
    }
    confidence *= confidence_multiplier;
    if(confidence >= 0.8f && confidence <= 0.9f) {
        // Discard this window and wait for next
        stage1_ctx.no_motion_window_count--;
        return false;
    }
    else if(confidence > 0.9f) {
        return true;
    }
    return false;
}

static inline float calculate_peak_deviation(float peak_mean, float interval_mean, 
                                      float curr_peak, size_t current_peak_interval) {
    float peak_diff = curr_peak > peak_mean ? (curr_peak - peak_mean) : (peak_mean - curr_peak);
    float interval_diff = (float)current_peak_interval > interval_mean ? 
                          ((float)current_peak_interval - interval_mean) : 
                          (interval_mean - (float)current_peak_interval);
                          
    return 0.2f*(peak_diff / peak_mean) + 0.8f*(interval_diff / interval_mean);
}

static uint8_t stage1_detect_window_steps(size_t samples) {
    uint8_t window_steps = 0;
    uint8_t vertical_accel_samples = stage1_ctx.vertical_accel_mod_history.elements;
    if(vertical_accel_samples < params.MIN_SAMPLES_BEFORE_STEP_INFERENCE) {
        return 0;
    }

    ring_buffer_t *buffer = &stage1_ctx.vertical_accel_mod_history;
    float positive_sum = 0;
    int positive_count = 0;
    for(size_t i = 0; i < vertical_accel_samples; i++) {
        float *value = ring_buffer_pointer_at_index(buffer, i);
        assert(value);
        // Ignore the floor noise
        if(*value > 0.05f) {
            positive_sum += *value;
            positive_count++;
        }
    }
    float threshold = params.STEP_MIN_PROMINENCE_FOR_PEAK;
    if (positive_count > 0) {
        threshold = (positive_sum / positive_count) * 0.6f;
    } 
    ESP_LOGD(TAG, "Threshold : %f", threshold);

    size_t starting_index = buffer->elements <= samples ? 0 : buffer->elements - samples;
    if(starting_index > buffer->elements) {
        ESP_LOGE(TAG, "Unexpected sample underflow : Elems = %u : Window samples = %zu", 
                 buffer->elements, samples);
        return 0;
    }

    int last_peak_idx = -1;
    size_t peak_to_peak_sample_interval = 12;   // Start off with 12/21 = 0.57s
                                                // step intervals
    float peak_mean = 0;
    float peak_sample_interval_mean = 0;
    bool first_window_peak = true;
    int reference_peak_count = 0;
    float candidate_max = -10000.0f;
    int candidate_max_idx = -1;
    uint16_t samples_since_last_peak = 0;
    step_state_t state = STEP_STATE_FIND_PEAK;

    ESP_LOGD(TAG, "Detection executing");
    for(int i=0; i<buffer->elements; i++) {
        float* curr_val = ring_buffer_pointer_at_index(buffer, i);
        assert(curr_val);
        ESP_LOGD(TAG, "Current sample : %f", *curr_val);
        if(state == STEP_STATE_FIND_PEAK) {
            float prominence = *curr_val-threshold;
            if(*curr_val > candidate_max && 
               prominence > params.STEP_MIN_PROMINENCE_FOR_PEAK) {
                candidate_max = *curr_val;
                candidate_max_idx = i;
                samples_since_last_peak = 0;
            }
            else {
                samples_since_last_peak++;
                if(candidate_max_idx != -1 && 
                   candidate_max - *curr_val >= params.STEP_PEAK_HYSTERESIS_THRESHOLD) {
                    // Finalize peak, count step if in window, check cadence
                    // variance to verify step, switch state to find valley
                    if(last_peak_idx != -1) {
                        peak_to_peak_sample_interval = candidate_max_idx - last_peak_idx;
                    }
                    else {
                        peak_to_peak_sample_interval = params.STEP_PEAK_MEAN_SAMPLE_INTERVAL_GUESS;
                    }
                    last_peak_idx = candidate_max_idx;

                    if(i >= starting_index) {
                        if(first_window_peak) {
                            ESP_LOGD(TAG, "Extracted learning from %d historical steps", reference_peak_count);
                            if(reference_peak_count == 0) {
                                // Go with initial guess of 0.8g peaks and
                                // 0.57s intervals
                                peak_mean = params.STEP_PEAK_MEAN_GUESS;
                                peak_sample_interval_mean = params.STEP_PEAK_MEAN_SAMPLE_INTERVAL_GUESS; 
                            }
                            else {
                                peak_mean /= reference_peak_count;
                                peak_sample_interval_mean /= reference_peak_count;
                            }
                            first_window_peak = false;
                        }
                        ESP_LOGD(TAG, "Peak interval = %zu", peak_to_peak_sample_interval);
                        float peak_deviation = calculate_peak_deviation(peak_mean, 
                                                                        peak_sample_interval_mean,
                                                                        candidate_max,
                                                                        peak_to_peak_sample_interval);
                        if(peak_deviation < params.STEP_MAX_PEAK_DEVIATION && 
                            peak_to_peak_sample_interval >= params.MIN_SAMPLES_BETWEEN_STEPS &&
                            peak_to_peak_sample_interval <= params.MAX_SAMPLES_BETWEEN_STEPS) {
                            // Valid peak
                            ESP_LOGD(TAG, "STEP! Dev: %f | Interval: %zu | Target Mean: %f",
                                     peak_deviation, peak_to_peak_sample_interval, peak_sample_interval_mean);
                            window_steps++;
                            peak_mean = (peak_mean * reference_peak_count + candidate_max) / 
                                        (reference_peak_count + 1);
                            peak_sample_interval_mean = (peak_sample_interval_mean * reference_peak_count 
                                                        + peak_to_peak_sample_interval) / (reference_peak_count + 1);
                            reference_peak_count++;
                        }
                        else {
                            ESP_LOGD(TAG, "REJECTED! Dev: %f | Interval: %zu | Target Mean: %f",
                                     peak_deviation, peak_to_peak_sample_interval, 
                                     peak_sample_interval_mean);
                        }
                    }
                    else {
                        if(peak_to_peak_sample_interval >= params.MIN_SAMPLES_BETWEEN_STEPS && 
                           peak_to_peak_sample_interval <= params.MAX_SAMPLES_BETWEEN_STEPS) {
                            // Valid peak
                            peak_mean += candidate_max; 
                            peak_sample_interval_mean += peak_to_peak_sample_interval;
                            reference_peak_count++;
                        }
                    }
                    state = STEP_STATE_FIND_VALLEY;
                }
            }
        }
        else if(state == STEP_STATE_FIND_VALLEY) {
            samples_since_last_peak++;
            if(samples_since_last_peak > params.MAX_SAMPLES_BETWEEN_STEPS) {
                state = STEP_STATE_FIND_PEAK;
                candidate_max_idx = -1;
                candidate_max = *curr_val;
                last_peak_idx = -1;
                samples_since_last_peak = 0;
                ESP_LOGW(TAG, "WATCHDOG: Signal took too long to drop. Resetting state!");
                continue;
            }
            if(*curr_val < threshold) {
                // Reset state back to FIND_PEAK
                state = STEP_STATE_FIND_PEAK;
                candidate_max = *curr_val;
                candidate_max_idx = -1;
                samples_since_last_peak = 0;
            }
        }
    }
    
    return window_steps;
}

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
    out_res->no_motion = stage1_detect_no_motion(accel_unstability_avg, window_jerk_sum);
    uint8_t window_steps = stage1_detect_window_steps(samples);
    if (window_steps > 0) {
        stage1_ctx.samples_since_valid_step = 0;

        if (stage1_ctx.is_walking) {
            out_res->steps = window_steps;
        } else {
            stage1_ctx.pending_steps += window_steps;

            if (stage1_ctx.pending_steps >= params.STEP_MIN_ACCEPTABLE_CONSECUTIVE_STEPS) {
                stage1_ctx.is_walking = true;
                out_res->steps = stage1_ctx.pending_steps;
                stage1_ctx.pending_steps = 0;
            } else {
                out_res->steps = 0;
            }
        }
    } 
    else {
        stage1_ctx.samples_since_valid_step += samples;

        if (stage1_ctx.samples_since_valid_step > params.STEP_MAX_SAMPLES_BEFORE_STATE_CHANGE) {
            stage1_ctx.is_walking = false;
            stage1_ctx.pending_steps = 0;
        }
        out_res->steps = 0;
    }
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
