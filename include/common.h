#ifndef SPROCKETS_COMMON_PROCS_H
#define SPROCKETS_COMMON_PROCS_H

#include <stdint.h>
#include <assert.h>
#include <netinet/in.h>

#include <ring_buffer.h>
#include <msg/packet.h>
#include <msg/timestamp.h>
#include <msg/vec3.h>

#define PACKET_DATA_LEN 128
#define PACKET_PREAMBLE 0x20F7

#pragma pack(push, 1)
typedef struct
{
    uint32_t preamble;
    uint32_t datalen;
    uint32_t datatype;
    uint8_t checksum;
}
packet_header_t;
#pragma pack(pop)

uint8_t array_sum(const char *array, size_t len);

packet_t get_stamped_packet(char *msg);

void print_hexdump(unsigned char *seq, size_t len);

int set_socket_reusable(int fsock);

void set_socket_timeout(int fsock, int sec, int usec);

struct sockaddr_in get_sockaddr(const char *ipaddr, int port);

struct sockaddr_in get_peer_name(int fsock);

char* get_peer_str(struct sockaddr_in addr);

int get_input_stdin(const char *prompt, char *buffer, size_t bufsize);

int send_message(int fsock, const char *msg, int len);

int send_string(int socket, const char *msg);

int read_message(int fsock, char *buffer, int len);

int send_packet(int fsock, const packet_t *packet);

int connect_to_server(int fsock, const char *ip_addr_str, int port, int max_retries);

int can_recv(int fsock);

int buffered_read_msg(int fsock, ring_buffer_t *inbuf);

typedef struct
{
    int socket_fd;
    int is_connected;
    ring_buffer_t read_buffer;
    ring_buffer_t write_buffer;
    ring_buffer_t inbox;
    ring_buffer_t outbox;
}
node_conn_t;

// returns 0 on not enough data
// returns < 0 for various error codes
// returns 1 if packet is successfully casted
int cast_buffer_to_packet(packet_t *p, const char *buffer, size_t len);

int spin_conn(node_conn_t *conn);

void init_conn(node_conn_t *conn);

void free_conn(node_conn_t *conn);

void print_conn(const node_conn_t *conn);

typedef struct
{
    int server_fd;
    struct sockaddr_in address;
    node_conn_t *clients;
    size_t client_max;
    size_t client_count;
}
server_t;

int init_server(server_t *s, int client_max);

int free_server(server_t *s);

int spin_server(server_t *server);

int host_localhost_server(server_t *server, int port);

#endif // SPROCKETS_COMMON_PROCS_H
