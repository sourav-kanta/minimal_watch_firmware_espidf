#ifndef MOTION_DETECTION_ENGINE_STAGE1_H
#define MOTION_DETECTION_ENGINE_STAGE1_H

#include <motion_types.h>
#include <stdint.h>

void stage1_process_motion_data(uint8_t *data, size_t samples, imu_stage1_result_t* out_res);
void stage1_init(void);
void stage1_deinit(void);

#endif /* MOTION_DETECTION_ENGINE_STAGE1_H */
