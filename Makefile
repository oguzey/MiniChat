CC = gcc

CFLAGS = -std=c11 -Wall -Wpedantic -O0 -g3
INCLUDES = -I ./include

COMMON_SRCS = ./src/wrsock.c ./src/vector.c
COMMON_OBJS = $(COMMON_SRCS:.c=.o)


CLIENT_SRC = ./src/client.c
SERVER_SRC = ./src/server.c

CLIENT_OBJ = $(CLIENT_SRC:.c=.o)
SERVER_OBJ = $(SERVER_SRC:.c=.o)


TARGET = client server


.PHONY: all clean

default: all

all: $(COMMON_OBJS) $(TARGET)

client: $(CLIENT_OBJ)
	$(CC) $(CFLAGS) $(INCLUDES) -o client $(COMMON_OBJS) $(CLIENT_OBJ)

server: $(SERVER_OBJ)
	$(CC) $(CFLAGS) $(INCLUDES) -o server $(COMMON_OBJS) $(SERVER_OBJ)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	find ./ -name "*.o" | xargs rm -f
	rm -f $(TARGET)

