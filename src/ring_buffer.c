#include <ring_buffer.h>

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int init_buffer(ring_buffer_t *buffer, size_t elem_size, size_t capacity)
{
    buffer->wptr = 0;
    buffer->rptr = 0;
    buffer->capacity = capacity;
    buffer->size = 0;
    buffer->elem_size = elem_size;
    buffer->high_water_mark = 0;
    // TODO make this allocate from an arena
    buffer->data = malloc(elem_size * capacity);
    if (!buffer->data)
    {
        return 0;
    }
    return 1;
}

void free_buffer(ring_buffer_t *buffer)
{
    free(buffer->data);
    memset(buffer, 0, sizeof(ring_buffer_t));
}

void print_ring_buffer(const ring_buffer_t *b)
{
    printf("[%zu/%zu r=%zu w=%zu h=%zu]",
        b->size, b->capacity, b->rptr, b->wptr, b->high_water_mark);
}

void reset_ptrs_if_possible(ring_buffer_t *b)
{
    int reset = b->rptr == b->wptr && !b->size;
    b->rptr -= b->rptr * reset;
    b->wptr -= b->wptr * reset;
}

void assert_invariants(const ring_buffer_t *buffer)
{
    if (buffer->size == 0 || buffer->size == buffer->capacity)
    {
        assert(buffer->rptr == buffer->wptr);
        return;
    }
    assert(buffer->rptr != buffer->wptr);
    size_t count = 0;
    for (size_t i = buffer->rptr; i != buffer->wptr; ++i, i %= buffer->capacity)
    {
        ++count;
    }
    assert(count == buffer->size);
}

void ring_put(ring_buffer_t *buffer, const void *data)
{
    void *dst = buffer->data + buffer->wptr * buffer->elem_size;
    memcpy(dst, data, buffer->elem_size);
    buffer->wptr++;
    buffer->wptr %= buffer->capacity;
    ++buffer->size;
    if (buffer->size >= buffer->capacity)
    {
        buffer->size = buffer->capacity;
        buffer->rptr = buffer->wptr;
    }
    ++buffer->high_water_mark;
    reset_ptrs_if_possible(buffer);
    // assert_invariants(buffer);
}

void* ring_get(ring_buffer_t *buffer)
{
    if (!buffer->size)
    {
        return 0;
    }
    void *ret = buffer->data + buffer->rptr * buffer->elem_size;
    buffer->rptr++;
    buffer->rptr %= buffer->capacity;
    --buffer->size;
    reset_ptrs_if_possible(buffer);
    // assert_invariants(buffer);
    return ret;
}

void* ring_peak(ring_buffer_t *buffer)
{
    if (!buffer->size)
    {
        return 0;
    }
    return buffer->data + buffer->rptr * buffer->elem_size;
}

unsigned char* to_contiguous_buffer(const ring_buffer_t *buffer)
{
    if (!buffer->size)
    {
        return 0;
    }

    unsigned char *ret = malloc(buffer->size);
    if (!ret)
    {
        return 0;
    }

    for (size_t i = 0; i < buffer->size; ++i)
    {
        size_t p = (buffer->rptr + i) % buffer->capacity;
        ret[i] = buffer->data[p];
    }

    return ret;
}
