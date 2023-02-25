#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <math.h>

#include "common.h"

typedef struct
{
    int server_fd;
    node_conn_t *clients;
    size_t client_max;
    size_t client_count;
}
server_t;

int open_server(server_t *s, int client_max)
{
    s->client_max = client_max;
    s->client_count = 0;
    s->clients = calloc(client_max, sizeof(node_conn_t));
    s->server_fd = 0;
    return 0;
}

int close_server(server_t *s)
{
    s->client_max = 0;
    s->client_count = 0;
    s->server_fd = 0;
    free(s->clients);
    return 0;
}

void print_server(const server_t *s)
{
    printf("fd=%d, clients=%zu/%zu\n", s->server_fd,
        s->client_count, s->client_max);
    for (size_t i = 0; i < s->client_max; ++i)
    {
        const node_conn_t *nc = s->clients + i;
        if (nc->is_connected)
        {
            print_conn(nc);
        }
    }
}

int spin_server(server_t *server, struct sockaddr_in *address)
{
    // if something happened on the master socket, then it's an incoming connection
    if (can_recv(server->server_fd))
    {
        int addrlen = sizeof(*address);
        int new_socket = accept(server->server_fd, (struct sockaddr *) address, (socklen_t*) &addrlen);

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

        if (send_string(new_socket, "Hello, welcome to Arby's\n"))
        {
            perror("send");
            return -1;
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

            printf("New connection: cid=%zu, fd=%d, %s:%d\n",
                i, new_socket,
                inet_ntoa(address->sin_addr),
                ntohs(address->sin_port));

            ++server->client_count;

            return 1;
        }
    }

    // else its some IO operation on some other socket
    for (size_t i = 0; i < server->client_max; i++)
    {
        node_conn_t *conn = server->clients + i;

        int ret = spin(conn);
        if (ret == -1) // client disconnected
        {
            // client disconnected
            struct sockaddr_in peer_addr = get_peer_name(conn->socket_fd);
            printf("Client disconnected: cid=%zu, fd=%d, %s:%d\n",
                i, conn->socket_fd,
                inet_ntoa(peer_addr.sin_addr),
                ntohs(peer_addr.sin_port));
            free_conn(conn);

            --server->client_count;
        }
    }

    return 1;
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "-h") == 0)
        {
            printf("Requires IP address and port number.\n");
            printf("usage: %s [port=8888]\n", argv[0]);
            return 0;
        }
    }

    const int port = argc > 1 ? atoi(argv[1]) : 8888;

    server_t server;
    open_server(&server, 10);

    server.server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server.server_fd == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (set_socket_reusable(server.server_fd) < 0)
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // bind the socket to localhost port 8888
    if (bind(server.server_fd, (struct sockaddr *)&address, sizeof(address))<0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Listening on %s:%d\n", inet_ntoa(address.sin_addr), port);

    // try to specify maximum of 3 pending connections for the master socket
    if (listen(server.server_fd, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // accept the incoming connection
    printf("Waiting for connections...\n");

    while (1)
    {
        spin_server(&server, &address);
        print_server(&server);
        usleep(50000);
    }

    return 0;
}