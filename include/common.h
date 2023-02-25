#ifndef SPROCKETS_COMMON_PROCS_H
#define SPROCKETS_COMMON_PROCS_H

#include <stdint.h>
#include <assert.h>
#include <netinet/in.h>

#include "ring_buffer.h"

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

uint8_t array_sum(const char *array, size_t len);

packet_t get_stamped_packet(char *msg);

void print_hexdump(char *seq, int len);

void print_packet(packet_t packet);

int set_socket_reusable(int fsock);

void set_socket_timeout(int fsock, int sec, int usec);

struct sockaddr_in get_sockaddr(const char *ipaddr, int port);

struct sockaddr_in get_peer_name(int fsock);

int get_input_stdin(const char *prompt, char *buffer, size_t bufsize);

int send_message(int fsock, const char *msg, int len);

int send_string(int socket, const char *msg);

int read_message(int fsock, char *buffer, int len);

int send_packet(int fsock, const packet_t *packet);

int connect_to_server(int fsock, const char *ip_addr_str, int port);

int can_recv(int fsock);

int buffered_read_msg(int fsock, ring_buffer_t *inbuf);

typedef struct
{
    int socket_fd;
    int is_connected;
    ring_buffer_t read_buffer;
    ring_buffer_t inbox;
    ring_buffer_t outbox;
}
node_conn_t;

// returns 0 on not enough data
// returns < 0 for various error codes
// returns 1 if packet is successfully casted
int cast_buffer_to_packet(packet_t *p, const char *buffer, size_t len);

int spin(node_conn_t *conn);

void init_conn(node_conn_t *conn);

void free_conn(node_conn_t *conn);

void print_conn(const node_conn_t *conn);

typedef struct
{
    int server_fd;
    node_conn_t *clients;
    size_t client_max;
    size_t client_count;
}
server_t;

int open_server(server_t *s, int client_max);

int close_server(server_t *s);

void print_server(const server_t *s);

int spin_server(server_t *server, struct sockaddr_in *address);

#endif // SPROCKETS_COMMON_PROCS_H
