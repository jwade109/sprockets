#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <signal.h>

#include "common.h"

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
    int client_fd = accept(fsock, (struct sockaddr*) &addr, (socklen_t * restrict) &addrlen);
    if (client_fd < 0)
    {
        printf("Failed to connect to client: %s\n", strerror(errno));
        return -1;
    }

    printf("Connected to client.\n");

    return client_fd;
}

uint32_t next_foreign_serial_no = 0;

typedef struct timeval timeval;

// computes t2 - t1
timeval get_timedelta(const timeval *t1, const timeval *t2)
{
    timeval dt;
    dt.tv_sec = t2->tv_sec - t1->tv_sec;
    dt.tv_usec = t2->tv_usec - t1->tv_usec;
    return dt;
}

int32_t to_usecs(const timeval *t)
{
    return t->tv_sec * 1000000 + t->tv_usec;
}

timeval current_time()
{
    timeval ret;
    gettimeofday(&ret, 0);
    return ret;
}

void user_got_a_packet(const packet_t *p)
{
    timeval tp;
    tp.tv_sec = p->secs;
    tp.tv_usec = p->usecs;

    timeval now = current_time();
    timeval delta_time = get_timedelta(&tp, &now);
    int32_t dt = to_usecs(&delta_time);

    printf("[RECV] ");
    print_packet(*p);
    printf(" [%d us]\n", dt);

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

    int conn_fd = -1;

    if (addr_is_local)
    {
        conn_fd = bind_to_server(fsock, ip_addr_str, port);
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
    }

    node_conn_t conn;
    init_conn(&conn);
    conn.socket_fd = conn_fd;

    int retcode = 0;

    while ((retcode = spin(&conn)) == 0)
    {
        packet_t *p;
        while ((p = ring_get(&conn.inbox)))
        {
            user_got_a_packet(p);
            if (strstr(p->data, "ACK") == 0)
            {
                const int len = 200;
                char buffer[len];
                sprintf(buffer, "ACK #%u 0x%02X", p->sno, p->checksum);
                packet_t ack = get_stamped_packet(buffer);
                ring_put(&conn.outbox, &ack);
            }
        }

        if (1.0 * rand() / RAND_MAX < 0.0001)
        {
            const int len = 200;
            char buffer[len];
            {
                time_t rawtime;
                struct tm *info;
                time(&rawtime);
                info = localtime(&rawtime);
                strftime(buffer, len, "%A, %B %d %FT%TZ", info);
            }

            packet_t out = get_stamped_packet(buffer);
            ring_put(&conn.outbox, &out);
        }
    }

    free_conn(&conn);

    printf("Done; exit code %d.\n", retcode);

    return 0;
}
