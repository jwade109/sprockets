#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <arpa/inet.h>

#include <common.h>
#include <datetime.h>

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

void send_test_packet(node_conn_t *conn)
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

    packet_t out = get_empty_packet(conn->packet_counter++);
    out.data = malloc(strlen(buffer));
    memcpy(out.data, buffer, strlen(buffer));
    out.datalen = strlen(buffer);
    datetime now = current_time();
    out.secs = now.tv_sec;
    out.usecs = now.tv_usec;

    ring_put(&conn->outbox, &out);
}

void send_random_bytes(node_conn_t *conn)
{
    size_t len = rand() % 300 + 12;
    uint8_t *buffer = malloc(len);
    for (size_t i = 0; i < len; ++i)
    {
        buffer[i] = rand() % 256;
    }

    packet_t out = get_empty_packet(conn->packet_counter++);
    out.data = buffer;
    out.datalen = len;
    datetime now = current_time();
    out.secs = now.tv_sec;
    out.usecs = now.tv_usec;
    set_checksum(&out);

    ring_put(&conn->outbox, &out);
}

void send_plaintext_date(node_conn_t *conn)
{
    if (1.0 * rand() / RAND_MAX < 0.01)
    {
        const int len = 400;
        char buffer[len];
        {
            time_t rawtime;
            struct tm *info;
            time(&rawtime);
            info = localtime(&rawtime);
            strftime(buffer, len, "Hello, welcome to Arby's. %A, %B %d %FT%TZ\n", info);
        }
        for (size_t i = 0; i < strlen(buffer); ++i)
        {
            ring_put(&conn->write_buffer, buffer + i);
        }
    }
}

node_conn_t** get_all_connections(node_conn_t *up, server_t *down)
{
    node_conn_t** conns = malloc((down->client_max + 1) * sizeof(node_conn_t*));
    if (!conns)
    {
        return 0;
    }
    conns[0] = up;
    for (size_t i = 0; i < down->client_max; ++i)
    {
        conns[i + 1] = down->clients + i;
    }
    return conns;
}

void print_help_info(const char *argv0)
{
    printf("Requires network configuration.\n");
    printf("usage: %s [addrup] [portup] [portdown]\n", argv0);
}

int program_exit = 0;

void sighandler(int signal)
{
    printf("Signal: %d\n", signal);
    program_exit = 1;
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
    signal(SIGINT, sighandler);
    // signal(SIGPIPE, sighandler);

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
            free_conn(&upstream);
            return 1;
        }
    }

    const size_t max_clients = 3;

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

    rate_limit send_test_messages, update_loop;
    init_rate_limit(&send_test_messages, 20);
    init_rate_limit(&update_loop, 100);

    node_conn_t** conns = get_all_connections(&upstream, &server);

    while (!program_exit)
    {
        datetime now = current_time();

        if (poll_rate(&send_test_messages, &now))
        {
            for (size_t i = 0; i < max_clients + 1; ++i)
            {
                node_conn_t *nc = conns[i];
                if (!nc->is_connected)
                {
                    continue;
                }
                // send_test_packet(nc);
                send_random_bytes(nc);
            }
        }

        if (poll_rate(&update_loop, &now))
        {
            for (size_t i = 0; i < max_clients + 1; ++i)
            {
                node_conn_t *nc = conns[i];
                if (!nc->is_connected)
                {
                    continue;
                }

                packet_t *p;
                while ((p = ring_get(&nc->inbox)))
                {
                    // printf("%s: ", get_peer_str(get_peer_name(nc->socket_fd)));
                    printf("R: ");
                    print_packet(p);
                    free(p->data);
                }
            }

            if (spin_conn(&upstream) < 0)
            {
                perror("upstream disconnected");
                break;
            }

            spin_server(&server);
        }

        usleep(2000); // 2 ms
    }

    free(conns);
    free_server(&server);
    free_conn(&upstream);

    printf("Done.\n");

    return 0;
}
