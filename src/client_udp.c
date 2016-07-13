/* $Id: client.c,v 1.2 2000/07/06 09:51:59 pfares Exp $
* $Log: client.c,v $
* Revision 1.2 2000/07/06 09:51:59 pfares
* Amélioration du protocole entre client et serveur (recupération du
* port client par recvfrom
*
* Revision 1.1 2000/07/05 20:51:16 root
* Initial revision
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
#include "wrsock.h"

#define BUF_SIZE 512

typedef enum {
    UNDEFINED = 0,
    CONNECT,
    QUIT,
    WHO
} ClientCommand;

struct conn_data {
    char *name;
    char *host;
    int port;
};

static int volatile _s_connected = 0;
static struct sockaddr_in *_s_server_addr;
static int _s_client_sock = 0;
int len = sizeof (struct sockaddr_in);


static ClientCommand parse_user_command(char *str)
{
    ClientCommand ret = UNDEFINED;
    if (strncmp(str, "_connect", 8) == 0) {
        ret = CONNECT;
    } else if (strncmp(str, "_quit", 5) == 0) {
        ret = QUIT;
    } else if (strncmp(str, "_who", 4) == 0) {
        ret = WHO;
    }
    return ret;
}

static struct conn_data* get_conn_arguments(char *a_data)
{
    char *arg;
    struct conn_data *conn = malloc(sizeof(struct conn_data));
    conn->host = NULL;
    conn->name = NULL;


#define get_string(value)                           \
    if ((arg = strtok (NULL, " ")) != NULL) {       \
        size_t len = strlen(arg);                   \
        char *param = malloc(len + 1);              \
        strncpy(param, arg, len);                   \
        param[len] = '\0';                          \
        value = param;                         \
    } else {                                        \
        goto error;                                 \
    }

    strtok(a_data, " ");
    get_string(conn->name);
    get_string(conn->host);

    if ((arg = strtok (NULL, " ")) != NULL) {
        conn->port = atoi(arg);
    } else {
        goto error;
    }
    return conn;

error:
    free(conn->host);
    free(conn->name);
    free(conn);
    conn = NULL;
    return conn;
}

void send_message(char *message)
{
    size_t length = strlen(message);
    sendto (_s_client_sock, &length, sizeof (length), 0
            , (struct sockaddr *) _s_server_addr, sizeof (struct sockaddr));
    sendto (_s_client_sock, message, length, 0, (struct sockaddr *) _s_server_addr
                                , sizeof (struct sockaddr));
}

static ClientCommand receive_user_data (struct conn_data **data, char *msg)
{
    char buf[BUF_SIZE] = {0};
	int taillemessage;
    ClientCommand cmd;
    taillemessage = read(0, buf, BUF_SIZE);
    cmd = parse_user_command(buf);
    *data = NULL;
    if (cmd == CONNECT) {
        *data = get_conn_arguments(buf);
    } else if (cmd == UNDEFINED) {
        sprintf(msg, "%s", buf);
    }
    return cmd;
}

void TraitementSock (int sock)
{
    char buf[BUF_SIZE] = {0}; /* Buffer de reception */
	int taillemessage; /* Taille du message a recevoir
	* Chaque emetteur commence par envoyer la taille du message */
	/* On recoit la taile du message puis le message */
	recvfrom (sock, &taillemessage, sizeof (taillemessage), 0, (struct sockaddr *) NULL, NULL);
	recvfrom (sock, buf, taillemessage, 0, (struct sockaddr *) NULL, NULL);
    printf("%s\n", buf);
    if (strcmp(buf, "server: You was killed.") == 0
            || strcmp(buf, "server: I am going shutdown. Bye.") == 0) {
        close(_s_client_sock);
        free(_s_server_addr);
        _s_client_sock = 0;
        _s_connected = 0;
    }
}


void send_conn_data(char *name)
{
    char message[BUF_SIZE] = {0};
    sprintf(message, "_connect %s", name);
    send_message(message);
}


int main (int argc, char **argv)
{
    char message [BUF_SIZE];
    fd_set readf;
    struct conn_data *data = NULL;

    printf("Client started. Allowed commands:"
           "\n\t1) _connect name address port"
           "\n\t2) _who"
           "\n\t3) _quit.\n"
           "All another words client would assume as message to other clients.\n");

    while(1) {
        FD_SET (0, &readf);
        if (_s_connected) {
            FD_SET(_s_client_sock, &readf);
        }
        switch (select (_s_client_sock + 1, &readf, 0, 0, 0)) {
        default:
            if (FD_ISSET (0, &readf)) {
                switch(receive_user_data(&data, message)) {
                case CONNECT:
                    if (_s_connected) {
                        printf("This client has already connected to server.\n");
                    } else if (!data) {
                        printf("Bad arguments for _connect.\n");
                    } else {
                        _s_client_sock = SockUdp(NULL, 0);
                        _s_server_addr = CreerSockAddr(data->host, data->port);
                        _s_connected = 1;
                        send_conn_data(data->name);
                        printf("Client was connected to '%s:%d'\n", data->host, data->port);
                        fflush(stdout);
                    }
                    if (data) {
                        free(data->host);
                        free(data->name);
                        free(data);
                    }
                    data = NULL;
                break;
                case QUIT:
                    if (!_s_connected) {
                        printf("Client has already disconnected.\n");
                        break;
                    }
                    send_message("_quit");
                    close(_s_client_sock);
                    free(_s_server_addr);
                    _s_client_sock = 0;
                    _s_connected = 0;
                break;
                case WHO:
                    if (!_s_connected) {
                        printf("Client is disconnected. Need to connect.\n");
                        break;
                    }
                    send_message("_who");
                break;
                case UNDEFINED:
                    if (!_s_connected) {
                        printf("Client is disconnected. Need to connect.\n");
                        break;
                    }
                    /* just simple message. need to send to server */
                    message[strlen(message) - 1] = '\0';
                    send_message(message);
                break;
                default:
                    printf("Unknown command\n");
                break;
                }
            } else if (FD_ISSET (_s_client_sock, &readf)) {
                TraitementSock(_s_client_sock);
            }
        }
    }
}
