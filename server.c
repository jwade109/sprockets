#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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

    while (1)
    {
        // get_input_stdin(">> ");
        // packet_t packet = get_stamped_packet();
        // char *msg = (char *) &packet;
        // int len = sizeof(packet_t);

        char *msg = get_input_stdin(">> ");
        if (!msg)
        {
            printf("Failed to get stdin: %s\n", strerror(errno));
            return 1;
        }
        int len = strlen(msg);

        packet_t packet = get_stamped_packet();
        memcpy(packet.data, msg, len);
        packet.checksum = compute_checksum((uint8_t *) &packet, sizeof(packet));

        msg = (char *) &packet;
        len = sizeof(packet);

        if (send_message(client_fd, msg, len))
        {
            return 1;
        }

        char *resp;
        read_message(client_fd, &resp);
        if (!resp)
        {
            return 1;
        }

        if (is_quit(msg))
        {
            printf("Received exit command.\n");
            break;
        }

        // free(msg);
    }

    close(fsock);

    printf("Done.\n");

    return 0;
}