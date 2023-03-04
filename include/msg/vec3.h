// autogenerated by msggen.c from msg/vec3.msg

// message definition:
// f32 x
// f32 y
// f32 z

#ifndef SPROCKETS_AUTOGEN_MESSAGE_vec3_H
#define SPROCKETS_AUTOGEN_MESSAGE_vec3_H

#include <stdint.h>

#pragma pack(push, 1)
typedef struct
{
    float x;
    float y;
    float z;
}
vec3_t;
#pragma pack(pop)

void print_vec3(const vec3_t *m);

#endif // SPROCKETS_AUTOGEN_MESSAGE_vec3_H
