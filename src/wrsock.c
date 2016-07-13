#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <assert.h>

#include "wrsock.h"

struct connection {
    ConnectionType type;
    struct sockaddr_in other_side;
    int fd;
};

static socket_create(int type, char *name, int port)
{
    int sock = -1;
    struct sockaddr_in *adsock = address_create(name, port);
    sock = socket(AF_INET, type, 0);
    if (sock <= 0) {
        perror("Could not create socket.\n");
    }
    if (bind(sock,(struct sockaddr *) adsock, sizeof(*adsock)) < 0) {
        perror("Could not bind to socket.\n");
    }
    return sock;
}

struct sockaddr_in *address_create(char *name, int port)
{
    struct sockaddr_in *adsock = malloc(sizeof(*adsock));
    struct hostent *haddr = NULL;
    struct in_addr **pptr = NULLs;

    bzero(adsock, sizeof(struct sockaddr_in));

    if (name) {
        haddr = gethostbyname(name);
        if (haddr <= 0) {
            perror("Unknown name of host.\n");
        } else {
            pptr = (struct in_addr **) haddr->h_addr_list;
            memcpy(&adsock->sin_addr, *pptr, sizeof(struct in_addr));
        }
    } else {
        adsock->sin_addr.s_addr = INADDR_ANY;
    }
    adsock->sin_family=AF_INET;
    adsock->sin_port = htons(port);
    return adsock;
}

int udp_socket_create(char *name, int port)
{
    return socket_create(SOCK_DGRAM, name, port);
}

int tcp_socket_create(char *name, int port)
{
    return socket_create(SOCK_STREAM, name, port);
}

int tcp_socket_connect(int fd, struct sockaddr_in *addr)
{
    int res;
    res = connect(fd, (struct sockaddr *)addr, sizeof(struct sockaddr_in));
    if( res == -1) {
       perror("Connect failed.\n");
       return 1;
    }
    return 0;
}

Connection* connection_create(ConnectionType type, char *name, int port)
{
    assert(type == TCP || type == UDP);

    Connection *conn = malloc(sizeof(*conn));
    conn->type = type;
    conn->other_side = address_create(name, port);
    conn->fd = socket(AF_INET, type == TCP ? SOCK_STREAM : SOCK_DGRAM, 0);
    if (conn->fd <= 0) {
        perror("Could not create socket.\n");
        connection_destroy(conn);
        return NULL;
    }
    if (bind(sock,(struct sockaddr *)adsock, sizeof(*adsock)) < 0) {
        perror("Could not bind to socket.\n");
        connection_destroy(conn);
        return NULL;
    }
    return conn;
}

void connection_destroy(Connection *connection)
{
    if (connection) {
        free(connection->other_side);
        free(connection);
    }
}

Connection* connection_tcp_accept_client(int server_fd)
{
    assert(server_fd > 0);
    socklen_t addr_len = sizeof(struct sockaddr_in);
    int fd = 0;
    struct sockaddr_in *address = NULL;

    fd = accept(server_fd, (struct sockaddr *)address, &addr_len);
    if (fd == -1) {
        perror("Could not accept connection.\n");
        return NULL;
    }
    assert(addr_len == sizeof(struct sockaddr_in));

    Connection *conn = malloc(sizeof(*conn));
    conn->type = TCP;
    conn->fd = fd;
    conn->other_side = address;
    return conn;
}

char* connection_get_address_str(Connection *conn)
{
    return inet_ntoa(conn->other_side.sin_addr);
}

char* connection_get_address(Connection *conn)
{
    return conn->other_side.sin_addr.s_addr;
}

int connection_get_port(Connection *conn)
{
    return conn->other_side.sin_port;
}

int connection_get_fd(Connection *conn)
{
    return conn->fd;
}
