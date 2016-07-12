#!/bin/bash

gcc -o ./client ./Client/client.c ./Client/wrsock.c
gcc -o ./server ./Server/server.c ./Server/vector.c ./Server/wrsock.c
