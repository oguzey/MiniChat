#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <strings.h>

#include "logger.h"
#include "wrsock.h"

typedef enum {
    UNDEFINED = 0,
    CONNECT,
    QUIT,
    WHO
} ClientCommand;

static Connection *_s_server_conn = NULL;
static ConnectionType _s_con_type = -1;

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

int parse_user_cmd_arguments(char *data, char **host, char **name, int *port)
{
    char *arg;
    size_t len_arg;
    int ret = -3;

    /* split by spaces */
    strtok(data, " ");

    if ((arg = strtok (NULL, " ")) != NULL) {
        len_arg = strlen(arg);
        *name = malloc(len_arg + 1);
        strncpy(*name, arg, len_arg);
        (*name)[len_arg] = '\0';
        ++ret;
    }
    if ((arg = strtok (NULL, " ")) != NULL) {
        len_arg = strlen(arg);
        *host = malloc(len_arg + 1);
        strncpy(*host, arg, len_arg);
        (*host)[len_arg] = '\0';
        ++ret;
    }
    if ((arg = strtok (NULL, " ")) != NULL) {
        *port = atoi(arg);
        ++ret;
    }
    return ret;
}

static void handle_user_data(void)
{
    char buf[BUF_SIZE] = {0};
    int res;
    int read_bytes;
    ClientCommand cmd;
    int port;
    char *host = NULL;
    char *name = NULL;

    read_bytes = read(0, buf, BUF_SIZE);
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

    cmd = parse_user_command(buf);
    switch(cmd) {
    case CONNECT:
        if (_s_server_conn) {
            info("Invalid command. The client already connected to server.");
            return;
        }
        res = parse_user_cmd_arguments(buf, &host, &name, &port);
        if (res) {
            warn("Bad arguments provided for _connect command.");
            free(host);
            free(name);
            return;
        }
        _s_server_conn = connection_create(TCP, host, port);
        if (connection_get_type(_s_server_conn) == TCP) {
            res = connection_tcp_connect(_s_server_conn);
            if (res) {
                goto error;
            }
        }
        sprintf(buf, "_connect %s", name);
        res = connection_send(_s_server_conn, buf, strlen(buf));
        if (res) {
            goto error;
        }
        debug("Message '%s' was sent to server.", buf);
        info("Client was connected to '%s:%d'.", host, port);
        free(host);
        free(name);
        fflush(stdout);
    break;
    case QUIT:
        if (!_s_server_conn) {
            info("Client has already disconnected.");
            break;
        }
        connection_send(_s_server_conn, "_quit", strlen("_quit"));
        connection_destroy(_s_server_conn);
        _s_server_conn = NULL;
    break;
    case WHO:
        if (!_s_server_conn) {
            info("Client is disconnected. Need to connect.");
            break;
        }
        info("Client has already disconnected.");
        res = connection_send(_s_server_conn, "_who", strlen("_who"));
        if (res) {
            goto error;
        }
    break;
    case UNDEFINED:
        if (!_s_server_conn) {
            info("Client is disconnected. Need to connect.");
            break;
        }
        /* just simple message. need to send to server */
        res = connection_send(_s_server_conn, buf, read_bytes);
        if (res) {
            goto error;
        }
    break;
    default:
        assert("Unknown command" == NULL);
    break;
    }
    return;
error:
    connection_destroy(_s_server_conn);
    _s_server_conn = NULL;
}

static void handle_server_data(void)
{
    assert(_s_server_conn);
    int res;
    char buf[BUF_SIZE] = {0};

    res = connection_receive(_s_server_conn, buf, BUF_SIZE);
    if (res) {
        info("We lose server...");
        connection_destroy(_s_server_conn);
        _s_server_conn = NULL;
    }
    info("server: '%s'", buf);
}

static void args_parse(int argc, char **argv)
{

#define print_help() \
    info("\nUsage: \n\t%s -t <TYPE>\nwhere TYPE is one from UDP or TCP.", argv[0]);\
    exit(EXIT_SUCCESS);

    int c;
    while ((c = getopt (argc, argv, "ht:")) != -1) {
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
        case '?':
            warn("Bad option -%c provided.", optopt);
            print_help();
        break;
        default:
            print_help();
        }
    }
}

int main (int argc, char **argv)
{
    fd_set readf;
    int socket;

    args_parse(argc, argv);

    assert(_s_con_type == TCP || _s_con_type == UDP);
    if (_s_con_type == TCP) {
        info("Client will use TCP protocol.");
    } else {
        info("Client will use UDP protocol.");
    }

    info("Client started. Allowed commands:"
           "\n\t1) _connect name address port"
           "\n\t2) _who"
           "\n\t3) _quit.\n"
           "All another words client would assume as message to other clients.");

    FD_ZERO(&readf);
    while(1) {
        socket = 0;
        FD_SET(0, &readf);
        if (_s_server_conn) {
            socket = connection_get_fd(_s_server_conn);
            FD_SET(socket, &readf);

        }
        switch (select (socket + 1, &readf, 0, 0, 0)) {
        default:
            if (FD_ISSET (0, &readf)) {
                handle_user_data();
            } else if (FD_ISSET (socket, &readf)) {
                handle_server_data();
            }
        }
    }
}
