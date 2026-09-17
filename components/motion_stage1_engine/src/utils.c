#include <utils.h>
#include <stage1_types.h>
#include <motion_types.h>
#include <string.h>

bool ring_buffer_init(uint8_t size, uint8_t elem_size, ring_buffer_t* out) {
    assert(out);
    out->data = malloc((size_t)size*elem_size);
    if(!out->data) return false;
    out->read_idx = 0;
    out->write_idx = 0;
    out->elements = 0;
    out->elem_size = elem_size;
    out->max_size = size;
    return true;
}

bool ring_buffer_deinit(ring_buffer_t* buf) {
    assert(buf);
    if(!buf->data) return false;
    free(buf->data);
    memset(buf, 0, sizeof(ring_buffer_t));
    return true;
}

void ring_buffer_insert(ring_buffer_t* buf, void *elem) {
    assert(buf);
    assert(buf->data);
    if(buf->elements < buf->max_size) {
        buf->elements++;
    }
    else {
        assert(buf->read_idx == buf->write_idx);
        buf->read_idx = (buf->read_idx+1) >= buf->max_size ?  0 : buf->read_idx + 1;
    }
    memcpy(ring_buffer_pointer_at_index(buf, buf->write_idx), elem, buf->elem_size);
    buf->write_idx = (buf->write_idx+1) >= buf->max_size ? 0 : buf->write_idx + 1;
}

void* ring_buffer_get_element_at_index(const ring_buffer_t* buf, uint8_t idx) {
    assert(idx < buf->elements);
    size_t pos = (size_t)buf->read_idx + idx;
    pos = (pos) >= buf->max_size ? pos - buf->max_size : (pos);
    return ring_buffer_pointer_at_index(buf, pos);
}

