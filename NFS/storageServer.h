#ifndef _SS_H_
#define _SS_H_

#define BUFFER_SIZE 4096
#define MAX_PATH_LENGTH 256
#define MAX_CLIENTS 20

typedef struct {
   int client_socket;
} ClientHandler;

typedef struct {
   int nm_socket;
} NamingServerHandler;

#endif