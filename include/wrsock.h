#ifndef WRSOCK_H
#define WRSOCK_H

typedef connection Connection;

typedef enum {
    TCP,
    UDP
} ConnectionType;

struct sockaddr_in *CreerSockAddr(char *, int);
int SockUdp(char *, int);
int SockTcp(char *nom, int numport);
int connect_tcp_socket(int fd, struct sockaddr_in *addr);

#endif // WRSOCK_H

