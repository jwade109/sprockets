#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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
        char *msg;
        int len = read_message(fsock, &msg);
        if (!msg)
        {
            return 1;
        }

        char ack_buffer[1024] = {0};
        sprintf(ack_buffer, "ACK %d", len);
        if (send_message(fsock, ack_buffer, strlen(ack_buffer)))
        {
            return 1;
        }

        if (is_quit(msg))
        {
            printf("Received exit command.\n");
            break;
        }
    }

    close(fsock);

    printf("Done.\n");

    return 0;
}