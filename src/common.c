#include <stdlib.h>
#include <ctype.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
#include <signal.h>
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <ring_buffer.h>

#include <common.h>

uint8_t array_sum(const char *array, size_t len)
{
    uint8_t sum = 0;
    for (size_t i = 0; i < len; ++i)
    {
        sum += (uint8_t) array[i];
    }
    return sum;
}

void print_packet(const packet_t *p)
{
    printf("%u.%06u #%d type=%d chk=%02x len=%d d=%p\n",
        p->secs, p->usecs, p->sno,
        p->datatype, p->checksum, p->datalen, p->data);
    // if (p->data)
    // {
    //     print_hexdump(p->data, p->datalen);
    // }
}

packet_t get_empty_packet(size_t sno)
{
    packet_t ret;
    memset(&ret, 0, sizeof(ret));
    ret.preamble = PACKET_PREAMBLE;
    ret.sno = sno;
    return ret;
}

uint8_t compute_checksum(const packet_t *p)
{
    uint8_t sum = 0;
    const uint8_t *data = (const uint8_t*) p;
    for (size_t i = 0; i < PACKET_HEADER_SIZE; ++i)
    {
        sum += data[i];
    }
    for (size_t i = 0; i < p->datalen; ++i)
    {
        sum += p->data[i];
    }
    return sum;
}

void set_checksum(packet_t *p)
{
    p->checksum = 256 - compute_checksum(p);
}

void print_hexdump(const uint8_t *seq, size_t len)
{
    if (!len)
    {
        return;
    }

    size_t i = 0, j = 0;

    while (i < len)
    {
        printf("%06zu | ", i);
        for (size_t r = 0; r < 16; ++r, ++i)
        {
            if (i < len)
            {
                printf("%02x ", seq[i]);
            }
            else
            {
                printf("   ");
            }
        }
        printf("| ");
        for (size_t r = 0; r < 16; ++r, ++j)
        {
            if (j < len)
            {
                unsigned char c = seq[j];
                if (c >= ' ' && c <= '~')
                {
                    printf("%c", c);
                }
                else
                {
                    printf(".");
                }
            }
            else
            {
                printf(" ");
            }
        }
        printf(" |\n");
    }
}

int set_socket_reusable(int fsock)
{
    int option = 1;
    return setsockopt(fsock, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
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

struct sockaddr_in get_peer_name(int fsock)
{
    struct sockaddr_in ret = {0};
    unsigned int len = sizeof(ret);
    getpeername(fsock, (struct sockaddr*) &ret, &len);
    return ret;
}

char* get_peer_str(struct sockaddr_in addr)
{
    static char buffer[300];
    sprintf(buffer, "%s:%d",
        inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
    return buffer;
}

int send_message(int fsock, const char *msg, int len)
{
    if (len < 1)
    {
        printf("Cannot send 0 characters.\n");
        return 1;
    }

    int sent = send(fsock, msg, len, MSG_NOSIGNAL);
    if (sent < 0 || len != sent)
    {
        printf("Failed to send: %s\n", strerror(errno));
        return 1;
    }
    return 0;
}

int send_string(int socket, const char *msg)
{
    return send_message(socket, msg, strlen(msg));
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

int connect_to_server(int fsock, const char *ip_addr_str, int port, int max_retries)
{
    struct sockaddr_in addr = get_sockaddr(ip_addr_str, port);

    int retcode = -1;
    int retries = 0;
    while (retcode < 0 && retries <= max_retries)
    {
        if (!retries)
        {
            printf("Attempting connection...\n");
        }
        retcode = connect(fsock, (struct sockaddr*) &addr, sizeof(addr));
        if (retcode < 0)
        {
            if (!retries && max_retries)
            {
                printf("Failed to connect: %s. "
                    "Will wait and retry for a little bit.\n",
                    strerror(errno));
            }
            if (max_retries)
            {
                usleep(100000);
            }
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

int buffered_read_msg(int fsock, ring_buffer_t *inbuf)
{
    const int buffer_size = inbuf->capacity;
    char buffer[buffer_size];

    int numread = read_message(fsock, buffer, buffer_size);
    for (int i = 0; i < numread; ++i)
    {
        ring_put(inbuf, (void *) &buffer[i]);
    }
    return numread;
}

size_t allocated = 0;

// returns 0 on not enough data
// returns < 0 for various error codes
// returns 1 if packet is successfully casted
int cast_buffer_to_packet(packet_t *p, const uint8_t *buffer, size_t len)
{
    if (len < PACKET_HEADER_SIZE)
    {
        return 0;
    }

    // only populate the header; else we risk a buffer overrun
    memcpy(p, buffer, PACKET_HEADER_SIZE);

    if (p->preamble != PACKET_PREAMBLE)
    {
        // printf("Bad preamble; expected 0x%X, got 0x%X.\n",
        //     PACKET_PREAMBLE, p->preamble);
        return -1;
    }

    if (len - PACKET_HEADER_SIZE < p->datalen)
    {
        // printf("Not enough data; datalen is %u, %zu remaining.\n",
        //     p->datalen, len - PACKET_HEADER_SIZE);
        return -2;
    }

    p->data = malloc(p->datalen);
    allocated += p->datalen;
    memcpy(p->data, buffer + PACKET_HEADER_SIZE, p->datalen);

    uint8_t checksum = compute_checksum(p);

    if (checksum)
    {
        printf("Nonzero checksum %d\n", checksum);
        // return -3;
    }

    return 1;
}

int pop_packet_if_exists(ring_buffer_t *buffer, packet_t *packet)
{
    if (!buffer->size)
    {
        return -1; // empty buffers contain no packets
    }

    while (1)
    {
        uint8_t* contig = to_contiguous_buffer(buffer);
        int status = cast_buffer_to_packet(packet, contig, buffer->size);
        free(contig);
        if (status < 0)
        {
            // pop a single byte off the buffer and try again
            ring_get(buffer);
        }
        else if (status == 0)
        {
            // not enough data in the buffer
            return 0;
        }
        else if (status == 1)
        {
            // got a packet
            for (size_t i = 0; i < PACKET_HEADER_SIZE + packet->datalen; ++i)
            {
                ring_get(buffer);
            }
            return 1;
        }
    }

    return 0;
}

int spin_conn(node_conn_t *conn)
{
    if (!conn->is_connected)
    {
        return 0;
    }

    int retcode = 1;

    if (can_recv(conn->socket_fd))
    {
        int len = buffered_read_msg(conn->socket_fd, &conn->read_buffer);
        if (!len)
        {
            conn->is_connected = 0;
            close(conn->socket_fd);
            printf("Closing.\n");
            retcode = -1;
        }
    }

    packet_t packet;
    while (pop_packet_if_exists(&conn->read_buffer, &packet) > 0)
    {
        ring_put(&conn->inbox, &packet);
    }

    while (conn->outbox.size)
    {
        packet_t *out = ring_get(&conn->outbox);

        for (size_t i = 0; i < PACKET_HEADER_SIZE; ++i)
        {
            ring_put(&conn->write_buffer, ((uint8_t*) out) + i);
        }

        for (size_t i = 0; i < out->datalen; ++i)
        {
            ring_put(&conn->write_buffer, out->data + i);
        }

        free(out->data);
    }

    while (conn->write_buffer.size)
    {
        // todo get contiguous regions of ring buffer
        const char *c = ring_get(&conn->write_buffer);
        if (send_message(conn->socket_fd, c, 1))
        {
            printf("Failed to send.\n");
            retcode = -1;
            break;
        }
    }

    return retcode;
}

void init_conn(node_conn_t *conn)
{
    conn->socket_fd = 0;
    conn->is_connected = 0;
    conn->packet_counter = 0;
    init_buffer(&conn->read_buffer,  sizeof(unsigned char), 3000);
    init_buffer(&conn->write_buffer, sizeof(unsigned char), 3000);
    init_buffer(&conn->inbox,  sizeof(packet_t), 100);
    init_buffer(&conn->outbox, sizeof(packet_t), 100);
}

void free_conn(node_conn_t *conn)
{
    conn->socket_fd = 0;
    conn->is_connected = 0;
    conn->packet_counter = 0;
    free_buffer(&conn->read_buffer);
    free_buffer(&conn->write_buffer);

    packet_t *p;
    while ((p = ring_get(&conn->inbox)))
    {
        free(p->data);
    }
    while ((p = ring_get(&conn->outbox)))
    {
        free(p->data);
    }

    free_buffer(&conn->inbox);
    free_buffer(&conn->outbox);
}

void print_conn(const node_conn_t *conn)
{
    printf("%s r: ", get_peer_str(get_peer_name(conn->socket_fd)));
    print_ring_buffer(&conn->read_buffer);
    printf(" w: ");
    print_ring_buffer(&conn->write_buffer);
    printf(" i: ");
    print_ring_buffer(&conn->inbox);
    printf(" o: ");
    print_ring_buffer(&conn->outbox);
    printf("\n");
}

int init_server(server_t *s, int client_max)
{
    s->client_max = client_max;
    s->client_count = 0;
    s->clients = calloc(client_max, sizeof(node_conn_t));
    s->server_fd = 0;
    return 0;
}

int free_server(server_t *s)
{
    s->client_max = 0;
    s->client_count = 0;
    s->server_fd = 0;
    for (size_t i = 0; i < s->client_max; ++i)
    {
        free_conn(s->clients + i);
    }
    free(s->clients);
    return 0;
}

int spin_server(server_t *server)
{
    // if something happened on the master socket, then it's an incoming connection
    if (can_recv(server->server_fd))
    {
        struct sockaddr_in client_addr;
        int addrlen = sizeof(client_addr);
        int new_socket = accept(server->server_fd,
            (struct sockaddr *) &client_addr, (socklen_t*) &addrlen);

        if (new_socket < 0)
        {
            perror("accept");
            return -1;
        }

        if (server->client_count >= server->client_max)
        {
            printf("Can't accept any more connections.\n");
            send_string(new_socket, "CLIENT LIMIT REACHED\n");
            close(new_socket);
            return 1;
        }

        // add new socket to array of sockets
        for (size_t i = 0; i < server->client_max; i++)
        {
            if (server->clients[i].is_connected)
            {
                continue;
            }

            init_conn(&server->clients[i]);
            server->clients[i].socket_fd = new_socket;
            server->clients[i].is_connected = 1;

            const char *greeting = "Hello, welcome to Arby's\n";

            for (size_t j = 0; j < strlen(greeting); ++j)
            {
                ring_put(&server->clients[i].write_buffer, greeting + j);
            }

            printf("New connection: cid=%zu, fd=%d, %s\n",
                i, new_socket, get_peer_str(get_peer_name(new_socket)));

            ++server->client_count;

            return 1;
        }
    }

    // else its some IO operation on some other socket
    for (size_t i = 0; i < server->client_max; i++)
    {
        node_conn_t *conn = server->clients + i;

        int ret = spin_conn(conn);
        if (ret == -1) // client disconnected
        {
            // client disconnected
            printf("Client disconnected: cid=%zu, fd=%d\n",
                i, conn->socket_fd);
            free_conn(conn);

            --server->client_count;
        }
    }

    return 1;
}

int host_localhost_server(server_t *server, int port)
{
    server->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->server_fd == 0)
    {
        perror("socket failed");
        return -1;
    }

    if (set_socket_reusable(server->server_fd) < 0)
    {
        perror("setsockopt");
        return -1;
    }

    server->address.sin_family = AF_INET;
    server->address.sin_addr.s_addr = INADDR_ANY;
    server->address.sin_port = htons(port);

    if (bind(server->server_fd, (const struct sockaddr *) &server->address,
        sizeof(server->address)) < 0)
    {
        perror("bind failed");
        return -1;
    }
    printf("Listening on %s:%d\n", inet_ntoa(server->address.sin_addr), port);

    // try to specify maximum of 3 pending connections for the master socket
    if (listen(server->server_fd, 3) < 0)
    {
        perror("listen");
        return -1;
    }

    return 0;
}
