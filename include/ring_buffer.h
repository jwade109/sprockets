#ifndef SPROCKETS_RING_BUFFER_H
#define SPROCKETS_RING_BUFFER_H

#include <stddef.h>

typedef struct
{
    char *data;
    size_t wptr;
    size_t rptr;
    size_t capacity;
    size_t size;
    size_t elem_size;
}
ring_buffer_t;

int init_buffer(ring_buffer_t *buffer, size_t elem_size, size_t capacity);

void free_buffer(ring_buffer_t *buffer);

void print_ring_buffer(const ring_buffer_t *b);

void assert_invariants(const ring_buffer_t *buffer);

void ring_put(ring_buffer_t *buffer, const void *data);

void* ring_get(ring_buffer_t *buffer);

#endif // SPROCKETS_RING_BUFFER_H