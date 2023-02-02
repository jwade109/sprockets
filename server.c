#include <stdio.h>
#include <unistd.h>

#include "common.c"

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        printf("Requires IP address and port number.\n");
        printf("usage: %s [address] [port]\n", argv[0]);
        return 1;
    }

    signal(SIGPIPE, SIG_IGN); // suppress SIGPIPE raised by socket write errors

    const char *ip_addr_str = argv[1];
    const int port = atoi(argv[2]);

    int fsock = socket(AF_INET, SOCK_STREAM, 0);
    if (fsock < 0)
    {
        printf("Failed to open socket: %s\n", strerror(errno));
        return 1;
    }

    set_socket_reusable(fsock);

    printf("Starting server on %s:%d\n", ip_addr_str, port);

    struct sockaddr_in addr = get_sockaddr(ip_addr_str, port);

    if (bind(fsock, (struct sockaddr*) &addr, sizeof(addr)) < 0)
    {
        printf("Failed to bind socket: %s\n", strerror(errno));
        return 1;
    }

    if (listen(fsock, 10) < 0)
    {
        printf("Failed to listen: %s\n", strerror(errno));
        return 1;
    }

    printf("Now accepting new connections.\n");
    int addrlen = sizeof(addr);
    int client_fd = accept(fsock, (struct sockaddr*) &addr, &addrlen);
    if (client_fd < 0)
    {
        printf("Failed to connect to client: %s\n", strerror(errno));
        return 1;
    }

    printf("Connected to client.\n");

    input_buffer_t buffer;
    reset_buffer(&buffer);

    while (two_way_loop(client_fd, &buffer) == 0);

    close(fsock);
    printf("Done.\n");

    return 0;
}
