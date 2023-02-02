#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include "common.c"

int main(int argc, char **argv)
{
    if (argc < 4)
    {
        printf("Requires IP address and port number.\n");
        printf("usage: %s [address] [port] [is_server]\n", argv[0]);
        return 1;
    }

    srand(time(0));

    signal(SIGPIPE, SIG_IGN); // suppress SIGPIPE raised by socket write errors

    const char *ip_addr_str = argv[1];
    const int port = atoi(argv[2]);
    const int is_server = atoi(argv[3]);

    int fsock = socket(AF_INET, SOCK_STREAM, 0);
    if (fsock < 0)
    {
        printf("Failed to open socket: %s\n", strerror(errno));
        return 1;
    }

    set_socket_reusable(fsock);

    struct sockaddr_in addr = get_sockaddr(ip_addr_str, port);

    int conn_fd = -1;

    if (is_server)
    {
        printf("Starting server on %s:%d\n", ip_addr_str, port);

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

        conn_fd = client_fd;
    }
    else
    {

        printf("Connecting to server at %s:%d\n", ip_addr_str, port);

        if (connect_to_server(fsock, addr) < 0)
        {
            return 1;
        }

        conn_fd = fsock;

        printf("Connected to server.\n");
    }

    input_buffer_t buffer;
    reset_buffer(&buffer);

    while (two_way_loop(conn_fd, &buffer) == 0);

    close(conn_fd);
    printf("Done.\n");

    return 0;
}
