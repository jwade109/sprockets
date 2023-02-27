#ifndef SPROCKETS_DYNAMIC_ARRAY_H
#define SPROCKETS_DYNAMIC_ARRAY_H

#include <stddef.h>

typedef struct
{
    char *data;
    size_t capacity;
    size_t size;
    size_t elem_size;
}
dynamic_array;

int init_array(dynamic_array *array, size_t elem_size, size_t init_capacity);

void free_array(dynamic_array *array);

void print_array(const dynamic_array *array);

void* array_new(dynamic_array *array);

void* array_get(const dynamic_array *array, size_t idx);

#endif // SPROCKETS_DYNAMIC_ARRAY_H