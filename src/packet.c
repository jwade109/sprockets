// autogenerated by msggen.c from msg/packet.msg

// message definition:
// u16 preamble
// u32 sno
// u32 secs
// u32 usecs
// u32 datalen
// u16 datatype
// u8 checksum
// str data

#include <packet.h>
#include <stdio.h>

void print_packet(const packet_t *m)
{
    printf("packet_t ");
    printf("preamble=%d ", m->preamble);
    printf("sno=%d ", m->sno);
    printf("secs=%d ", m->secs);
    printf("usecs=%d ", m->usecs);
    printf("datalen=%d ", m->datalen);
    printf("datatype=%d ", m->datatype);
    printf("checksum=%d ", m->checksum);
    printf("data=%s ", m->data);
    printf("\n");
}
