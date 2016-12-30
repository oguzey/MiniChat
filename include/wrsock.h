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


Connection* connection_client_create(ConnectionType type, char *other_host,
                                     int other_port, char *my_host, int my_port);
Connection *connection_server_create(ConnectionType type, char *host, int port,
                                     unsigned int max_clients);
Connection *connection_create_raw(ConnectionType type, int fd,
                                  struct sockaddr_in *other_addr);
void connection_destroy(Connection *connection);
Connection* connection_tcp_accept(int server_fd);
int connection_tcp_connect(Connection *conn);
int connection_tcp_send(Connection *conn, const char *data, int len);
int connection_udp_send(Connection *conn, const char *data, int len);
int connection_send(Connection *conn, const char *data, int len);
int connection_tcp_receive(Connection *conn, char *buf, size_t bufsize);
int connection_udp_receive(Connection *conn, char *buf, size_t bufsize);
int connection_udp_receive_with_addr(Connection *conn, char *buf, size_t bufsize,
                           struct sockaddr **addr);
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

