CC = gcc

CFLAGS = -std=c11 -Wall -Wpedantic -O0 -g3
INCLUDES = -I ./include

COMMON_SRCS = ./src/wrsock.c ./src/vector.c
COMMON_OBJS = $(COMMON_SRCS:.c=.o)


TCP_CLIENT_SRC = ./src/client_tcp.c
TCP_SERVER_SRC = ./src/server_tcp.c

TCP_CLIENT_OBJ = $(TCP_CLIENT_SRC:.c=.o)
TCP_SERVER_OBJ = $(TCP_SERVER_SRC:.c=.o)


UDP_CLIENT_SRC = ./src/client_udp.c
UDP_SERVER_SRC = ./src/server_udp.c

UDP_CLIENT_OBJ = $(UDP_CLIENT_SRC:.c=.o)
UDP_SERVER_OBJ = $(UDP_SERVER_SRC:.c=.o)


TARGET = client_tcp client_udp server_tcp server_udp


.PHONY: all clean

default: all

all: $(COMMON_OBJS) $(TARGET) 

client_tcp: $(TCP_CLIENT_OBJ)
	$(CC) $(CFLAGS) $(INCLUDES) -o client_tcp $(COMMON_OBJS) $(TCP_CLIENT_OBJ)

server_tcp: $(TCP_SERVER_OBJ)
	$(CC) $(CFLAGS) $(INCLUDES) -o server_tcp $(COMMON_OBJS) $(TCP_SERVER_OBJ)

client_udp: $(UDP_CLIENT_OBJ)
	$(CC) $(CFLAGS) $(INCLUDES) -o client_udp $(COMMON_OBJS) $(UDP_CLIENT_OBJ)

server_udp: $(UDP_SERVER_OBJ)
	$(CC) $(CFLAGS) $(INCLUDES) -o server_udp $(COMMON_OBJS) $(UDP_SERVER_OBJ)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	find ./ -name "*.o" | xargs rm -f
	rm -f $(TARGET)
