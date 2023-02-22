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

int send_string(int socket, const char *msg)
{
    return send(socket, msg, strlen(msg), 0) != (int) strlen(msg);
}

int main()
{
    const int MAX_CLIENTS = 3;
    int client_socket_descriptors[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        client_socket_descriptors[i] = 0;
    }
    int client_count = 0;

    char buffer[1025];

    int master_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (master_socket == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // set master socket to allow multiple connections
    if (set_socket_reusable(master_socket) < 0)
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    const int port = 8888;
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // bind the socket to localhost port 8888
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Listening on %s:%d\n", inet_ntoa(address.sin_addr), port);

    // try to specify maximum of 3 pending connections for the master socket
    if (listen(master_socket, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // accept the incoming connection
    int addrlen = sizeof(address);
    puts("Waiting for connections...");

    while (1)
    {
        fd_set readfds;

        // clear the socket set
        FD_ZERO(&readfds);

        // add master socket to set
        FD_SET(master_socket, &readfds);
        int max_sd = master_socket;

        // add child sockets to set
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            //socket descriptor
            int sd = client_socket_descriptors[i];
            if (sd == 0)
            {
                continue;
            }

            FD_SET(sd, &readfds);
            max_sd = (int) fmax(max_sd, sd);
        }

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 2000;

        // TODO add a timeout for this. blocking calls are big no-no
        int select_ret = select(max_sd + 1, &readfds, 0, 0, &timeout);
        if (select_ret < 0 && errno != EINTR)
        {
            printf("select error");
        }

        if (select_ret == 0)
        {
            // nothing to see here
            continue;
        }

        // if something happened on the master socket, then it's an incoming connection
        if (FD_ISSET(master_socket, &readfds))
        {
            int new_socket = accept(master_socket, (struct sockaddr *) &address, (socklen_t*) &addrlen);

            if (new_socket < 0)
            {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            if (client_count >= MAX_CLIENTS)
            {
                printf("Can't accept any more connections.\n");
                send_string(new_socket, "CLIENT LIMIT REACHED\n");
                close(new_socket);
                continue;
            }

            if (send_string(new_socket, "Hello, welcome to Arby's\n"))
            {
                perror("send");
            }

            // add new socket to array of sockets
            for (int i = 0; i < MAX_CLIENTS; i++)
            {
                if (client_socket_descriptors[i])
                {
                    continue;
                }

                client_socket_descriptors[i] = new_socket;
                printf("New connection: cid=%d, fd=%d, %s:%d\n",
                    i, new_socket,
                    inet_ntoa(address.sin_addr),
                    ntohs(address.sin_port));

                ++client_count;
                printf("Clients: %d\n", client_count);

                break;
            }
        }

        // else its some IO operation on some other socket
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            int sd = client_socket_descriptors[i];

            if (!FD_ISSET(sd, &readfds))
            {
                // nothing to do for this client
                continue;
            }

            int valread = 0;

            // oh boy, something happened
            if ((valread = read(sd, buffer, 1024)) == 0)
            {
                // client disconnected
                struct sockaddr_in peer_addr = get_peer_name(sd);
                printf("Client disconnected: cid=%d, fd=%d, %s:%d\n", i, sd,
                    inet_ntoa(peer_addr.sin_addr),
                    ntohs(peer_addr.sin_port));
                close(sd);
                client_socket_descriptors[i] = 0;
                --client_count;
                printf("Clients: %d\n", client_count);
                continue;
            }

            // got a message from the client
            buffer[valread] = '\0';
            printf("[#%d] %d bytes\n", i, valread);
            print_hexdump(buffer, valread);
            if (send_string(sd, buffer))
            {
                perror("send");
                exit(EXIT_FAILURE);
            }
        }
    }

    return 0;
}