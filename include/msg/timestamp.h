// autogenerated by msggen.c from msg/timestamp.msg

// message definition:
// u32 secs
// u32 usecs

#ifndef SPROCKETS_AUTOGEN_MESSAGE_timestamp_H
#define SPROCKETS_AUTOGEN_MESSAGE_timestamp_H

#include <stdint.h>
#include <stddef.h>

#pragma pack(push, 1)
typedef struct
{
    uint32_t secs;
    uint32_t usecs;
}
timestamp_t;
#pragma pack(pop)

void print_timestamp(const timestamp_t *m);

timestamp_t new_timestamp();

void serialize_timestamp(const timestamp_t *m, uint8_t *dst);

int deserialize_timestamp(timestamp_t *dst, const uint8_t *buffer, size_t len);

#endif // SPROCKETS_AUTOGEN_MESSAGE_timestamp_H

