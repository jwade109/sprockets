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