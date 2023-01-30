#include <stdlib.h>
#include <ctype.h>
#include <sys/time.h>

#pragma pack(push, 1)
typedef struct packet_t
{
    uint64_t secs;
    uint64_t usecs;
    uint64_t datalen;
    uint16_t datatype;
    uint8_t checksum;
    uint8_t data[128];
} packet_t;
#pragma pack(pop)

uint8_t compute_checksum(uint8_t *array, size_t len)
{
    uint8_t sum = 0;
    for (size_t i = 0; i < len; ++i)
    {
        sum ^= array[i];
    }
    sum = ~sum;
    printf("checksum: %d\n", (int) sum);
    return sum;
}

packet_t get_stamped_packet()
{
    struct timeval time;
    gettimeofday(&time, NULL);
    packet_t packet;
    packet.secs = time.tv_sec;
    packet.usecs = time.tv_usec;
    packet.datatype = 0;
    packet.datalen = 0;
    memset(packet.data, 0, sizeof(packet.data));
    return packet;
};

void print_binary_str_sequence(char *seq, int len)
{
    printf("  ");
    for (int i = 0; i < len; ++i)
    {
        char c = seq[i];
        printf("%c", (c > 31 && c < 127) ? seq[i] : '.');
    }
    printf("\n  ");
    for (int i = 0; i < len; ++i)
    {
        printf("%02x ", (uint8_t) seq[i]);
    }
    printf("\n");
}

int is_quit(char *msg)
{
    return strcmp(msg, "quit") == 0;
}

int send_message(int fsock, char *msg, int len)
{
    if (len < 1)
    {
        printf("Cannot send 0 characters.\n");
        return 1;
    }

    printf("Sending %d characters:\n", len);
    print_binary_str_sequence(msg, len);

    int sent = send(fsock, msg, len, 0);
    if (sent < 0 || len != sent)
    {
        printf("Failed to send: %s\n", strerror(errno));
        return 1;
    }
    return 0;
}

int read_message(int fsock, char **msg)
{
    size_t bufsize = 1024;
    *msg = calloc(1024, 1);

    printf("Reading...\n");
    int numread = read(fsock, *msg, bufsize);
    if (numread < 0)
    {
        printf("Failed to read: %s\n", strerror(errno));
        return -1;
    }

    printf("Read %d characters:\n", numread);
    if (numread == 0)
    {
        printf("Empty message.\n");
        return 0;
    }
    print_binary_str_sequence(*msg, numread);
    return numread;
}

char* get_input_stdin(const char *prompt)
{
    size_t bufsize = 1024;
    char *msg = calloc(1024, sizeof(char));
    if (!msg)
    {
        return msg;
    }
    int len = 0;
    while (len < 2)
    {
        printf("%s", prompt);
        len = getline(&msg, &bufsize, stdin);
    }
    msg[len-1] = 0; // remove newline
    return msg;
}
