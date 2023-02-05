#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include "common.c"

int bind_to_server(int fsock, const char *ip_addr_str, int port)
{
    printf("Starting server on %s:%d\n", ip_addr_str, port);

    struct sockaddr_in addr = get_sockaddr(ip_addr_str, port);

    if (bind(fsock, (struct sockaddr*) &addr, sizeof(addr)) < 0)
    {
        printf("Failed to bind socket: %s\n", strerror(errno));
        return -1;
    }

    if (listen(fsock, 10) < 0)
    {
        printf("Failed to listen: %s\n", strerror(errno));
        return -1;
    }

    printf("Now accepting new connections.\n");
    int addrlen = sizeof(addr);
    int client_fd = accept(fsock, (struct sockaddr*) &addr, &addrlen);
    if (client_fd < 0)
    {
        printf("Failed to connect to client: %s\n", strerror(errno));
        return -1;
    }

    printf("Connected to client.\n");

    return client_fd;
}

uint32_t next_foreign_serial_no = 0;

void user_got_a_packet(const packet_t *p)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    uint64_t nusec = now.tv_sec * 1E6 + now.tv_usec;
    uint64_t pusec = p->secs * 1E6 + p->usecs;
    int64_t dt = nusec - pusec;

    printf("[RECV] ");
    print_packet(*p);
    printf(" [%ld us]\n", dt);

    // if (p->sno != next_foreign_serial_no)
    // {
    //     printf("Serial number mismatch; expected %u, got %u.\n",
    //         next_foreign_serial_no, p->sno);
    // }
    // next_foreign_serial_no = p->sno + 1;
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "-h") == 0)
        {
            printf("Requires IP address and port number.\n");
            printf("usage: %s [address=0.0.0.0] [port=4300]\n", argv[0]);
            return 0;
        }
    }

    srand(time(0));
    signal(SIGPIPE, SIG_IGN); // suppress SIGPIPE raised by socket write errors

    const char *ip_addr_str = argc > 1 ? argv[1] : "0.0.0.0";
    const int port = argc > 2 ? atoi(argv[2]) : 4300;
    const int addr_is_local = strcmp("0.0.0.0", ip_addr_str) == 0;

    int fsock = socket(AF_INET, SOCK_STREAM, 0);
    if (fsock < 0)
    {
        printf("Failed to open socket: %s\n", strerror(errno));
        return 1;
    }

    set_socket_reusable(fsock);

    struct sockaddr_in addr = get_sockaddr(ip_addr_str, port);

    int conn_fd = -1;
    int is_server = 0;

    if (addr_is_local)
    {
        conn_fd = bind_to_server(fsock, ip_addr_str, port);
        is_server = 1;
    }

    if (conn_fd < 0)
    {
        printf("Connecting to server at %s:%d\n", ip_addr_str, port);

        if (connect_to_server(fsock, ip_addr_str, port) < 0)
        {
            return 1;
        }

        conn_fd = fsock;

        printf("Connected to server.\n");
        is_server = 0;
    }

    node_conn_t conn;
    reset_conn(&conn);
    conn.socket_fd = conn_fd;
    conn.on_packet = user_got_a_packet;

    while (spin(&conn) == 0);

    close(conn_fd);
    printf("Done.\n");

    return 0;
}
