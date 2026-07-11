#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef enum
{
    RB_OVERFLOW_DROP_OLDEST,
    RB_OVERFLOW_DROP_NEWEST,
} rb_overflow_policy_t;

typedef struct
{
    void *buffer;
    size_t element_size;
    uint16_t capacity;
    uint16_t read_idx;
    uint16_t write_idx;
    uint16_t count;
    SemaphoreHandle_t mutex;
    rb_overflow_policy_t overflow_policy;
} ring_buffer_t;

bool ringbuf_init(ring_buffer_t *rb,
                  void *storage,
                  uint16_t capacity,
                  size_t element_size,
                  rb_overflow_policy_t policy);

void ringbuf_deinit(ring_buffer_t *rb);
bool ringbuf_push(ring_buffer_t *rb, const void *item);
bool ringbuf_pop(ring_buffer_t *rb, void *item);
bool ringbuf_is_empty(const ring_buffer_t *rb);
bool ringbuf_is_full(const ring_buffer_t *rb);
bool ringbuf_clear(ring_buffer_t *rb);
uint16_t ringbuf_capacity(const ring_buffer_t *rb);

#endif /* RING_BUFFER_H */
