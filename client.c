#include <stdio.h>
#include <unistd.h>

#include "common.c"

int fsock = -1;

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

    fsock = socket(AF_INET, SOCK_STREAM, 0);
    if (fsock < 0)
    {
        printf("Failed to open socket: %s\n", strerror(errno));
        return 1;
    }

    set_socket_reusable(fsock);

    printf("Connecting to server at %s:%d\n", ip_addr_str, port);

    struct sockaddr_in addr = get_sockaddr(ip_addr_str, port);

    if (connect_to_server(fsock, addr) < 0)
    {
        return 1;
    }

    printf("Connected to server.\n");

    input_buffer_t buffer;
    reset_buffer(&buffer);

    while (two_way_loop(fsock, &buffer) == 0);

    close(fsock);
    printf("Done.\n");

    return 0;
}