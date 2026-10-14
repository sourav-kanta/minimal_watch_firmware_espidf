#include <step_detection.h>
#include <math.h>
#include <stage1_types.h>
#include <stddef.h>
#include <assert.h>
#include <utils.h>
#include <esp_log.h>
#include <stdbool.h>

#define MIN_CORELATION_FOR_CONFIDENCE           0.7f
#define MAX_PERIODICTY_TIE_MARGIN               0.05f
#define PEAK_NEIGHBOURHOOD_COVERAGE_PCNT        45U

static const char *TAG = "Step Detection";

typedef struct {
    uint8_t index;
    float prominence;
} peak_candidate_t;

static float calculate_signal_mean(const ring_buffer_t* history,
                                   uint8_t start_idx, uint8_t end_idx) {
    float mean = 0.0f;

    assert(history);
    assert(history->elements > 0);
    assert(history->elem_size == sizeof(float));
    assert(start_idx < history->elements);
    assert(end_idx < history->elements);
    assert(start_idx < end_idx);

    for (size_t i = start_idx; i <= end_idx; i++) {
        mean += *(float*)ring_buffer_get_element_at_index(history, i);
    }

    return mean / (end_idx - start_idx + 1);
}

static float calculate_signal_variance(const ring_buffer_t* history,
                                       float mean,
                                       uint8_t start_idx,
                                       uint8_t end_idx) {
    float variance = 0.0f;

    assert(history);
    assert(history->elements > 0);
    assert(history->elem_size == sizeof(float));
    assert(start_idx < history->elements);
    assert(end_idx < history->elements);
    assert(start_idx < end_idx);

    for (size_t i = start_idx; i <= end_idx; i++) {
        float delta_signal =
            *(float*)ring_buffer_get_element_at_index(history, i) - mean;
        variance += delta_signal * delta_signal;
    }

    return variance / (end_idx - start_idx + 1);
}

static uint8_t calculate_signal_periodicity(const ring_buffer_t* history,
                                           uint8_t min_periodicity,
                                           uint8_t max_periodicity,
                                           bool *out_confident) {
    assert(max_periodicity > min_periodicity);

    *out_confident = false;

    size_t samples = history->elements;
    assert(samples > max_periodicity);

    float fallback_period =
        (float)(max_periodicity + min_periodicity) / 2.0f;

    size_t correlation_count =
        max_periodicity - min_periodicity + 1;

    float correlations[STAGE1_CORELATION_BUFFER_SIZE];

    for (size_t i = 0; i < correlation_count; i++) {
        correlations[i] = -2.0f;
    }

    float best_period = fallback_period;
    float best_correlation = -2.0f;
    float mean = calculate_signal_mean(
        history, 0, history->elements - 1U);

    ESP_LOGD(TAG,
             "[AC] samples=%zu mean=%.5fg lag_range=%u..%u threshold=%.3f",
             samples, mean,
             (unsigned)min_periodicity,
             (unsigned)max_periodicity,
             MIN_CORELATION_FOR_CONFIDENCE);

    for (size_t lag = min_periodicity;
         lag <= max_periodicity; lag++) {
        assert(samples > lag);

        size_t pair_boundary = samples - lag;
        float product_sum = 0.0f;
        float energy_a = 0.0f;
        float energy_b = 0.0f;

        for (size_t pair_first = 0;
             pair_first < pair_boundary; pair_first++) {
            float a =
                *(float*)ring_buffer_get_element_at_index(
                    history, pair_first) - mean;
            float b =
                *(float*)ring_buffer_get_element_at_index(
                    history, pair_first + lag) - mean;

            product_sum += a * b;
            energy_a += a * a;
            energy_b += b * b;
        }

        float denominator = sqrtf(energy_a * energy_b);

        if (denominator <= 1.0e-5f) {
            ESP_LOGD(TAG,
                     "[AC] lag=%zu skipped: denominator=%.7g pairs=%zu",
                     lag, denominator, pair_boundary);
            continue;
        }

        float correlation = product_sum / denominator;

        if (correlation > 1.0f) correlation = 1.0f;
        if (correlation < -1.0f) correlation = -1.0f;

        correlations[lag - min_periodicity] = correlation;

        ESP_LOGD(TAG, "[AC] lag=%zu corr=%+.4f pairs=%zu",
                 lag, correlation, pair_boundary);

        if (correlation > best_correlation) {
            best_correlation = correlation;
            best_period = (uint8_t)lag;
        }
    }

    ESP_LOGD(TAG, "[AC] raw best period=%.0f corr=%+.4f fallback=%.1f",
             best_period, best_correlation, fallback_period);

    // Weed out harmonics by selecting the first maxima within a threshold.
    if (best_correlation > MIN_CORELATION_FOR_CONFIDENCE) {
        for (size_t lag = min_periodicity;
             lag <= best_period; lag++) {
            float current = correlations[lag - min_periodicity];

            if (current <
                best_correlation - MAX_PERIODICTY_TIE_MARGIN) {
                continue;
            }

            float prev =
                lag > min_periodicity
                ? correlations[lag - min_periodicity - 1]
                : -2.0f;

            float next =
                lag < max_periodicity
                ? correlations[lag - min_periodicity + 1]
                : -2.0f;

            ESP_LOGD(TAG,
                     "[AC] harmonic check lag=%zu prev=%+.4f curr=%+.4f next=%+.4f",
                     lag, prev, current, next);

            if (current >= prev && current >= next) {
                best_period = (uint8_t)lag;
                break;
            }
        }

        *out_confident = true;
    }

    ESP_LOGD(TAG, "[AC] selected period=%.0f confidence=%s best_corr=%+.4f",
             best_period,
             *out_confident ? "YES" : "NO",
             best_correlation);

    return (uint8_t)best_period;
}

static inline uint8_t calculate_peak_radius(uint8_t period) {
    uint16_t radius =
        ((uint16_t)period * PEAK_NEIGHBOURHOOD_COVERAGE_PCNT + 50U)
        / 100U;
    return (uint8_t)radius;
}

static inline void calculate_peak_boundaries(
    const ring_buffer_t* history,
    uint8_t peak_idx,
    uint8_t peak_radius,
    uint8_t *out_left_boundary,
    uint8_t *out_right_boundary) {
    assert(history);
    assert(out_left_boundary);
    assert(out_right_boundary);
    assert(history->elements > 3U);

    *out_left_boundary =
        peak_idx > peak_radius ? peak_idx - peak_radius : 0U;

    *out_right_boundary =
        history->elements > peak_idx + peak_radius
        ? peak_idx + peak_radius
        : history->elements - 1U;
}

/**
 * @brief Calculates the prominence of a peak based on its difference from
 * max(left_min, right_min) within the peak radius Here a small number of
 * samples will skewer the prominence(let right boundary consist of only
 * 2 samples, then the right_min will be very high and calculated prominence
 *   becomes extremely low), so we only accept a side as baseline if side has
 *   at least peak radius elements in it, otherwise the other side wins by
 *   default
 *
 * @param history
 * @param peak_idx
 * @param peak_radius
 *
 * @return Calculated prominence
 */
static float calculate_peak_prominence(const ring_buffer_t* history,
                                       uint8_t peak_idx,
                                       uint8_t peak_radius) {
    assert(history->elements > 3U);

    uint8_t left_boundary, right_boundary;
    calculate_peak_boundaries(history, peak_idx, peak_radius,
                              &left_boundary, &right_boundary);

    float left_min = -INFINITY;
    float right_min = -INFINITY;
    bool has_left = peak_idx >= peak_radius;
    bool has_right = right_boundary - peak_idx >= peak_radius;

    if(has_left) {
        left_min = *(float*)ring_buffer_get_element_at_index(history, left_boundary);
        for (size_t i = left_boundary; i < peak_idx; i++) {
            float signal =
                *(float*)ring_buffer_get_element_at_index(history, i);
            if (signal < left_min) left_min = signal;
        }
    }

    if(has_right) {
        right_min = *(float*)ring_buffer_get_element_at_index(history, right_boundary);
        for (size_t i = peak_idx + 1U; i < right_boundary; i++) {
            float signal =
                *(float*)ring_buffer_get_element_at_index(history, i);
            if (signal < right_min) right_min = signal;
        }
    }

    float baseline_prominence =
        right_min > left_min ? right_min : left_min;
    float peak_prominence =
        *(float*)ring_buffer_get_element_at_index(history, peak_idx);

    if(!has_left && !has_right) {
        // Reject the peak
        ESP_LOGD(TAG, "[PEAK] peak=%u has no left or right elements"
                 "on either side, rejecting peak", peak_idx);
        baseline_prominence = peak_prominence;
    }

    return peak_prominence - baseline_prominence;
}

/**
 * @brief Check if a neighbouring peak exists for current candidate within the
 * acceptable step interval and calculates the deviation from the established
 * periodicity based on such a neighbor that matches this periodicty most
 *
 * @param candidates List of candidates
 * @param candidate_count Count of candidates
 * @param candidate_idx Index of currently checked candidate
 * @param min_interval Minimum acceptable step interval
 * @param max_interval Maximum acceptable step interval
 * @param periodicity Established periodicity
 * @param out_deviation Outputs the deviation from the neighbour most closely
 * matching the periodicity, example : say periodicity is 15 and we have
 * neighbors at intervals 10 18 24, then deviation is calculated based on
 * candidate with interval as 18
 *
 * @return True if a neighbor is found within acceptable step interval else
 * false
 */
static bool peak_has_rhythmic_neighbor(peak_candidate_t *candidates,
                                      uint8_t candidate_count,
                                      uint8_t candidate_idx,
                                      uint8_t min_interval,
                                      uint8_t max_interval,
                                      uint8_t periodicity,
                                      float *out_deviation) {
    uint8_t curr_idx = candidates[candidate_idx].index;
    bool found = false;
    unsigned int min_delta = UINT32_MAX;

    for (size_t i = 0U; i < candidate_count; i++) {
        if (i == candidate_idx) continue;

        uint8_t neighbor_peak_idx = candidates[i].index;
        uint8_t interval =
            curr_idx >= neighbor_peak_idx
            ? curr_idx - neighbor_peak_idx
            : neighbor_peak_idx - curr_idx;

        if (interval >= min_interval && interval <= max_interval) {
            ESP_LOGD(TAG,
                     "[PEAK] peak=%u rhythmic neighbor=%u interval=%u",
                     (unsigned)curr_idx,
                     (unsigned)neighbor_peak_idx,
                     (unsigned)interval);
            unsigned int delta = interval > periodicity ? interval - periodicity :
                                periodicity - interval;
            if(delta < min_delta) {
                min_delta = delta;
            }
            found = true;
        }
    }

    if(found && periodicity != 0) {
        *out_deviation = (float)min_delta / periodicity;
        return true;
    }
    return false;
}

/**
 * @brief Merge candidates that are too close to each otherbased on their
 * prominence, as second or third harmonic can generate a higher prominence
 * peak, which shouldnt be counted as a separate step
 *
 * @param candidates List of candidates
 * @param candidate_count Number of candidates
 * @param peak_idx Index of candidate
 * @param prominence Calculated prominence of candidate
 * @param min_interval Bound under which merging should happen
 *
 * @return Number of candidates after merge
 */
static uint8_t append_peak_candidate(peak_candidate_t* candidates,
                                     uint8_t candidate_count,
                                     uint8_t peak_idx,
                                     float prominence,
                                     uint8_t min_interval) {
    if (candidate_count > 0U) {
        peak_candidate_t *prev = &candidates[candidate_count - 1U];
        uint8_t interval = peak_idx - prev->index;

        if (interval < min_interval) {
            ESP_LOGD(TAG,
                     "[PEAK] merge index=%u prom=%.4fg with index=%u prom=%.4fg interval=%u",
                     (unsigned)peak_idx, prominence,
                     (unsigned)prev->index, prev->prominence,
                     (unsigned)interval);

            if (prominence > prev->prominence) {
                prev->index = peak_idx;
                prev->prominence = prominence;

                ESP_LOGD(TAG,
                         "[PEAK] replacement accepted index=%u prom=%.4fg",
                         (unsigned)peak_idx, prominence);
            }

            return candidate_count;
        }
    }

    candidates[candidate_count].index = peak_idx;
    candidates[candidate_count].prominence = prominence;
    candidate_count++;

    ESP_LOGD(TAG, "[PEAK] candidate index=%u prominence=%.4fg count=%u",
             (unsigned)peak_idx, prominence,
             (unsigned)candidate_count);

    return candidate_count;
}

uint8_t stage1_detect_window_steps(stage1_ctx_t *stage1_ctx,
                                   const stage1_tuning_params_t *params,
                                   size_t window_samples) {
    assert(stage1_ctx);
    assert(params);

    ESP_LOGD(TAG, "[RESULT] detection start window_samples=%zu",
             window_samples);

    if (window_samples == 0U) {
        ESP_LOGD(TAG, "[RESULT] skipped: empty window");
        return 0U;
    }

    const ring_buffer_t *history =
        &stage1_ctx->vertical_accel_mod_history;

    if (history->data == NULL ||
        history->elements < params->MIN_SAMPLES_BEFORE_STEP_INFERENCE) {
        ESP_LOGD(TAG,
                 "[RESULT] skipped: history=%u required=%u data=%s",
                 (unsigned)history->elements,
                 (unsigned)params->MIN_SAMPLES_BEFORE_STEP_INFERENCE,
                 history->data == NULL ? "NULL" : "valid");
        return 0U;
    }

    assert(history->elements < UINT8_MAX);
    assert(window_samples < history->elements);

    uint8_t detected_steps = 0U;
    bool periodicity_confidence = false;

    uint8_t period = calculate_signal_periodicity(
        history,
        params->MIN_SAMPLES_BETWEEN_STEPS,
        params->MAX_SAMPLES_BETWEEN_STEPS,
        &periodicity_confidence);

    uint8_t peak_radius = calculate_peak_radius(period);
    size_t samples = history->elements;

    ESP_LOGD(TAG,
             "[RESULT] history=%zu window=%zu boundary=%zu period=%u radius=%u min_prom=%.4fg min_var=%.6fg^2",
             samples, window_samples, samples - window_samples,
             (unsigned)period, (unsigned)peak_radius,
             params->STEP_MIN_PROMINENCE_FOR_PEAK,
             params->STEP_MIN_VARIANCE_ACTIVITY_FOR_STEP);

    peak_candidate_t candidate_peaks[STAGE1_CANDIDATE_BUFFER_SIZE];

    uint8_t candidate_count = 0U;

    for (size_t i = 1U; i < samples - 1U; i++) {
        float prev =
            *(float*)ring_buffer_get_element_at_index(history, i - 1U);
        float next =
            *(float*)ring_buffer_get_element_at_index(history, i + 1U);
        float curr =
            *(float*)ring_buffer_get_element_at_index(history, i);

        if (!(curr >= prev && curr >= next)) continue;

        float prominence =
            calculate_peak_prominence(history, (uint8_t)i, peak_radius);

        if (prominence < params->STEP_MIN_PROMINENCE_FOR_PEAK) {
            ESP_LOGD(TAG,
                     "[PEAK] reject index=%zu prominence=%.4fg < %.4fg",
                     i, prominence,
                     params->STEP_MIN_PROMINENCE_FOR_PEAK);
            continue;
        }

        candidate_count = append_peak_candidate(
            candidate_peaks, candidate_count,
            (uint8_t)i, prominence,
            params->MIN_SAMPLES_BETWEEN_STEPS);
    }

    ESP_LOGD(TAG, "[PEAK] candidates after merge=%u",
             (unsigned)candidate_count);

    if (candidate_count == 0U) {
        ESP_LOGD(TAG, "[RESULT] no candidates; steps=0");
        return 0U;
    }

    // Overlap the window with one historical sample so we dont loose the peak
    // that lies at the very end of a window.
    uint8_t window_boundary = history->elements - window_samples - 1;

    for (size_t i = 0U; i < candidate_count; i++) {
        uint8_t peak_idx = candidate_peaks[i].index;
        float peak_prominence = candidate_peaks[i].prominence;

        if (peak_idx < window_boundary) {
            ESP_LOGD(TAG,
                     "[FILTER] peak=%u ignored: outside window boundary=%u",
                     (unsigned)peak_idx, (unsigned)window_boundary);
            continue;
        }

        uint8_t left_boundary, right_boundary;
        calculate_peak_boundaries(history, peak_idx, peak_radius,
                                  &left_boundary, &right_boundary);

        float local_mean =
            calculate_signal_mean(history, left_boundary, right_boundary);
        float peak_variance =
            calculate_signal_variance(history, local_mean,
                                      left_boundary, right_boundary);

        // Subtle issue here, consider the case when we start from rest to
        // walking, assuming a peak energy measurement of 0.2, the mean lies
        // somewhere close to 0.1, and if the first peak coincidentaly
        // lies close to the end, variance will come out as low (sum [0.1^2
        // ] over only one half of radius / radius and we will loose this step
        if (peak_variance < params->STEP_MIN_VARIANCE_ACTIVITY_FOR_STEP) {
            ESP_LOGD(TAG,
                     "[FILTER] peak=%u rejected: variance=%.6fg^2 threshold=%.6fg^2",
                     (unsigned)peak_idx, peak_variance,
                     params->STEP_MIN_VARIANCE_ACTIVITY_FOR_STEP);
            continue;
        }

        float deviation = INFINITY;
        bool has_rhythmic_neighbor = peak_has_rhythmic_neighbor(
            candidate_peaks, candidate_count, (uint8_t)i,
            params->MIN_SAMPLES_BETWEEN_STEPS,
            params->MAX_SAMPLES_BETWEEN_STEPS,
            period, &deviation);

        bool clearly_prominent =
            peak_prominence >= params->STEP_MIN_PROMINENCE_FOR_PEAK * 1.5f;

        // Special case for not loosing the first step
        bool is_first_step = !stage1_ctx->is_walking &&
                             (stage1_ctx->pending_steps == 0) &&
                             (detected_steps == 0U) &&
                             clearly_prominent;

        // Initial few steps when periodicity hasnt been established, a rhthymic
        // motion that is clearly prominent is accepted as a step. This is
        // a source of false positives, we are depending on pending step gating
        // to prevent counting these dubious steps. However period motion just
        // after walking has been established still gives false positives
        bool supported_by_prominence =
            !periodicity_confidence &&
            has_rhythmic_neighbor &&
            clearly_prominent;

        // Once periodicity has stabilized, make the acceptance stricter and
        // allow only steps that deviate from the period by a small margin
        bool timing_supported =
            periodicity_confidence && has_rhythmic_neighbor &&
            deviation <= params->MAX_STEP_PERIODICITY_DEVIATION &&
            peak_prominence >= params->STEP_MIN_PROMINENCE_FOR_PEAK * 1.15f;

        ESP_LOGD(TAG,
                 "[FILTER] peak=%u prom=%.4fg variance=%.6fg^2 neighbor=%s prominent=%s"
                 " cadence=%s timing=%s first_step :%s fallback=%s deviation=%f",
                 (unsigned)peak_idx, peak_prominence, peak_variance,
                 has_rhythmic_neighbor ? "YES" : "NO",
                 clearly_prominent ? "YES" : "NO",
                 periodicity_confidence ? "YES" : "NO",
                 timing_supported ? "YES" : "NO",
                 is_first_step ? "YES" : "NO",
                 supported_by_prominence ? "YES" : "NO",
                 deviation);

        if (timing_supported || supported_by_prominence || is_first_step) {
            if (detected_steps < UINT8_MAX) {
                detected_steps++;
                ESP_LOGD(TAG, "[RESULT] ACCEPT peak=%u steps=%u",
                         (unsigned)peak_idx, (unsigned)detected_steps);
            }
        } else {
            ESP_LOGD(TAG,
                     "[FILTER] peak=%u rejected: acceptance conditions failed",
                     (unsigned)peak_idx);
        }
    }

    ESP_LOGD(TAG, "[RESULT] detection complete candidates=%u steps=%u",
             (unsigned)candidate_count, (unsigned)detected_steps);

    return detected_steps;
}

uint8_t stage1_derive_accepted_steps_from_window(
    stage1_ctx_t *stage1_ctx,
    const stage1_tuning_params_t* params,
    uint8_t window_steps,
    size_t samples) {
    assert(stage1_ctx);
    assert(params);

    ESP_LOGD(TAG,
             "[STATE] input window_steps=%u samples=%zu walking=%s pending=%u elapsed=%u",
             (unsigned)window_steps, samples,
             stage1_ctx->is_walking ? "YES" : "NO",
             (unsigned)stage1_ctx->pending_steps,
             (unsigned)stage1_ctx->samples_since_valid_step);

    if (window_steps == 0U) {
        uint32_t elapsed =
            (uint32_t)stage1_ctx->samples_since_valid_step + samples;

        stage1_ctx->samples_since_valid_step =
            (elapsed > UINT16_MAX) ? UINT16_MAX : (uint16_t)elapsed;

        if (stage1_ctx->samples_since_valid_step >=
            params->STEP_MAX_SAMPLES_BEFORE_STATE_CHANGE) {
            ESP_LOGD(TAG,
                     "[STATE] timeout elapsed=%u threshold=%u; clearing walking and pending",
                     (unsigned)stage1_ctx->samples_since_valid_step,
                     (unsigned)params->STEP_MAX_SAMPLES_BEFORE_STATE_CHANGE);

            stage1_ctx->is_walking = false;
            stage1_ctx->pending_steps = 0U;
        }

        ESP_LOGD(TAG,
                 "[STATE] no steps walking=%s pending=%u elapsed=%u",
                 stage1_ctx->is_walking ? "YES" : "NO",
                 (unsigned)stage1_ctx->pending_steps,
                 (unsigned)stage1_ctx->samples_since_valid_step);

        return 0U;
    }

    stage1_ctx->samples_since_valid_step = 0U;

    if (stage1_ctx->is_walking) {
        stage1_ctx->pending_steps = 0U;

        ESP_LOGD(TAG, "[STATE] already walking; returning %u steps",
                 (unsigned)window_steps);
        return window_steps;
    }

    uint16_t pending =
        (uint16_t)stage1_ctx->pending_steps + (uint16_t)window_steps;

    if (pending > UINT8_MAX) pending = UINT8_MAX;

    stage1_ctx->pending_steps = (uint8_t)pending;

    ESP_LOGD(TAG, "[STATE] pending=%u required=%u",
             (unsigned)stage1_ctx->pending_steps,
             (unsigned)params->STEP_MIN_ACCEPTABLE_CONSECUTIVE_STEPS);

    if (stage1_ctx->pending_steps >=
        params->STEP_MIN_ACCEPTABLE_CONSECUTIVE_STEPS) {
        uint8_t accepted_steps = stage1_ctx->pending_steps;

        stage1_ctx->is_walking = true;
        stage1_ctx->pending_steps = 0U;

        ESP_LOGD(TAG, "[STATE] WALKING confirmed; releasing %u steps",
                 (unsigned)accepted_steps);
        return accepted_steps;
    }

    ESP_LOGD(TAG, "[STATE] walking not confirmed");
    return 0U;
}
