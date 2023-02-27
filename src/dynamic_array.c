
#include <dynamic_array.h>

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int init_array(dynamic_array *array, size_t elem_size, size_t init_capacity)
{
    array->data = malloc(elem_size * init_capacity);
    if (!array->data)
    {
        return 0;
    }
    array->capacity = init_capacity;
    array->elem_size = elem_size;
    array->size = 0;
    return 1;
}

void free_array(dynamic_array *array)
{
    free(array->data);
    memset(array, 0, sizeof(dynamic_array));
}

void print_array(const dynamic_array *a)
{
    printf("[%zu/%zu d=%p sizeof=%zu]\n",
        a->size, a->capacity, a->data, a->elem_size);
}

int array_expand(dynamic_array *array, size_t new_capacity)
{
    // this should only be used for growing the array, not shrinking it
    assert(array->capacity < new_capacity);

    void *old_data = array->data;
    array->data = malloc(array->elem_size * new_capacity);
    if (!array->data)
    {
        return 0;
    }
    memcpy(array->data, old_data, array->elem_size * array->capacity);
    array->capacity = new_capacity;
    return 1;
}

void* array_get(const dynamic_array *array, size_t idx)
{
    assert(array->size > 0);
    assert(idx < array->size);

    return array->data + array->elem_size * idx;
}

void* array_new(dynamic_array *array)
{
    // TODO implement expansion
    assert(array->size + 1 < array->capacity);

    ++array->size;
    return array_get(array, array->size - 1);
}
