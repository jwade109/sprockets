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
#define PACKET_PREAMBLE 0x20F7

#pragma pack(push, 1)
typedef struct packet_t
{
    uint16_t preamble;
    uint32_t sno;
    uint32_t secs;
    uint32_t usecs;
    uint32_t datalen;
    uint16_t datatype;
    uint8_t checksum;
    uint8_t data[PACKET_DATA_LEN];
} packet_t;
#pragma pack(pop)

static_assert(sizeof(packet_t) == 149, "Packet size is not as expected");
static_assert(sizeof(packet_t) == 21 + PACKET_DATA_LEN, "Packet size is not as expected");
static_assert(offsetof(packet_t, preamble) == 0, "packet_t::secs offset");
static_assert(offsetof(packet_t, sno) == 2, "packet_t::secs offset");
static_assert(offsetof(packet_t, secs) == 6, "packet_t::secs offset");
static_assert(offsetof(packet_t, usecs) == 10, "packet_t::usecs offset");
static_assert(offsetof(packet_t, datalen) == 14, "packet_t::datalen offset");
static_assert(offsetof(packet_t, datatype) == 18, "packet_t::datatype offset");
static_assert(offsetof(packet_t, checksum) == 20, "packet_t::checksum offset");
static_assert(offsetof(packet_t, data) == 21, "packet_t::data offset");

uint8_t array_sum(const char *array, size_t len)
{
    uint8_t sum = 0;
    for (size_t i = 0; i < len; ++i)
    {
        sum += (uint8_t) array[i];
    }
    return sum;
}

uint64_t packet_serial_no = 0;

packet_t get_stamped_packet(char *msg)
{
    struct timeval time;
    gettimeofday(&time, NULL);
    packet_t packet;
    memset(&packet, 0, sizeof(packet_t));
    packet.preamble = PACKET_PREAMBLE;
    packet.sno = packet_serial_no++;
    packet.secs = time.tv_sec;
    packet.usecs = time.tv_usec;
    packet.datatype = 0;
    packet.datalen = 0;
    if (msg)
    {
        memcpy(packet.data, msg, strlen(msg));
    }
    packet.checksum = 256 - array_sum((uint8_t *) &packet, sizeof(packet));
    return packet;
};

void print_hexdump(char *seq, int len)
{
    if (!len)
    {
        return;
    }
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
    printf("PACKET[%u] %u.%06u %s 0x%02X",
        packet.sno, packet.secs, packet.usecs, packet.data, packet.checksum);
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
    // print_hexdump(msg, len);

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

#define INPUT_BUFFER_SIZE 6000

typedef struct
{
    char data[INPUT_BUFFER_SIZE];
    int wptr;
    int rptr;
    int capacity;
    int size;
}
ring_buffer_t;

void reset_buffer(ring_buffer_t *buffer)
{
    buffer->wptr = 0;
    buffer->rptr = 0;
    buffer->capacity = INPUT_BUFFER_SIZE;
    buffer->size = 0;
}

void print_ring_buffer(ring_buffer_t *b)
{
    printf("[rptr=%d wptr=%d size=%d cap=%d]\n",
        b->rptr, b->wptr, b->size, b->capacity);
}

void assert_invariants(ring_buffer_t *buffer)
{
    if (buffer->size == 0 || buffer->size == buffer->capacity)
    {
        assert(buffer->rptr == buffer->wptr);
        return;
    }
    assert(buffer->rptr != buffer->wptr);
    int count = 0;
    for (int i = buffer->rptr; i != buffer->wptr; ++i, i %= buffer->capacity)
    {
        ++count;
    }
    assert(count == buffer->size);
}

void ring_put(ring_buffer_t *buffer, char c)
{
    buffer->data[buffer->wptr++] = c;
    buffer->wptr %= buffer->capacity;
    ++buffer->size;
    if (buffer->size >= buffer->capacity)
    {
        buffer->size = buffer->capacity;
        buffer->rptr = buffer->wptr;
    }
    assert_invariants(buffer);
}

char ring_get(ring_buffer_t *buffer)
{
    if (!buffer->size)
    {
        return 0;
    }
    char c = buffer->data[buffer->rptr++];
    buffer->rptr %= buffer->capacity;
    --buffer->size;
    assert_invariants(buffer);
    return c;
}

int buffered_read_msg(int fsock, ring_buffer_t *inbuf)
{
    char buffer[INPUT_BUFFER_SIZE];

    int numread = read_message(fsock, buffer, INPUT_BUFFER_SIZE);
    for (int i = 0; i < numread; ++i)
    {
        ring_put(inbuf, buffer[i]);
    }
    return numread;
}

typedef void(*on_packet_cb_t)(const packet_t *);

typedef struct
{
    int socket_fd;
    ring_buffer_t read_buffer;
    on_packet_cb_t on_packet;
}
node_conn_t;

// returns 0 on not enough data
// returns < 0 for various error codes
// returns 1 if packet is successfully casted
int cast_buffer_to_packet(packet_t *p, const char *buffer, int len)
{
    if (len < sizeof(packet_t))
    {
        return 0;
    }

    uint8_t computed_checksum = array_sum(buffer, len);

    *p = *((packet_t *) buffer);

    if (p->preamble != PACKET_PREAMBLE)
    {
        printf("Bad preamble; expected 0x%X, got 0x%X.\n",
            PACKET_PREAMBLE, p->preamble);
        return -1;
    }
    if (computed_checksum)
    {
        printf("Bad checksum; expected 0x00, got 0x%02X.\n", computed_checksum);
        return -2;
    }

    return 1;
}

uint64_t seqnum = 0;

int two_way_loop(int fsock, ring_buffer_t *buffer, on_packet_cb_t on_rcv_cb)
{
    if (!on_rcv_cb)
    {
        printf("Bad callback.\n");
        return 1;
    }

    // reset_buffer(buffer);

    if (can_recv(fsock))
    {
        int len = buffered_read_msg(fsock, buffer);
        if (!len)
        {
            return 1;
        }

        // print_ring_buffer(buffer);

        while (buffer->size >= sizeof(packet_t))
        {
            char contig_buf[sizeof(packet_t)];
            for (int i = 0; i < sizeof(packet_t); ++i)
            {
                contig_buf[i] = ring_get(buffer);
            }
            packet_t packet;
            if (cast_buffer_to_packet(&packet, contig_buf, sizeof(packet_t)) == 1)
            {
                on_rcv_cb(&packet);
            }
        }

    }

    if (seqnum++ % 25 == 0)
    {
        time_t rawtime;
        struct tm *info;
        const int len = 200;
        char buffer[len];

        time( &rawtime );

        info = localtime( &rawtime );

        strftime(buffer, len, "%A, %B %d %FT%TZ", info);

        packet_t packet = get_stamped_packet(buffer);
        if (send_packet(fsock, &packet))
        {
            printf("Failed to send.\n");
            return 1;
        }
    }

    usleep(1000);

    return 0;
}

int spin(node_conn_t *conn)
{
    return two_way_loop(conn->socket_fd, &conn->read_buffer, conn->on_packet);
}

void reset_conn(node_conn_t *conn)
{
    reset_buffer(&conn->read_buffer);
    conn->on_packet = 0;
}
