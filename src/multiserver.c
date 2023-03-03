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
            printf("usage: %s [port=4300]\n", argv[0]);
            return 0;
        }
    }

    const int port = argc > 1 ? atoi(argv[1]) : 4300;

    server_t server;
    if (host_localhost_server(&server, port, 3) < 0)
    {
        perror("failed to init server");
        return -1;
    }

    // accept the incoming connection
    printf("Waiting for connections...\n");

    while (1)
    {
        spin_server(&server);
        print_server(&server);
        usleep(50000);
    }

    return 0;
}