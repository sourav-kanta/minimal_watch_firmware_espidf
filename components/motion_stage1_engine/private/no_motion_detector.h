#ifndef NO_MOTION_DETECTOR_H
#define NO_MOTION_DETECTOR_H

#include <stage1_types.h>

bool stage1_detect_no_motion(stage1_ctx_t *stage1_ctx, const stage1_tuning_params_t *params,
                             float accel_unstability, float jerk_average);

#endif /* NO_MOTION_DETECTOR_H */
