#include <ring_buffer.h>
#include <string.h>

static inline uint16_t ringbuf_next(const ring_buffer_t *rb, uint16_t index)
{
    return (index + 1U) % rb->capacity;
}

static inline bool ringbuf_is_empty_unsafe(const ring_buffer_t *rb)
{
    return rb->count == 0;
}

static inline bool ringbuf_is_full_unsafe(const ring_buffer_t *rb)
{
    return rb->count == rb->capacity;
}

static inline void *ringbuf_ptr(const ring_buffer_t *rb, uint16_t index)
{
    return (uint8_t *)rb->buffer + (index * rb->element_size);
}

bool ringbuf_init(ring_buffer_t *rb, void *storage, uint16_t capacity, 
                  size_t element_size, rb_overflow_policy_t policy)
{
    if (rb == NULL || storage == NULL || capacity < 1U || element_size == 0U)
    {
        return false;
    }

    rb->buffer = storage;
    rb->capacity = capacity;
    rb->element_size = element_size;
    rb->read_idx = 0;
    rb->write_idx = 0;
    rb->count = 0;
    rb->overflow_policy = policy;

    rb->mutex = xSemaphoreCreateMutex();

    if (rb->mutex == NULL)
    {
        return false;
    }

    return true;
}

void ringbuf_deinit(ring_buffer_t *rb)
{
    if ((rb == NULL) || (rb->mutex == NULL))
    {
        return;
    }

    vSemaphoreDelete(rb->mutex);
    rb->mutex = NULL;
}

bool ringbuf_is_empty(const ring_buffer_t *rb)
{
    if ((rb == NULL) || (rb->mutex == NULL))
    {
        return true;
    }

    configASSERT(xSemaphoreTake(rb->mutex, portMAX_DELAY) == pdTRUE);
    bool empty = ringbuf_is_empty_unsafe(rb);
    xSemaphoreGive(rb->mutex);

    return empty;
}

bool ringbuf_is_full(const ring_buffer_t* rb) {
    if ((rb == NULL) || (rb->mutex == NULL))
    {
        return true;
    }

    configASSERT(xSemaphoreTake(rb->mutex, portMAX_DELAY) == pdTRUE);
    bool full = ringbuf_is_full_unsafe(rb);
    xSemaphoreGive(rb->mutex);
    return full;
}

bool ringbuf_clear(ring_buffer_t *rb) {
    if ((rb == NULL) || (rb->mutex == NULL))
    {
        return false;
    }

    configASSERT(xSemaphoreTake(rb->mutex, portMAX_DELAY) == pdTRUE);
    rb->read_idx = 0;
    rb->write_idx = 0;
    rb->count = 0;
    xSemaphoreGive(rb->mutex);
    return true;
}

bool ringbuf_pop(ring_buffer_t *rb, void* item) {
    if(rb == NULL || rb->mutex == NULL || item == NULL) {
        return false;
    }
    
    configASSERT(xSemaphoreTake(rb->mutex, portMAX_DELAY) == pdTRUE);
    if(ringbuf_is_empty_unsafe(rb)) {
        xSemaphoreGive(rb->mutex);
        return false;
    }
    void* read_ptr = ringbuf_ptr(rb, rb->read_idx);
    memcpy(item, read_ptr, rb->element_size);
    rb->read_idx = ringbuf_next(rb, rb->read_idx);
    rb->count--;
    configASSERT(rb->count <= rb->capacity);
    xSemaphoreGive(rb->mutex);
    return true;
}

bool ringbuf_push(ring_buffer_t *rb, const void *item) {
    if(rb == NULL || rb->mutex == NULL || item == NULL) {
        return false;
    }
    
    configASSERT(xSemaphoreTake(rb->mutex, portMAX_DELAY) == pdTRUE);
    if(ringbuf_is_full_unsafe(rb)) {
        switch(rb->overflow_policy) {
            case RB_OVERFLOW_DROP_NEWEST :
                // Unimplemented
                xSemaphoreGive(rb->mutex);
                return false;
                break;
            case RB_OVERFLOW_DROP_OLDEST :
                rb->count--;
                rb->read_idx = ringbuf_next(rb, rb->read_idx);
                break;
        }
    }
    void* write_ptr = ringbuf_ptr(rb, rb->write_idx);
    memcpy(write_ptr, item, rb->element_size);
    rb->write_idx = ringbuf_next(rb, rb->write_idx);
    rb->count++;
    configASSERT(rb->count <= rb->capacity);
    xSemaphoreGive(rb->mutex);
    return true;
}

uint16_t ringbuf_capacity(const ring_buffer_t *rb)
{
    return (rb != NULL) ? rb->capacity : 0;
}
