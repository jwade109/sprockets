#include <stdio.h>
#include <unistd.h>

#include "common.c"

int main()
{
    int fsock = socket(AF_INET, SOCK_STREAM, 0);
    if (fsock < 0)
    {
        printf("Failed to open socket: %s\n", strerror(errno));
        return 1;
    }

    int option = 1;
    setsockopt(fsock, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

    const char *ip_addr_str = "0.0.0.0";
    const int port = 4300;

    printf("Starting server on %s:%d\n", ip_addr_str, port);

    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_aton(ip_addr_str, (struct in_addr *) &addr.sin_addr.s_addr);

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

    printf("Connected: %d\n", client_fd);

    packet_t hello_packet = get_stamped_packet("hello!");
    if (send_packet(client_fd, &hello_packet))
    {
        return 1;
    }

    packet_t hello_ack;
    if (read_packet(client_fd, &hello_ack))
    {
        return 1;
    }

    char buffer[1024];
    while (1)
    {
        int len = get_input_stdin(">> ", buffer, sizeof(buffer));
        if (len < 0)
        {
            printf("Failed to get stdin: %s\n", strerror(errno));
            return 1;
        }

        packet_t packet = get_stamped_packet(buffer);
        if (send_packet(client_fd, &packet))
        {
            return 1;
        }

        packet_t resp;
        if (read_packet(client_fd, &resp))
        {
            return 1;
        }
    }

    close(fsock);

    printf("Done.\n");

    return 0;
}
