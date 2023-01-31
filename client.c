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

    const char *ip_addr_str = argv[1];
    const int port = atoi(argv[2]);

    fsock = socket(AF_INET, SOCK_STREAM, 0);
    if (fsock < 0)
    {
        printf("Failed to open socket: %s\n", strerror(errno));
        return 1;
    }

    int option = 1;
    setsockopt(fsock, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

    printf("Connecting to server at %s:%d\n", ip_addr_str, port);

    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_aton(ip_addr_str, (struct in_addr *) &addr.sin_addr.s_addr);

    int server_fd = -1;
    while (server_fd < 0)
    {
        server_fd = connect(fsock, (struct sockaddr*) &addr, sizeof(addr));
        if (server_fd < 0)
        {
            printf("Failed to connect: %s\n", strerror(errno));
            usleep(1000000);
        }
    }

    printf("Connected to server: %d\n", server_fd);

    while (1)
    {
        packet_t packet;
        if (read_packet(fsock, &packet))
        {
            return 1;
        }

        packet_t resp = get_stamped_packet("ACK");
        if (send_packet(fsock, &resp))
        {
            return 1;
        }
    }

    close(fsock);

    printf("Done.\n");

    return 0;
}