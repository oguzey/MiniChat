#ifndef WRSOCK_H
#define WRSOCK_H

#include <sys/socket.h>
#include <netinet/in.h>

#define BUF_SIZE 2048
#define USER_FD 0



enum connection_type {
    TCP,
    UDP
};

typedef enum connection_type ConnectionType;
typedef struct connection Connection;


Connection* connection_create(ConnectionType type, char *name, int port);
void connection_destroy(Connection *connection);
Connection* connection_tcp_accept_client(int server_fd);
int connection_tcp_connect(Connection *conn);
int connection_tcp_send(Connection *conn, char *data, int len);
int connection_udp_send(Connection *conn, char *data, int len);
int connection_send(Connection *conn, char *data, int len);
int connection_tcp_receive(Connection *conn, char *buf, size_t bufsize);
int connection_udp_receive(Connection *conn, char *buf, size_t bufsize);
int connection_receive(Connection *conn, char *buf, size_t bufsize);
char* connection_get_address_str(Connection *conn);
in_addr_t connection_get_address(Connection *conn);
int connection_get_port(Connection *conn);
int connection_get_fd(Connection *conn);
ConnectionType connection_get_type(Connection *conn);

struct sockaddr_in *address_create(char *, int);
int udp_socket_create(char *, int);
int tcp_socket_create(char *nom, int numport);
int tcp_socket_connect(int fd, struct sockaddr_in *addr);

#endif // WRSOCK_H

