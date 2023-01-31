#include <stdlib.h>
#include <ctype.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <errno.h>

#define PACKET_DATA_LEN 128

#pragma pack(push, 1)
typedef struct packet_t
{
    uint64_t secs;
    uint64_t usecs;
    uint64_t datalen;
    uint16_t datatype;
    uint8_t checksum;
    uint8_t data[PACKET_DATA_LEN];
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
    return sum;
}

packet_t get_stamped_packet(char *msg)
{
    struct timeval time;
    gettimeofday(&time, NULL);
    packet_t packet;
    packet.secs = time.tv_sec;
    packet.usecs = time.tv_usec;
    packet.datatype = 0;
    packet.datalen = 0;
    memset(packet.data, 0, sizeof(packet.data));
    if (msg)
    {
        memcpy(packet.data, msg, strlen(msg));
    }
    packet.checksum = compute_checksum((uint8_t *) &packet, sizeof(packet));
    return packet;
};

void print_binary_str_sequence(char *seq, int len)
{
    for (int i = 0; i < len; ++i)
    {
        char c = seq[i];
        printf("%c", (c > 31 && c < 127) ? seq[i] : '.');
    }
    printf("\n");
    // for (int i = 0; i < len; ++i)
    // {
    //     printf("%02x ", (uint8_t) seq[i]);
    // }
    // printf("\n");
}

void print_packet(packet_t packet)
{
    printf("PACKET %lu.%06lu DATA %s", packet.secs, packet.usecs, packet.data);
}

int get_input_stdin(const char *prompt, char *buffer, size_t bufsize)
{
    printf("%s", prompt);
    int len = getline(&buffer, &bufsize, stdin);
    buffer[len-1] = 0; // remove newline
    return len;
}

int send_message(int fsock, char *msg, int len)
{
    if (len < 1)
    {
        printf("Cannot send 0 characters.\n");
        return 1;
    }

    // printf("Sending %d characters:\n", len);
    // print_binary_str_sequence(msg, len);

    int sent = send(fsock, msg, len, 0);
    if (sent < 0 || len != sent)
    {
        printf("Failed to send: %s\n", strerror(errno));
        return 1;
    }
    return 0;
}

int read_message(int fsock, char *buffer, int len)
{
    int numread = read(fsock, buffer, len);
    if (numread < 0)
    {
        printf("Failed to read: %s\n", strerror(errno));
        return -1;
    }

    if (numread == 0)
    {
        printf("Empty message.\n");
        return 0;
    }
    return numread;
}

int send_packet(int fsock, const packet_t *packet)
{
    int ret = send_message(fsock, (char *) packet, sizeof(packet_t));
    if (ret)
    {
        return ret;
    }

    printf("[SENT] ");
    print_packet(*packet);
    printf("\n");

    return ret;
}

int read_packet(int fsock, packet_t *packet)
{
    int numread = read_message(fsock, (char *) packet, sizeof(packet_t));
    if (numread != sizeof(packet_t))
    {
        return 1; // failure
    }

    struct timeval now;
    gettimeofday(&now, NULL);
    uint64_t nusec = now.tv_sec * 1E6 + now.tv_usec;
    uint64_t pusec = packet->secs * 1E6 + packet->usecs;
    int64_t dt = nusec - pusec;

    printf("[RECV] ");
    print_packet(*packet);
    printf(" [%ld us]\n", dt);

    return 0; // success
}
