#include <stdlib.h>
#include <ctype.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
#include <signal.h>
#include <assert.h>
#include <stddef.h>

#define PACKET_DATA_LEN 128

#pragma pack(push, 1)
typedef struct packet_t
{
    uint32_t sno;
    uint32_t secs;
    uint32_t usecs;
    uint32_t datalen;
    uint16_t datatype;
    uint8_t checksum;
    uint8_t data[PACKET_DATA_LEN];
} packet_t;
#pragma pack(pop)

static_assert(sizeof(packet_t) == 128 + 19, "Packet size is not as expected");
static_assert(offsetof(packet_t, sno) == 0, "packet_t::secs offset");
static_assert(offsetof(packet_t, secs) == 4, "packet_t::secs offset");
static_assert(offsetof(packet_t, usecs) == 8, "packet_t::usecs offset");
static_assert(offsetof(packet_t, datalen) == 12, "packet_t::datalen offset");
static_assert(offsetof(packet_t, datatype) == 16, "packet_t::datatype offset");
static_assert(offsetof(packet_t, checksum) == 18, "packet_t::checksum offset");
static_assert(offsetof(packet_t, data) == 19, "packet_t::data offset");

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

uint64_t packet_serial_no = 0;

packet_t get_stamped_packet(char *msg)
{
    struct timeval time;
    gettimeofday(&time, NULL);
    packet_t packet;
    memset(&packet, 0, sizeof(packet_t));
    packet.sno = packet_serial_no++;
    packet.secs = time.tv_sec;
    packet.usecs = time.tv_usec;
    packet.datatype = 0;
    packet.datalen = 0;
    // memset(packet.data, 0, sizeof(packet.data));
    if (msg)
    {
        memcpy(packet.data, msg, strlen(msg));
    }
    packet.checksum = compute_checksum((uint8_t *) &packet, sizeof(packet));
    return packet;
};

void print_binary_str_sequence(char *seq, int len)
{
    if (!len)
    {
        return;
    }

    // for (int i = 0; i < len; ++i)
    // {
    //     char c = seq[i];
    //     printf("%c", (c > 31 && c < 1272) ? seq[i] : '.');
    // }
    // printf("\n");
    for (int i = 0; i < len; ++i)
    {
        printf(" %02x", (uint8_t) seq[i]);
        if ((i + 1) % 16 == 0)
        {
            printf("\n");
        }
    }
    printf("\n");
}

void print_packet(packet_t packet)
{
    // printf("PACKET %lu.%06lu DATA %s", packet.secs, packet.usecs, packet.data);
    printf("PACKET[%u] %u %u",
        packet.sno, packet.secs, packet.usecs); //, packet.data);
}

void set_socket_reusable(int fsock)
{
    int option = 1;
    setsockopt(fsock, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
}

void set_socket_timeout(int fsock, int sec, int usec)
{
    struct timeval to;
    to.tv_sec = sec;
    to.tv_usec = usec;
    setsockopt(fsock, SOL_SOCKET, SO_RCVTIMEO, (const char*) &to, sizeof(to));
}

struct sockaddr_in get_sockaddr(const char *ipaddr, int port)
{
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_aton(ipaddr, (struct in_addr *) &addr.sin_addr.s_addr);
    return addr;
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

    uint64_t usecs;
    uint64_t datalen;
    uint16_t datatype;
    uint8_t checksum;
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

    printf("[SEND] ");
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

int connect_to_server(int fsock, const char *ip_addr_str, int port)
{
    struct sockaddr_in addr = get_sockaddr(ip_addr_str, port);

    int retcode = -1;
    int retries = 0;
    const int max_retries = 10000;
    while (retcode < 0 && retries < max_retries)
    {
        if (!retries)
        {
            printf("Attempting connection...\n");
        }
        retcode = connect(fsock, (struct sockaddr*) &addr, sizeof(addr));
        if (retcode < 0)
        {
            if (!retries)
            {
                printf("Failed to connect: %s. "
                    "Will wait and retry for a little bit.\n",
                    strerror(errno));
            }
            usleep(100000);
        }
        ++retries;
    }
    if (retcode < 0)
    {
        printf("Failed to connect: %s\n", strerror(errno));
    }
    return retcode;
}

int can_recv(int fsock)
{
    struct timeval select_timeout;
    select_timeout.tv_usec = 200;
    select_timeout.tv_sec = 0;

    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(fsock, &read_set);
    return select(fsock + 1, &read_set, NULL, NULL, &select_timeout) > 0;
}

// doesn't seem to actually work
// int can_send(int fsock)
// {
//     struct timeval select_timeout;
//     select_timeout.tv_usec = 200;
//     select_timeout.tv_sec = 0;

//     fd_set write_set;
//     FD_ZERO(&write_set);
//     FD_SET(fsock, &write_set);
//     return select(fsock + 1, NULL, &write_set, NULL, &select_timeout) > 0;
// }

#define INPUT_BUFFER_SIZE 10240

typedef struct input_buffer_t
{
    char data[INPUT_BUFFER_SIZE];
    int wptr;
}
input_buffer_t;

void reset_buffer(input_buffer_t *buffer)
{
    buffer->wptr = 0;
}

int buffered_read_msg(int fsock, input_buffer_t *inbuf)
{
    int remaining = INPUT_BUFFER_SIZE - inbuf->wptr;
    // printf("Remaining: %d\n", remaining);
    int numread = read_message(fsock, inbuf->data + inbuf->wptr, remaining);
    if (numread > 0)
    {
        // ADD THIS BACK IN
        inbuf->wptr += numread;
    }
    return numread;
}

// uint64_t byte_unpack_u64(const char *bptr)
// {
//     uint64_t ret = 0;

//     for (int i = 0; i < 8; ++i)
//     {
//         printf("%d\n", (int) bptr[i]);
//         ret |= (bptr[i] << (i * 8));
//     }

//     return ret;
// }

int two_way_loop(int fsock, input_buffer_t *buffer, int is_server)
{
    reset_buffer(buffer);

    if (can_recv(fsock))
    {
        packet_t packet;
        if (read_packet(fsock, &packet))
        {
            return 1;
        }
    }

    if (1.0 * rand() / RAND_MAX < 0.001)
    {
        char outbuf[1024];
        const char *msg = "hello world!";
        strcpy(outbuf, msg);

        packet_t packet = get_stamped_packet(outbuf);
        if (send_packet(fsock, &packet))
        {
            printf("Failed to send.\n");
            return 1;
        }
    }

    usleep(1000);

    return 0;
}
