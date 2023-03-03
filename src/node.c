#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <signal.h>
#include <arpa/inet.h>

#include "common.h"

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
    print_packet(p);
    printf(" [%d us]\n", dt);
}

int connect_to_upstream(node_conn_t *conn, const char *ipaddr, int port)
{
    conn->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (conn->socket_fd < 0)
    {
        printf("Failed to open socket: %s\n", strerror(errno));
        return -1;
    }
    set_socket_reusable(conn->socket_fd);

    printf("Connecting to server at %s:%d\n", ipaddr, port);

    if (connect_to_server(conn->socket_fd, ipaddr, port, 0) < 0)
    {
        return -1;
    }

    conn->is_connected = 1;

    printf("Connected to server %s\n",
        get_peer_str(get_peer_name(conn->socket_fd)));

    return 0;
}

void print_ring_dump(const ring_buffer_t *buffer)
{
    unsigned char *contig = to_contiguous_buffer(buffer);
    print_hexdump(contig, buffer->size);
    free(contig);
}

void send_packet_at_random(node_conn_t *conn)
{
    if (1.0 * rand() / RAND_MAX < 0.1)
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
        ring_put(&conn->outbox, &out);
    }
}

void send_random_ascii_bytes(node_conn_t *conn)
{
    const unsigned char start = ' ';
    const unsigned char end = '~' + 1;

    if (1.0 * rand() / RAND_MAX < 0.05)
    {
        size_t len = rand() % 300 + 12;
        for (size_t i = 0; i < len; ++i)
        {
            unsigned char c = rand() % (end - start) + start;
            ring_put(&conn->write_buffer, &c);
        }
    }
}

void print_help_info(const char *argv0)
{
    printf("Requires network configuration.\n");
    printf("usage: %s [addrup] [portup] [portdown]\n", argv0);
}

int main(int argc, char **argv)
{
    if (argc < 4)
    {
        print_help_info(argv[0]);
        return 1;
    }
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "-h") == 0)
        {
            print_help_info(argv[0]);
            return 1;
        }
    }

    srand(time(0));

    const char *addrup = argv[1];
    const int portup = atoi(argv[2]);
    const int portdown = atoi(argv[3]);

    node_conn_t upstream;
    init_conn(&upstream);

    if (strcmp(addrup, "-") != 0)
    {
        if (connect_to_upstream(&upstream, addrup, portup) < 0)
        {
            perror("failed to connect to upstream");
            return 1;
        }
    }

    const size_t max_clients = 10;

    server_t server;
    if (init_server(&server, max_clients) < 0)
    {
        perror("open server failed");
        return -1;
    }

    if (host_localhost_server(&server, portdown) < 0)
    {
        perror("failed to init server");
        return -1;
    }

    size_t iter = 0;

    while (1)
    {
        ++iter;
        node_conn_t* connections[max_clients + 1];
        connections[0] = &upstream;
        for (size_t i = 0; i < max_clients; ++i)
        {
            connections[i + 1] = server.clients + i;
        }

        // loop for every connection, both down and upstream
        for (size_t i = 0; i < max_clients + 1; ++i)
        {
            node_conn_t *nc = connections[i];
            if (!nc->is_connected)
            {
                continue;
            }
            // send_packet_at_random(nc);
            send_random_ascii_bytes(nc);
            if (iter % 100 == 0)
            {
                if (i == 0)
                {
                    printf("U  ");
                }
                else
                {
                    printf("D%zu ", i - 1);
                }
                print_conn(nc);
            }
            // print_ring_dump(&nc->read_buffer);
        }

        if (spin_conn(&upstream) < 0)
        {
            perror("upstream disconnected");
            return 1;
        }

        packet_t *p;
        while ((p = ring_get(&upstream.inbox)))
        {
            // do stuff with p
        }

        spin_server(&server);
        usleep(1000);
    }

    printf("Done.\n");

    return 0;
}
