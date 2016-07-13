#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>

#include "wrsock.h"

struct connection {
    ConnectionType type;
    struct sockaddr_in server_address;
    int fd;
};



struct sockaddr_in *CreerSockAddr(char *name, int port)
{
    struct sockaddr_in *adsock =(struct sockaddr_in *)
    malloc(sizeof(struct sockaddr_in));
    struct hostent *haddr=NULL;
    struct in_addr **pptr;
    //char str[32];
    #ifdef DEBUG
    printf("IN CreerSockAddr %s,%d\n", name, port);
    #endif
    bzero(adsock, sizeof(struct sockaddr_in));
    if (name)
    {
        haddr = gethostbyname(name);
        if (haddr <= 0)
        {
            perror("Nom de machine inconnu");
        }
        else
        {
            pptr = (struct in_addr **) haddr->h_addr_list;
            memcpy(&adsock->sin_addr, *pptr, sizeof(struct in_addr));
            //inet_ntop(haddr->h_addrtype, *pptr, str, sizeof(str));
            #ifdef DEBUG
            printf("%s", str);
            #endif
        }
    }
    else
    {
        adsock->sin_addr.s_addr=INADDR_ANY;
    }
    adsock->sin_family=AF_INET;
    adsock->sin_port = htons(port);
    #ifdef DEBUG
    printf("fin\n");
    #endif
    return (adsock);
}

int SockUdp(char *nom, int numport)
{
    int sock;
    //int r=1;
    /* adsock : adresse de la socket d'écoute */
    struct sockaddr_in *adsock = (struct sockaddr_in *) CreerSockAddr(nom, numport);
    if ((sock=socket(AF_INET,SOCK_DGRAM,0)) <= 0)
    {
        perror("\n pb creation socket \n");
    }
    /* sock=dup(sock);
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &r, sizeof(r));
    */
    #ifdef DEBUG
    printf("La socket num %d\n", sock);
    #endif
    if (bind(sock,(struct sockaddr *) adsock, sizeof(*adsock)) <0) {
        perror("\n pb bind");
    }
    return(sock);
}

int SockTcp(char *nom, int numport)
{
    int sock = -1;
    struct sockaddr_in *adsock = CreerSockAddr(nom, numport);
    if ((sock=socket(AF_INET, SOCK_STREAM, 0)) <= 0) {
        perror("\n pb creation socket \n");
    }
    if (bind(sock,(struct sockaddr *) adsock, sizeof(*adsock)) < 0) {
        perror("\n pb bind");
    }
    return sock;
}

int connect_tcp_socket(int fd, struct sockaddr_in *addr)
{
    int res;
    res = connect(fd, (struct sockaddr *)addr, sizeof(struct sockaddr_in));
    if( res < 0) {
       printf("Connect failed. Error was: '%d'\n", res);
       return 1;
    }
    return 0;
}
