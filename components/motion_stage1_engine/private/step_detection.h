#ifndef STEP_DETECTION_H
#define STEP_DETECTION_H

#include <stage1_types.h>
#include <stddef.h>
#include <stdint.h>

uint8_t stage1_detect_window_steps(stage1_ctx_t *stage1_ctx, const stage1_tuning_params_t *params,
                                          size_t samples);

uint8_t stage1_derive_accepted_steps_from_window(stage1_ctx_t *stage1_ctx, const stage1_tuning_params_t *params,
                                                 uint8_t window_steps, size_t samples);

#endif /* STEP_DETECTION_H */
