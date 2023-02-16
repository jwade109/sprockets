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
    buffer->data = 0;
}

void print_ring_buffer(ring_buffer_t *b)
{
    printf("[size=%zu rptr=%zu wptr=%zu cap=%zu]",
        b->size, b->rptr, b->wptr, b->capacity);
}

void assert_invariants(ring_buffer_t *buffer)
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
    // assert_invariants(buffer);
    return ret;
}
