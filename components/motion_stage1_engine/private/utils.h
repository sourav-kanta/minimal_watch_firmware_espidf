#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stage1_types.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>

#define SAMPLE_SCALING_FACTOR           0.0002441f

bool ring_buffer_init(uint8_t size, uint8_t elem_size, ring_buffer_t* out);
bool ring_buffer_deinit(ring_buffer_t* buf);
void ring_buffer_insert(ring_buffer_t* buf, void *elem);
void* ring_buffer_get_element_at_index(const ring_buffer_t* buf, uint8_t idx);

static inline void* ring_buffer_pointer_at_index(const ring_buffer_t* buf, uint8_t i) {
    return ((uint8_t*)buf->data + i*buf->elem_size);
}

static inline float squaref(float x) {
    return (x*x);
}

static inline float vec_mod_square(const motion_vec3_t *vector) {
    return squaref(vector->x) + squaref(vector->y) + squaref(vector->z);
}

static inline void vec_subtract(const motion_vec3_t *a, const motion_vec3_t *b,
                                 motion_vec3_t* out) {
    out->x = a->x - b->x;
    out->y = a->y - b->y;
    out->z = a->z - b->z;
}

static inline void vec_add(const motion_vec3_t *a, const motion_vec3_t *b,
                                motion_vec3_t* out) {
    out->x = a->x + b->x;
    out->y = a->y + b->y;
    out->z = a->z + b->z;
}

static inline void vec_scale(const motion_vec3_t* v, float scalar, motion_vec3_t* out) {
    out->x = v->x * scalar;
    out->y = v->y * scalar;
    out->z = v->z * scalar;
}

static inline float vec_mod_of_a_minus_b_square(const motion_vec3_t *a, const motion_vec3_t *b) {
    motion_vec3_t res;
    vec_subtract(a, b, &res);
    return vec_mod_square(&res);
}

static inline float vec_a_component_on_g(const motion_vec3_t* a, const motion_vec3_t* g) {
    float res = a->x*g->x + a->y*g->y + a->z*g->z;
    float denominator_sq = vec_mod_square(g);
    if(denominator_sq <= 0.2f) return res;
    // |g| should be close 1 so sqr(g) and g should be equivalent
    return res/denominator_sq;
}

static inline void sample_to_g(int16_t x, int16_t y, int16_t z,  motion_vec3_t *out) {
    out->x = ((float) x)*SAMPLE_SCALING_FACTOR;
    out->y = ((float) y)*SAMPLE_SCALING_FACTOR;
    out->z = ((float) z)*SAMPLE_SCALING_FACTOR;
}

#endif /* UTILS_H */
