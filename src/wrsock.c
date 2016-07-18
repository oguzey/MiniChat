#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <arpa/inet.h>

#include "logger.h"
#include "wrsock.h"

struct connection {
    ConnectionType type;
    struct sockaddr_in *other_side;
    int fd;
};

static int socket_create(int type, char *name, int port)
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
    struct in_addr **pptr = NULL;

    bzero(adsock, sizeof(struct sockaddr_in));

    if (name) {
        haddr = gethostbyname(name);
        if (haddr) {
            pptr = (struct in_addr **) haddr->h_addr_list;
            memcpy(&adsock->sin_addr, *pptr, sizeof(struct in_addr));
        } else {
            perror("Unknown name of host.\n");
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
//    if (bind(conn->fd,(struct sockaddr *)conn->other_side, sizeof(struct sockaddr)) < 0) {
//        perror("Could not bind to socket.\n");
//        connection_destroy(conn);
//        return NULL;
//    }
    assert(conn);
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

int connection_tcp_connect(Connection *conn)
{
    assert(conn);
    int res = connect(conn->fd, (struct sockaddr *)conn->other_side, sizeof(struct sockaddr_in));
    if( res == -1) {
       perror("Connect failed.\n");
       return 1;
    }
    return 0;
}

int connection_tcp_send(Connection *conn, char *data, int len)
{
    assert(conn);
    int res;
    res = write(conn->fd, &len, sizeof(len));
    if (res == -1) {
        warn("Could not write length of data to server.");
        return -1;
    }
    debug("Was wrote %d bytes to connection '%s:%d'.",
                    res, connection_get_address_str(conn),
                    connection_get_port(conn));
    res = write(conn->fd, data, len);
    if (res == -1) {
        warn("Could not write to server.");
        return -1;
    }
    debug("Was wrote %d bytes to connection '%s:%d'.",
                    res, connection_get_address_str(conn),
                    connection_get_port(conn));
    return 0;
}

int connection_udp_send(Connection *conn, char *data, int len)
{
    int res = 0;
    res = sendto (conn->fd, &len, sizeof (len), 0
            , (struct sockaddr *) conn->other_side, sizeof (struct sockaddr));
    debug("Send to udp connection: res '%d'", res);
    sendto (conn->fd, data, len, 0, (struct sockaddr *) conn->other_side
                                , sizeof (struct sockaddr));
    debug("Send to udp connection: res '%d'", res);
    return 0;
}

int connection_send(Connection *conn, char *data, int len)
{
    if (conn->type == TCP) {
        return connection_tcp_send(conn, data, len);
    } else {
        return connection_udp_send(conn, data, len);
    }
}

int connection_tcp_receive(Connection *conn, char *buf, size_t bufsize)
{
    int length = 0;
    int res;

    res = read(conn->fd, &length, sizeof (length));
    if (res == -1) {
        perror("Could not read data from connection.\n");
        return -1;
    } else if (res == 0) {
        debug("Connection end '%s:%d'", connection_get_address_str(conn),
                                        connection_get_port(conn));
        return -1;
    }
    assert(length <= bufsize);
    res = read(conn->fd, buf, length);
    if (res == -1) {
        perror("Could not read data from connection.\n");
        return -1;
    } else if (res == 0) {
        debug("Connection end '%s:%d'", connection_get_address_str(conn),
                                        connection_get_port(conn));
        return -1;
    }
    return 0;
}

int connection_udp_receive(Connection *conn, char *buf, size_t bufsize)
{
    int length = 0;
    int res = 0;

    //TODO: add check for address
    res = recvfrom(conn->fd, &length, sizeof (length), 0, (struct sockaddr *)NULL, NULL);
    debug("From udp connection receive res '%d'", res);
    assert(length <= bufsize);
    res = recvfrom (conn->fd, buf, length, 0, (struct sockaddr *) NULL, NULL);
    debug("From udp connection receive res '%d'", res);
    return 0;
}

int connection_receive(Connection *conn, char *buf, size_t bufsize)
{
    if (conn->type == TCP) {
        return connection_tcp_receive(conn, buf, bufsize);
    } else {
        return connection_udp_receive(conn, buf, bufsize);
    }
}

char* connection_get_address_str(Connection *conn)
{
    return inet_ntoa(conn->other_side->sin_addr);
}

in_addr_t connection_get_address(Connection* conn)
{
    return conn->other_side->sin_addr.s_addr;
}

int connection_get_port(Connection *conn)
{
    return conn->other_side->sin_port;
}

int connection_get_fd(Connection *conn)
{
    return conn->fd;
}

ConnectionType connection_get_type(Connection *conn)
{
    return conn->type;
}
