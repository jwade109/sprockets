// autogenerated by msggen.c from msg/timestamp.msg

// message definition:
// u32 secs
// u32 usecs

#include <msg/timestamp.h>
#include <stdio.h>
#include <string.h>

void print_timestamp(const timestamp_t *m)
{
    printf("timestamp_t ");
    printf("secs=%d ", m->secs);
    printf("usecs=%d ", m->usecs);
    printf("\n");
}

timestamp_t new_timestamp()
{
    timestamp_t ret;
    memset(&ret, 0, sizeof(ret));
    return ret;
}

void serialize_timestamp(const timestamp_t *m, uint8_t *dst)
{
    memcpy(dst, m, sizeof(timestamp_t));
}

int deserialize_timestamp(timestamp_t *dst, const uint8_t *buffer, size_t len)
{
    if (len != sizeof(timestamp_t)) { return -1; }
    memcpy(dst, buffer, len);
    return 0;
}

