#ifndef WRSOCK_H
#define WRSOCK_H

typedef connection Connection;

typedef enum {
    TCP,
    UDP
} ConnectionType;

struct sockaddr_in *address_create(char *, int);
int udp_socket_create(char *, int);
int tcp_socket_create(char *nom, int numport);
int tcp_socket_connect(int fd, struct sockaddr_in *addr);

#endif // WRSOCK_H

