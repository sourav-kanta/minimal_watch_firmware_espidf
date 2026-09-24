#ifndef MOTION_TYPES_H
#define MOTION_TYPES_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    MOTION_MODE_ACCEL_ONLY,
    MOTION_MODE_ACCEL_GYRO,
} motion_state_t;

typedef struct {
    float x;
    float y;
    float z;
} motion_vec3_t;

typedef struct {
    bool no_motion;
    uint8_t steps;
}imu_stage1_result_t;

#endif /* MOTION_TYPES_H */
