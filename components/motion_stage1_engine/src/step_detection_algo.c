#include <step_detection.h>
#include <stage1_types.h>
#include <utils.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
#include <esp_log.h>

static const char* TAG = "Step Detector";

static inline float calculate_peak_deviation(float peak_mean, float interval_mean,
                                      float curr_peak, size_t current_peak_interval) {
    float peak_diff = curr_peak > peak_mean ? (curr_peak - peak_mean) : (peak_mean - curr_peak);
    float interval_diff = (float)current_peak_interval > interval_mean ?
                          ((float)current_peak_interval - interval_mean) :
                          (interval_mean - (float)current_peak_interval);

    return 0.2f*(peak_diff / peak_mean) + 0.8f*(interval_diff / interval_mean);
}

uint8_t stage1_detect_window_steps(stage1_ctx_t *stage1_ctx, const stage1_tuning_params_t *params,
                                          size_t samples) {
    uint8_t window_steps = 0;
    uint8_t vertical_accel_samples = stage1_ctx->vertical_accel_mod_history.elements;
    if(vertical_accel_samples < params->MIN_SAMPLES_BEFORE_STEP_INFERENCE) {
        return 0;
    }

    ring_buffer_t *buffer = &stage1_ctx->vertical_accel_mod_history;
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
    float threshold = params->STEP_MIN_PROMINENCE_FOR_PEAK;
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
               prominence > params->STEP_MIN_PROMINENCE_FOR_PEAK) {
                candidate_max = *curr_val;
                candidate_max_idx = i;
                samples_since_last_peak = 0;
            }
            else {
                samples_since_last_peak++;
                if(candidate_max_idx != -1 &&
                   candidate_max - *curr_val >= params->STEP_PEAK_HYSTERESIS_THRESHOLD) {
                    // Finalize peak, count step if in window, check cadence
                    // variance to verify step, switch state to find valley
                    if(last_peak_idx != -1) {
                        peak_to_peak_sample_interval = candidate_max_idx - last_peak_idx;
                    }
                    else {
                        peak_to_peak_sample_interval = params->STEP_PEAK_MEAN_SAMPLE_INTERVAL_GUESS;
                    }
                    last_peak_idx = candidate_max_idx;

                    if(i >= starting_index) {
                        if(first_window_peak) {
                            ESP_LOGD(TAG, "Extracted learning from %d historical steps", reference_peak_count);
                            if(reference_peak_count == 0) {
                                // Go with initial guess of 0.8g peaks and
                                // 0.57s intervals
                                peak_mean = params->STEP_PEAK_MEAN_GUESS;
                                peak_sample_interval_mean = params->STEP_PEAK_MEAN_SAMPLE_INTERVAL_GUESS;
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
                        if(peak_deviation < params->STEP_MAX_PEAK_DEVIATION &&
                            peak_to_peak_sample_interval >= params->MIN_SAMPLES_BETWEEN_STEPS &&
                            peak_to_peak_sample_interval <= params->MAX_SAMPLES_BETWEEN_STEPS) {
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
                        if(peak_to_peak_sample_interval >= params->MIN_SAMPLES_BETWEEN_STEPS &&
                           peak_to_peak_sample_interval <= params->MAX_SAMPLES_BETWEEN_STEPS) {
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
            if(samples_since_last_peak > params->MAX_SAMPLES_BETWEEN_STEPS) {
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

uint8_t stage1_derive_accepted_steps_from_window(stage1_ctx_t *stage1_ctx, const stage1_tuning_params_t *params,
                                                 uint8_t window_steps, size_t samples) {
    if (window_steps > 0) {
        stage1_ctx->samples_since_valid_step = 0;

        if (stage1_ctx->is_walking) {
            return window_steps;
        } else {
            stage1_ctx->pending_steps += window_steps;

            if (stage1_ctx->pending_steps >= params->STEP_MIN_ACCEPTABLE_CONSECUTIVE_STEPS) {
                uint8_t accepted_steps = stage1_ctx->pending_steps;
                stage1_ctx->is_walking = true;
                stage1_ctx->pending_steps = 0;
                return accepted_steps;
            } else {
                return 0;
            }
        }
    }
    else {
        stage1_ctx->samples_since_valid_step += samples;

        if (stage1_ctx->samples_since_valid_step > params->STEP_MAX_SAMPLES_BEFORE_STATE_CHANGE) {
            stage1_ctx->is_walking = false;
            stage1_ctx->pending_steps = 0;
        }
        return 0;
    }
}
