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
typedef struct
{
    uint16_t preamble;
    uint32_t sno;
    uint32_t secs;
    uint32_t usecs;
    uint32_t datalen;
    uint16_t datatype;
    uint8_t checksum;
    char data[PACKET_DATA_LEN];
}
packet_t;
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

uint32_t packet_serial_no = 0;

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
    packet.checksum = 256 - array_sum((const char *) &packet, sizeof(packet));
    return packet;
}

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
    printf("PACKET #%u %u.%06u %s 0x%02X",
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

    // uint64_t usecs;
    // uint64_t datalen;
    // uint16_t datatype;
    // uint8_t checksum;
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
    return ret;
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
    char *data;
    size_t wptr;
    size_t rptr;
    size_t capacity;
    size_t size;
    size_t elem_size;
}
ring_buffer_t;

int init_buffer(ring_buffer_t *buffer, size_t elem_size, size_t capacity)
{
    buffer->wptr = 0;
    buffer->rptr = 0;
    buffer->capacity = capacity;
    buffer->size = 0;
    buffer->elem_size = elem_size;
    // TODO make this allocate from an arena
    buffer->data = malloc(elem_size * capacity);
    if (!buffer->data)
    {
        return 0;
    }
    return 1;
}

void free_buffer(ring_buffer_t *buffer)
{
    free(buffer->data);
    buffer->data = 0;
}

void print_ring_buffer(ring_buffer_t *b)
{
    printf("[size=%zu rptr=%zu wptr=%zu cap=%zu]",
        b->size, b->rptr, b->wptr, b->capacity);
}

void assert_invariants(ring_buffer_t *buffer)
{
    if (buffer->size == 0 || buffer->size == buffer->capacity)
    {
        assert(buffer->rptr == buffer->wptr);
        return;
    }
    assert(buffer->rptr != buffer->wptr);
    size_t count = 0;
    for (size_t i = buffer->rptr; i != buffer->wptr; ++i, i %= buffer->capacity)
    {
        ++count;
    }
    assert(count == buffer->size);
}

void ring_put(ring_buffer_t *buffer, const void *data)
{
    void *dst = buffer->data + buffer->wptr * buffer->elem_size;
    memcpy(dst, data, buffer->elem_size);
    buffer->wptr++;
    buffer->wptr %= buffer->capacity;
    ++buffer->size;
    if (buffer->size >= buffer->capacity)
    {
        buffer->size = buffer->capacity;
        buffer->rptr = buffer->wptr;
    }
    // assert_invariants(buffer);
}

void* ring_get(ring_buffer_t *buffer)
{
    if (!buffer->size)
    {
        return 0;
    }
    void *ret = buffer->data + buffer->rptr * buffer->elem_size;
    buffer->rptr++;
    buffer->rptr %= buffer->capacity;
    --buffer->size;
    // assert_invariants(buffer);
    return ret;
}

int buffered_read_msg(int fsock, ring_buffer_t *inbuf)
{
    char buffer[INPUT_BUFFER_SIZE];

    int numread = read_message(fsock, buffer, INPUT_BUFFER_SIZE);
    for (int i = 0; i < numread; ++i)
    {
        ring_put(inbuf, (void *) &buffer[i]);
    }
    return numread;
}

typedef struct
{
    int socket_fd;
    ring_buffer_t read_buffer;
    ring_buffer_t inbox;
    ring_buffer_t outbox;
}
node_conn_t;

// returns 0 on not enough data
// returns < 0 for various error codes
// returns 1 if packet is successfully casted
int cast_buffer_to_packet(packet_t *p, const char *buffer, size_t len)
{
    if (len < sizeof(packet_t))
    {
        return 0;
    }

    uint8_t computed_checksum = array_sum(buffer, len);

    *p = *((packet_t *) buffer);

    if (p->preamble != PACKET_PREAMBLE)
    {
        // printf("Bad preamble; expected 0x%X, got 0x%X.\n",
        //     PACKET_PREAMBLE, p->preamble);
        return -1;
    }
    if (computed_checksum)
    {
        // printf("Bad checksum; expected 0x00, got 0x%02X.\n", computed_checksum);
        return -2;
    }

    return 1;
}

uint32_t seqnum = 0;

int spin(node_conn_t *conn)
{
    if (can_recv(conn->socket_fd))
    {
        int len = buffered_read_msg(conn->socket_fd, &conn->read_buffer);
        if (!len)
        {
            return 1;
        }

        while (conn->read_buffer.size >= sizeof(packet_t))
        {
            char contig_buf[sizeof(packet_t)];
            for (size_t i = 0; i < sizeof(packet_t); ++i)
            {
                contig_buf[i] = *(char *) ring_get(&conn->read_buffer);
            }
            packet_t packet;
            if (cast_buffer_to_packet(&packet, contig_buf, sizeof(packet_t)) == 1)
            {
                ring_put(&conn->inbox, &packet);
            }
        }
    }

    while (conn->outbox.size)
    {
        packet_t *out = ring_get(&conn->outbox);
        if (send_packet(conn->socket_fd, out))
        {
            printf("Failed to send.\n");
            return 1;
        }
    }

    return 0;
}

void init_conn(node_conn_t *conn)
{
    init_buffer(&conn->read_buffer, sizeof(char), 6000);
    init_buffer(&conn->inbox, sizeof(packet_t), 100);
    init_buffer(&conn->outbox, sizeof(packet_t), 100);
}

void free_conn(node_conn_t *conn)
{
    free_buffer(&conn->read_buffer);
    free_buffer(&conn->inbox);
    free_buffer(&conn->outbox);
    close(conn->socket_fd);
}

void print_conn(node_conn_t *conn)
{
    printf(  "rbuf: ");
    print_ring_buffer(&conn->read_buffer);
    printf("\nin:   ");
    print_ring_buffer(&conn->inbox);
    printf("\nout:  ");
    print_ring_buffer(&conn->outbox);
    printf("\n");
}
