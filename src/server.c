#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
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
    Connection *conn;
    int connected;
} Client;

static char* get_command_argument(char *a_data);
static void send_broadcast_msg(void *a_client, void *a_message);
static void collect_all_names(void *item, void *names);
static int send_msg(Client *a_client, char *a_message);

static volatile int _s_runing = -1;
static Vector *_s_clients = NULL;

static Client* client_create(Connection *conn)
{
    Client *c = malloc(sizeof(Client));
    c->name = NULL;
    c->conn = conn;
    c->connected = 0;
    return c;
}

static void client_destroy(Client *client)
{
    if (client) {
        free(client->name);
        connection_destroy(client->conn);
        free(client);
    }
}

static int cmp_clients_by_ptr(void *item, void *pattern)
{
    return !(item == pattern);
}

int cmp_clients_by_name(void *item, void *pattern)
{
    Client *cl_item = item;
    char *pat_name = pattern;
    if (cl_item->name == NULL && pat_name == NULL) {
        return 0;
    } else if (cl_item->name && pat_name
                    && strcmp(cl_item->name, pat_name) == 0) {
        return 0;
    }
    return 1;
}

int cmp_clients_by_addr(void *item, void *pattern)
{
    Client *cl_item = item;
    struct sockaddr_in *pat_addr = pattern;
    if (cl_item->address->sin_port == pat_addr->sin_port
            && cl_item->address->sin_addr.s_addr == pat_addr->sin_addr.s_addr) {
        return 0;
    }
    return 1;
}



static ServerCommand parse_user_command(char *str)
{
    ServerCommand ret = UNDEFINED;
    if (strncmp(str, "_kill", 5) == 0) {
        ret = KILL;
    } else if (strncmp(str, "_shutdown", 9) == 0) {
        ret = SHUTDOWN;
    } else if (strncmp(str, "_who", 4) == 0) {
        ret = WHO;
    }
    return ret;
}

static ServerCommand parse_client_command(char *str)
{
    ServerCommand ret = UNDEFINED;
    if (strncmp(str, "_connect", 8) == 0) {
        ret = CONNECT;
    } else if (strncmp(str, "_quit", 5) == 0) {
        ret = QUIT;
    } else if (strncmp(str, "_who", 4) == 0) {
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
            info("Name of client should be provided.");
            break;
        }
        client = vector_delete_first_equal(_s_clients, name, cmp_clients_by_name);
        info("Server has '%d' clients.", vector_total(_s_clients));
        if (client) {
            sprintf(message, "The client '%s' is quit.", client->name);
            vector_foreach(_s_clients, message, send_broadcast_msg);
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
        vector_foreach(_s_clients, message, send_broadcast_msg);
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
    fflush(stdout);
}

static int send_msg(Client *a_client, char *a_message)
{
    int length = strlen(a_message);
    int res;
    res = write(a_client->socket, &length, sizeof(length));
    if (res == -1) {
        printf("Could not wwrite to client '%s'\n", a_client->name);
        return 1;
    }
    res = write(a_client->socket, a_message, length);
    if (res == -1) {
        printf("Could not wwrite to client '%s'\n", a_client->name);
        return 1;
    }
    return 0;
}

static void send_broadcast_msg(void *a_client, void *a_message)
{
    Client *client = a_client;
    char *message = a_message;
    if (client->connected) {
        send_msg(client, message);
    }
}

static void collect_all_names(void *item, void *names)
{
    Client *client = item;
    char *all = names;
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
    sprintf(message, "Client with name '%s' is connecting.\n", *a_name);

    while (vector_is_contains(_s_clients, *a_name, cmp_clients_by_name) == 1) {
        was_used = 1;
        length = sizeof(*a_name);
        new_name = malloc(length + 1);
        sprintf(new_name, "%s_", *a_name);
        free(*a_name);
        *a_name = new_name;
    }
    if (was_used) {
        printf("%s", message);
        printf("Client '%s' already exist. Set new name '%s'\n", *a_name, new_name);
    }
}

void handle_client_command(char *a_data, Client *a_client, char *address)
{
    char *name = NULL;
    char message[BUF_SIZE] = {0};
    char buf[BUF_SIZE] = {0};
    ServerCommand client_cmd = parse_client_command(buf);

    if (a_client->connected == 0 && client_cmd != CONNECT) {
        info("Client with addres '%s' is not connected. "
             "Message '%s' not allowed.", address, buf);
        return;
    }

    switch (client_cmd) {
    case CONNECT:
        if (a_client->connected == 1) {
            info("Client '%s' with address '%s' already connected.",
                 a_client->name, address);
            return;
        }
        name = get_command_argument(a_data);
        verificate_name(&name);
        a_client->name = name;
        sprintf(message, "New client '%s' was connected.", a_client->name);
        vector_foreach(_s_clients, message, send_broadcast_msg);
        info("%s", message);
        a_client->connected = 1;
    break;
    case QUIT:
        vector_delete_first_equal(_s_clients, a_client, cmp_clients_by_ptr);
        sprintf(message, "The client '%s' is quit.", a_client->name);
        vector_foreach(_s_clients, message, send_broadcast_msg);
        info("%s", message);
        client_destroy(a_client);
        break;
    case WHO:
        vector_foreach(_s_clients, message, collect_all_names);
        sprintf(buf, "All clients: %s", message);
        printf("Send info about all clients to '%s'\n", a_client->name);
        send_msg(a_client, buf);
        break;
    case UNDEFINED:
        /* send received message to all clients */
        printf("Send '%s' to all clients from '%s'\n", a_data, a_client->name);
        sprintf(message, "%s: %s", a_client->name, a_data);
        vector_foreach(_s_clients, message, send_broadcast_msg);
        break;
    default:
        assert("Unknown command " == NULL);
        break;
    }
}

static void receive_client_data(Client *a_client)
{
    char buf[BUF_SIZE] = {0};
    char address[64] = {0};
    int res;

    res = connection_receive(a_client->conn, buf, BUF_SIZE);
    if (res) {
        vector_delete_first_equal(_s_clients, a_client, cmp_clients_by_ptr);
        sprintf(message, "Something happened. The client '%s' is quit.", client->name);
        vector_foreach(_s_clients, message, send_broadcast_msg);
        info("%s", message);
        client_shutdown(client);
        return;
    }

    sprintf(address, "%s:%d", connection_get_address_str(a_client->conn),
            connection_get_port(a_client->conn));

    info("From address '%s' recived message '%s'", address, buf);

    handle_client_command(buf, a_client, address);
    fflush(stdout);
    return ret;
}

int main(int argc, char** argv)
{
    Client *client = NULL;
    fd_set readf;
    int maxfd;
    int server_socket;
    int i;
    int ready;
    int amount;
    int server_port;

    if (argc < 2 || (server_port = atoi (argv[1])) == 0) {
        server_port = SERVER_PORT;
    }
    server_socket = tcp_socket_create(NULL, server_port);
    if (listen(server_socket, MAX_AMOUNT_CLIENTS) != 0) {
        fatal("Could not listen server socket '%d'. Error was '%d'\n"
                                    , server_socket, errno);
    }
    _s_clients = vector_create();

    printf("Server has started. Using port %d\n", server_port);
    printf("Allowed commands:"
           "\n\t1) _who"
           "\n\t2) _kill <name>"
           "\n\t3) _shutdown\n");

    _s_runing = 1;

    while(_s_runing) {
        FD_ZERO(&readf);
        maxfd = server_socket;
        for (i = 0; i < vector_total(_s_clients); ++i) {
            client = vector_get(_s_clients, i);
            FD_SET(client->socket, &readf);
            if (client->socket > maxfd) {
                maxfd = client->socket;
            }
        }
        FD_SET(server_socket, &readf);
        FD_SET(0, &readf);
        ready = select(maxfd + 1, &readf, 0, 0, 0);

        /* read data from user terminal */
        if (FD_ISSET(0, &readf)) {
            receive_user_data();
            if (--ready <= 0) {
                /* no more descriptors for handling*/
                continue;
            }
        }
        /* accept new client connection */
        if (FD_ISSET(server_socket, &readf)) {
            /* new client connection */
            Connection *conn = connection_tcp_accept_client(server_socket);
            if (conn) {
                client = client_create(conn);
                vector_add(_s_clients, client);
                /* add new descriptor to set */
                FD_SET(client->socket, &readf);
                if (--ready <= 0) {
                    /* no more descriptors for handling*/
                    continue;
                }
            }
        }
        /* read data from clients */
        amount = vector_total(_s_clients);
        for (i = 0; i <= amount; ++i) {
            /* check all clients for data */
            client = vector_get(_s_clients, i);
            if (client == NULL) {
                assert("Client is NULL" == NULL);
                continue;
            }
            if (FD_ISSET(client->socket, &readf)) {
                receive_client_data(client);
                if (--ready <= 0) {
                    /* no more descriptors for handling */
                    break;
                }
            }
        }
    }
    info("Close all sockets and removing all clients...");
    close(server_socket);
    amount = vector_total(_s_clients);
    for (i = 0; i <= amount; ++i) {
        if ((client = vector_get(_s_clients, i)) != NULL) {
            close(client->socket);
            client_destroy(client);
        };
    }
    vector_free(_s_clients);
    info("Done.");
}
