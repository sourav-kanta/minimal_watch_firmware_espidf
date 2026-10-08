#include <stage1_types.h>
#include <no_motion_detector.h>
#include <utils.h>
#include <math.h>
#include <esp_log.h>

static const char* TAG = "No Motion Detector";

bool stage1_detect_no_motion(stage1_ctx_t *stage1_ctx, const stage1_tuning_params_t *params,
                             float accel_unstability, float jerk_average) {
    float confidence_multiplier = 1.0f;
    float accel_stability_weight = 0.4f;
    float jerk_weight = 0.6f;
    if(stage1_ctx->no_motion_window_count < params->MIN_WINDOW_NO_MOTION_THRESHOLD) {
        stage1_ctx->no_motion_window_count++;
        confidence_multiplier = 0;
    }
    // Decreases prop to (excess)^4
    float accel_confidence = accel_unstability < params->MAX_ACCEL_UNSTABLE_THRESHOLD_DENSITY ?
                    1.0f/(1+accel_unstability) :
                    0.4f/squaref(squaref((1+accel_unstability - params->MAX_ACCEL_UNSTABLE_THRESHOLD_DENSITY)));
    // Decrease prop to (excess)^4
    float jerk_confidence = jerk_average < params->MAX_JERK_THRESHOLD_PER_SAMPLE ?
                    1.0f : 0.4f/squaref(squaref(1 + jerk_average - params->MAX_JERK_THRESHOLD_PER_SAMPLE));
    float confidence = accel_stability_weight * accel_confidence + jerk_weight * jerk_confidence;
    ESP_LOGD(TAG, "Accel confidence : %f", accel_confidence);
    ESP_LOGD(TAG, "Jerk confidence : %f", jerk_confidence);
    ESP_LOGD(TAG, "Confidence : %f", confidence);
    if(confidence < 0.8f) {
        stage1_ctx->no_motion_window_count = 0;
    }
    confidence *= confidence_multiplier;
    if(confidence >= 0.8f && confidence <= 0.9f) {
        // Discard this window and wait for next
        stage1_ctx->no_motion_window_count--;
        return false;
    }
    else if(confidence > 0.9f) {
        return true;
    }
    return false;
}
