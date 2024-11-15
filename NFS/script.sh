#!/usr/bin/bash

gcc namingServer.c helper.c -o namingServer
gcc storageServer.c helper.c -o storageServer
gcc client.c -o client
