/*
$Id: serveur.c,v 1.2 2000/07/06 09:52:41 pfares Exp $
$Log: serveur.c,v $
Revision 1.2 2000/07/06 09:52:41 pfares
Amélioration du protocole entre client et serveur (recupération du
port client par recvfrom
.
Revision 1.1 2000/07/05 20:52:04 root
Initial revision
* Revision 1.2 1997/03/22 06:15:04 pascal
* Ajout des controles et entete
*
*/
#include <sys/types.h> /* See NOTES */
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
#include "wrsock.h"
#include "vector.h"

#define DEBUG       1
#define SERVER_PORT 4040

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
    struct sockaddr_in *address;
} Client;

static char* get_command_argument(char *a_data);
static void send_broadcast_msg(void *a_client, void *a_message);
static void collect_all_names(void *item, void *names);
static void send_msg(char *a_message, struct sockaddr_in *a_addr);

static volatile int _s_runing = -1;
static Vector *_s_clients = NULL;
static int _s_server_socket = -1;

static Client* client_create(char *name, struct sockaddr_in *address)
{
    Client *c = malloc(sizeof(Client));
    c->name = name;
    c->address = address;
    return c;
}

static void client_destroy(Client *client)
{
    if (client) {
        free(client->name);
        free(client->address);
        free(client);
    }
}


int cmp_clients_by_name(void *item, void *pattern)
{
    Client *cl_item = item;
    char *pat_name = pattern;
    if (strcmp(cl_item->name, pat_name) == 0) {
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

void receive_user_data(void)
{
    char buf[BUF_SIZE] = {0};
    char message[BUF_SIZE] = {0};
    char *name;
    Client *client;
    int count_recv = read(0, buf, BUF_SIZE);
    buf[count_recv - 1] = '\0';

    switch(parse_user_command(buf)) {
    case KILL:
        name = get_command_argument(buf);
        if (!name) {
            printf("Name of client should be provided.\n");
            break;
        }
        client = vector_delete_first_equal(_s_clients, name, cmp_clients_by_name);
        printf("Server has '%d' clients\n", vector_total(_s_clients));
        fflush(stdout);
        if (client) {
            sprintf(message, "The client '%s' is quit.", client->name);
            fflush(stdout);
            vector_foreach(_s_clients, message, send_broadcast_msg);
            printf("%s\n", message);
            /* send info data to client */
            sprintf(message, "server: You was killed.");
            send_msg(message, client->address);
            client_destroy(client);
        } else {
            printf("Client '%s' was not found\n", name);
        }
    break;
    case SHUTDOWN:
        sprintf(message, "server: I am going shutdown. Bye.");
        vector_foreach(_s_clients, message, send_broadcast_msg);
        _s_runing = 0;
        printf("%s\n", message);
    break;
    case WHO:
        vector_foreach(_s_clients, message, collect_all_names);
        printf("All clients: '%s'\n", message);
    break;
    default:
        printf("Unknown command. Allowed commands: _kill <name>, _shutdown, _who\n");
    break;
    }
    fflush(stdout);
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

static void send_msg(char *a_message, struct sockaddr_in *a_addr)
{
    size_t length = strlen(a_message);
    sendto(_s_server_socket, &length, sizeof(length), 0
                    , (struct sockaddr *)a_addr, sizeof(struct sockaddr));
    sendto(_s_server_socket, a_message, length, 0
                    , (struct sockaddr *)a_addr, sizeof(struct sockaddr));
}

static void send_broadcast_msg(void *a_client, void *a_message)
{
    Client *client = a_client;
    char *message = a_message;
    send_msg(message, client->address);
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
    if (arg) {
        len = strlen(arg);
        ret = malloc(len + 1);
        strncpy(ret, arg, len);
        ret[len] = '\0';
    }
    return ret;
}

void handle_client_command(ServerCommand a_cmd, char *a_data
                                , struct sockaddr_in *a_client_addr)
{
    size_t length = 0;
    char *name = NULL;
    char *new_name = NULL;
    char message[BUF_SIZE] = {0};
    char buf[BUF_SIZE] = {0};
    Client *client = NULL;
    switch (a_cmd) {
    case CONNECT:
        name = get_command_argument(a_data);
        if (vector_is_contains(_s_clients, name, cmp_clients_by_name) == 1) {
            length = sizeof(name);
            new_name = malloc(length + 1);
            sprintf(new_name, "%s_", name);
            printf("Client with name '%s' already exist. Set new name '%s'\n"
                                        , name, new_name);
            free(name);
            name = new_name;
        }
        client = client_create(name, a_client_addr);
        sprintf(message, "New client '%s' was connected.", client->name);
        vector_foreach(_s_clients, message, send_broadcast_msg);
        vector_add(_s_clients, client);
        printf("%s\n", message);

        break;
    case QUIT:
        client = vector_delete_first_equal(_s_clients, a_client_addr, cmp_clients_by_addr);
        sprintf(message, "The client '%s' is quit.", client->name);
        vector_foreach(_s_clients, message, send_broadcast_msg);
        printf("%s\n", message);
        client_destroy(client);
        break;
    case WHO:
        client = vector_get_first_equal(_s_clients, a_client_addr, cmp_clients_by_addr);
        vector_foreach(_s_clients, message, collect_all_names);
        sprintf(buf, "All clients: %s", message);
        printf("Send info about all clients to '%s'\n", client->name);
        send_msg(buf, a_client_addr);
        break;
    case UNDEFINED:
        /* send received message to all clients */
        //a_data[strlen(a_data) - 1] = '\0';
        client = vector_get_first_equal(_s_clients, a_client_addr, cmp_clients_by_addr);
        printf("Send '%s' to all clients from '%s'\n", a_data, client->name);
        sprintf(message, "%s: %s", client->name, a_data);
        vector_foreach(_s_clients, message, send_broadcast_msg);
        break;
    default:
        assert("Unknown command " == NULL);
        break;
    }
}

void receive_client_data(void)
{
    char buf[BUF_SIZE] = {0};
    int size_message;
    int res;
    char *straddr;
    ServerCommand client_cmd;
    socklen_t addr_len = sizeof(struct sockaddr_in);;
    struct sockaddr_in *addr= malloc(sizeof(struct sockaddr_in));

    recvfrom(_s_server_socket, &size_message, sizeof(size_message)
                    , 0, (struct sockaddr *)addr, &addr_len);
    recv(_s_server_socket, buf, size_message, 0);
    straddr = inet_ntoa(addr->sin_addr);

#ifdef DEBUG
    printf("From address '%s:%d' recived message '%s'\n", straddr, addr->sin_port, buf);
#endif

    client_cmd = parse_client_command(buf);
    res = vector_is_contains(_s_clients, addr, cmp_clients_by_addr);

    if (res == 0 && client_cmd != CONNECT) {
        printf("Client with addres '%s:%d' is not connected."
                        "Message '%s' not allowed.\n", straddr, addr->sin_port, buf);
    } else if (res == 1 && client_cmd == CONNECT) {
        printf("Cannot add client from address '%s:%d'. Client with the same"
               "address already connected.\n", straddr, addr->sin_port);
    } else {
        handle_client_command(client_cmd, buf, addr);
    }
    fflush(stdout);
}


int main(int argc, char** argv)
{
    _s_clients = vector_create();
    fd_set readf;
    int server_port;
    if (argc < 2 || (server_port = atoi (argv[1])) == 0) {
        server_port = SERVER_PORT;
    }
    printf("Server has started. Using port %d\n", server_port);
    printf("Allowed commands:"
           "\n\t1) _who"
           "\n\t2) _kill <name>"
           "\n\t3) _shutdown\n");
    _s_server_socket = udp_socket_create(NULL, server_port);
    _s_runing = 1;
    while(_s_runing)
	{
        FD_SET(_s_server_socket, &readf);
		FD_SET(0, &readf);
        switch (select (_s_server_socket + 1, &readf, 0,0,0))
		{
			default :
			if (FD_ISSET(0, &readf))
			{ 
				/* STDIN*/
                receive_user_data();
			}
            else if (FD_ISSET(_s_server_socket, &readf))
			{
				/*SOCKET*/
                receive_client_data();
			}
		}
	}
    close(_s_server_socket);
}
