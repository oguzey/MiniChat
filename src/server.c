#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include "wrsock.h"
#include "vector.h"
#include "logger.h"

#define DEBUG       1
#define SERVER_PORT 4040

#define MAX_AMOUNT_CLIENTS FD_SETSIZE

typedef enum {
    UNDEFINED = 0,
    CONNECT,
    QUIT,
    WHO,
    KILL,
    SHUTDOWN
} ServerCommand;

typedef struct {
    char *name;
    int connected;
    Connection *conn;
} Client;

static char* get_command_argument(char *a_data);
static inline void send_broadcast_msg(char *a_message);
static void collect_all_names(void *a_item, void *a_names);
static int send_msg(Client *a_client, const char *a_message);
static void send_msg_if_connected(Client *a_client, char *a_message);

static volatile int _s_runing = -1;
static Vector *_s_clients = NULL;
static ConnectionType _s_con_type = -1;
static int _s_server_port = -1;


static void args_parse(int argc, char **argv)
{

#define print_help()                                                            \
    fprintf(stdout, "Usage: %s \n"                                              \
            "\t -t <TYPE> \t type of connection (UDP or TCP) (mandatory);\n"    \
            "\t -p <PORT> \t port of server (optional), use %d as default.\n",  \
            argv[0], SERVER_PORT);                                              \
    exit(EXIT_SUCCESS);

    int c;
    while ((c = getopt (argc, argv, "ht:p:")) != -1) {
        switch (c) {
        case 'h':
            print_help();
        break;
        case 't':
            if (strcasecmp(optarg, "tcp") == 0) {
                _s_con_type = TCP;
            } else if (strcasecmp(optarg, "udp") == 0) {
                _s_con_type = UDP;
            } else {
                fatal("Option -t requires an argument. See help.");
            }
        break;
        case 'p':
            _s_server_port = atoi(optarg);
            if (_s_server_port < 0 || _s_server_port > 65535) {
                info("Bad port provided '%d'. Use default %d.", _s_server_port, SERVER_PORT);
                _s_server_port = SERVER_PORT;
            }
        break;
        case '?':
            warn("Bad option -%c provided.", optopt);
            print_help();
        break;
        default:
            print_help();
        }
    }
}

static Client* client_create(Connection *a_conn)
{
    assert(a_conn);

    Client *c = malloc(sizeof(Client));
    c->name = NULL;
    c->connected = 0;
    c->conn = a_conn;
    return c;
}

static void client_destroy(Client *a_client)
{
    if (a_client) {
        free(a_client->name);
        connection_destroy(a_client->conn);
        free(a_client);
    }
}

static int cmp_clients_by_ptr(void *a_item, void *a_pattern)
{
    return !(a_item == a_pattern);
}

int cmp_clients_by_name(void *a_item, void *a_pattern)
{
    Client *cl_item = a_item;
    char *pat_name = a_pattern;
    if (cl_item->name == NULL && pat_name == NULL) { // TODO: Do we realy need this?
        return 0;
    } else if (cl_item->name && pat_name
                    && strcmp(cl_item->name, pat_name) == 0) {
        return 0;
    }
    return 1;
}

int cmp_clients_by_addr(void *a_item, void *a_pattern)
{
    Client *cl_item = a_item;
    struct sockaddr_in *pat_addr = a_pattern;

    in_addr_t ip = connection_get_address(cl_item->conn);
    int port = connection_get_port(cl_item->conn);

    if (port == pat_addr->sin_port && ip == pat_addr->sin_addr.s_addr) {
        return 0;
    }
    return 1;
}


static ServerCommand parse_user_command(char *a_str)
{
    ServerCommand ret = UNDEFINED;
    if (strncmp(a_str, "_kill", 5) == 0) {
        ret = KILL;
    } else if (strncmp(a_str, "_shutdown", 9) == 0) {
        ret = SHUTDOWN;
    } else if (strncmp(a_str, "_who", 4) == 0) {
        ret = WHO;
    }
    return ret;
}

static ServerCommand parse_client_command(char *a_str)
{
    ServerCommand ret = UNDEFINED;
    if (strncmp(a_str, "_connect", 8) == 0) {
        ret = CONNECT;
    } else if (strncmp(a_str, "_quit", 5) == 0) {
        ret = QUIT;
    } else if (strncmp(a_str, "_who", 4) == 0) {
        ret = WHO;
    }
    return ret;
}

void receive_user_data(void)
{
    char buf[BUF_SIZE] = {0};
    char message[BUF_SIZE] = {0};
    char *name;
    Client *client;

    int read_bytes = read(0, buf, BUF_SIZE);

    assert(read_bytes < BUF_SIZE);
    if (read_bytes == -1) {
        warn("Could not read from STDIN data.");
        return;
    }
    if (read_bytes == 1) {
        /* read only LF symbol */
        info("Could not send empty message.");
        return;
    }
    /* remove last LF symbol readed from stdin */
    buf[read_bytes - 1] = '\0';

    switch(parse_user_command(buf)) {
    case KILL:
        name = get_command_argument(buf);
        if (!name) {
            warn("Name of client should be provided.");
            break;
        }
        client = vector_delete_first_equal(_s_clients, name, cmp_clients_by_name);
        info("Server has '%d' clients.", vector_total(_s_clients));
        if (client) {
            sprintf(message, "The client '%s' left the chat.", client->name);
            send_broadcast_msg(message);
            info("%s", message);
            /* send info data to client */
            sprintf(message, "server: You was killed.");
            send_msg(client, message);
            client_destroy(client);
        } else {
            info("Client '%s' was not found.", name);
        }
    break;
    case SHUTDOWN:
        sprintf(message, "The server is going shutdown. Bye.");
        send_broadcast_msg(message);
        _s_runing = 0;
        info("%s", message);
    break;
    case WHO:
        vector_foreach(_s_clients, message, collect_all_names);
        info("All clients: '%s'", message);
    break;
    default:
        warn("Unknown command. Allowed commands: _kill <name>, _shutdown, _who");
    break;
    }
}

static int send_msg(Client *a_client, const char *a_message)
{
    assert(a_client);
    return connection_send(a_client->conn, a_message, strlen(a_message));
}

static void send_msg_if_connected(Client *a_client, char *a_message)
{
    if (a_client->connected) {
        send_msg(a_client, a_message);
    }
}

static inline void send_broadcast_msg(char *a_message)
{
    vector_foreach(_s_clients, a_message,
                   (void (*)(void *, void *))send_msg_if_connected);
}

static void collect_all_names(void *a_item, void *a_names)
{
    Client *client = a_item;
    char *all = a_names;
    sprintf(all, "%s %s", all, client->name);
}

static char* get_command_argument(char *a_data)
{
    char *arg;
    char *ret = NULL;
    size_t len;
    strtok (a_data, " ");
    arg = strtok (NULL, " ");
    if (arg && (len = strlen(arg)) > 0) {
        ret = malloc(len + 1);
        strncpy(ret, arg, len);
        ret[len] = '\0';
    }
    return ret;
}

void verificate_name(char **a_name)
{
    char message[BUF_SIZE];
    char *new_name = NULL;
    size_t length;
    int was_used = 0;
    sprintf(message, "Client with name '%s' is connecting.", *a_name);

    while (vector_is_contains(_s_clients, *a_name, cmp_clients_by_name) == 1) {
        was_used = 1;
        length = sizeof(*a_name);
        new_name = malloc(length + 1);
        sprintf(new_name, "%s_", *a_name);
        free(*a_name);
        *a_name = new_name;
    }
    if (was_used) {
        info("%s", message);
        info("Client '%s' already exist. Set new name '%s'.", *a_name, new_name);
    }
}

void handle_client_data(Client *a_client, char *a_data, const char *a_address)
{
    char *name = NULL;
    char message[BUF_SIZE] = {0};
    char buf[BUF_SIZE] = {0};

    ServerCommand client_cmd = parse_client_command(a_data);

    if (a_client->connected == 0 && client_cmd != CONNECT) {
        warn("Client with addres '%s' is not connected."
                        "Message '%s' not allowed.", a_address, a_data);
        return;
    } else if (a_client->connected == 1 && client_cmd == CONNECT) {
        warn("Cannot add client from address '%s'. Client with the same"
               "address already connected.", a_address);
        return;
    }

    switch (client_cmd) {
    case CONNECT:
        name = get_command_argument(a_data);
        verificate_name(&name);
        a_client->name = name;
        sprintf(message, "New client '%s' was connected.", a_client->name);
        send_broadcast_msg(message);
        info("%s", message);
        a_client->connected = 1;
    break;
    case QUIT:
        vector_delete_first_equal(_s_clients, a_client, cmp_clients_by_ptr);
        sprintf(message, "The client '%s' left the chat.", a_client->name);
        send_broadcast_msg(message);
        info("%s", message);
        client_destroy(a_client);
        break;
    case WHO:
        vector_foreach(_s_clients, buf, collect_all_names);
        sprintf(message, "All clients: %s", buf);
        info("Send info about all clients to '%s'.", a_client->name);
        send_msg(a_client, message);
        break;
    case UNDEFINED:
        /* send received message to all clients */
        info("Send '%s' to all clients from '%s'.", a_data, a_client->name);
        sprintf(message, "%s: %s", a_client->name, a_data);
        send_broadcast_msg(message);
        break;
    default:
        assert("Unknown command " == NULL);
        break;
    }
}

static void receive_tcp_client_data(Client *a_client)
{
    char buf[BUF_SIZE] = {0};
    char address[64] = {0};
    int res;

    res = connection_tcp_receive(a_client->conn, buf, BUF_SIZE);
    if (res) {
        vector_delete_first_equal(_s_clients, a_client, cmp_clients_by_ptr);
        char message[BUF_SIZE] = {0};
        sprintf(message, "Something happened. The client '%s' left the chat.",
                a_client->name);
        send_broadcast_msg(message);
        info("%s", message);
        client_destroy(a_client);
        return;
    }

    sprintf(address, "%s:%d", connection_get_address_str(a_client->conn),
            connection_get_port(a_client->conn));

    info("From address '%s' recived message '%s'", address, buf);

    handle_client_data(a_client, buf, address);
}

void handle_server_event(Connection *a_server)
{
    Client *client = NULL;

    if (connection_get_type(a_server) == TCP) {
        Connection *conn = connection_tcp_accept(connection_get_fd(a_server));
        if (conn) {
            client = client_create(conn);
            vector_add(_s_clients, client);
        }
    } else {
        char buf[BUF_SIZE] = {0};
        char straddr[BUF_SIZE] = {0};
        struct sockaddr_in *addr = NULL;
        connection_udp_receive_with_addr(a_server, buf, BUF_SIZE, (struct sockaddr **)&addr);
        assert(addr);
        client = vector_get_first_equal(_s_clients, addr, cmp_clients_by_addr);
        if (client == NULL) {
            Connection *cl_conn = connection_create_raw(UDP, connection_get_fd(a_server), addr);
            client = client_create(cl_conn);
            vector_add(_s_clients, client);
        }
        sprintf(straddr, "%s:%d", inet_ntoa(addr->sin_addr), addr->sin_port);
        handle_client_data(client, buf, straddr);
    }
}

int main(int argc, char** argv)
{
    Connection *server = NULL;
    Client *client = NULL;
    fd_set readf;
    int maxfd;
    int server_socket;
    int client_fd;
    int i;
    int ready;
    int amount;

    args_parse(argc, argv);

    if (_s_server_port < 0 || _s_server_port > 65535) {
        _s_server_port = SERVER_PORT;
    }
    if (_s_con_type == TCP) {
        info("Server will use TCP connection.");
    } else if (_s_con_type == UDP) {
        info("Server will use UDP connection.");
    } else {
       fatal("You should provide connection type.");
    }

    server = connection_server_create(_s_con_type, "0.0.0.0", _s_server_port,
                                      MAX_AMOUNT_CLIENTS);
    server_socket = connection_get_fd(server);
    _s_clients = vector_create();

    info("Server has started on port '%d'.", _s_server_port);
    info("Allowed commands:"
           "\n\t1) _who"
           "\n\t2) _kill <name>"
           "\n\t3) _shutdown");

    _s_runing = 1;

    while(_s_runing) {
        FD_ZERO(&readf);
        maxfd = server_socket;
        if (connection_get_type(server) == TCP) {
            for (i = 0; i < vector_total(_s_clients); ++i) {
                client = vector_get(_s_clients, i);
                client_fd = connection_get_fd(client->conn);
                FD_SET(client_fd, &readf);
                if (client_fd > maxfd) {
                    maxfd = client_fd;
                }
            }
        }
        FD_SET(server_socket, &readf);
        FD_SET(0, &readf);
        ready = select(maxfd + 1, &readf, 0, 0, 0);

        /* read data from user terminal */
        if (FD_ISSET(0, &readf)) {
            receive_user_data();
            if (--ready <= 0) {
                /* no more descriptors for handling */
                continue;
            }
        }
        /* got something on server connection */
        if (FD_ISSET(server_socket, &readf)) {
            /* new client connection */
            handle_server_event(server);
            if (--ready <= 0) {
                /* no more descriptors for handling */
                continue;
            }
        }
        if (connection_get_type(server) == TCP) {
            /* read data from clients */
            amount = vector_total(_s_clients);
            for (i = 0; i <= amount; ++i) {
                /* check all clients for data */
                client = vector_get(_s_clients, i);
                if (client == NULL) {
                    assert("Client is NULL" == NULL);
                    continue;
                }
                if (FD_ISSET(connection_get_fd(client->conn), &readf)) {
                    receive_tcp_client_data(client);
                    if (--ready <= 0) {
                        /* no more descriptors for handling */
                        break;
                    }
                }
            }
        }
    }
    info("Close all sockets and removing all clients...");
    close(server_socket);
    amount = vector_total(_s_clients);
    for (i = 0; i <= amount; ++i) {
        if ((client = vector_get(_s_clients, i)) != NULL) {
            close(connection_get_fd(client->conn));
            client_destroy(client);
        };
    }
    vector_free(_s_clients);
    info("Done.");
}
