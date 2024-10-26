// naming_server.c (Updated to store multiple file paths for each Storage Server)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <semaphore.h>

#define BUFFER_SIZE 1024
#define MAX_STORAGE_SERVERS 100
#define MAX_PATHS_PER_SERVER 10
#define MAX_PATH_LENGTH 100

typedef struct {
   char ip[INET_ADDRSTRLEN];
   int port;
   char paths[MAX_PATHS_PER_SERVER][MAX_PATH_LENGTH]; // 2D array to store multiple paths
   int path_count; // To keep track of the number of paths for the server
   int sock; // Socket descriptor for future communication
} StorageServerInfo;

StorageServerInfo storage_servers[MAX_STORAGE_SERVERS];
int storage_server_count = 0;
pthread_mutex_t storage_mutex;

// Function to handle requests from a connected storage server
void* handle_storage_server(void* server_info) {
   StorageServerInfo* server = (StorageServerInfo*)server_info;
   char buffer[BUFFER_SIZE];

   while (1) {
      memset(buffer, 0, BUFFER_SIZE);
      int bytes_received = recv(server->sock, buffer, BUFFER_SIZE, 0);
      if (bytes_received <= 0) {
         printf("Connection closed by Storage Server %s:%d\n", server->ip, server->port);
         break; // Exit if connection is closed
      }

      printf("Received from Storage Server %s:%d: %s\n", server->ip, server->port, buffer);
      // Handle the received message here (e.g., process requests, etc.)
   }

   close(server->sock);
   free(server); // Free the dynamically allocated server_info here
   return NULL;
}

// Function to handle a connected client
void* handle_client(void* sock_ptr) {
   int sock = *((int*)sock_ptr);
   free(sock_ptr);

   char buffer[BUFFER_SIZE];
   memset(buffer, 0, BUFFER_SIZE);

   recv(sock, buffer, BUFFER_SIZE, 0);
   printf("Received path query from Client: %s\n", buffer);

   char requested_path[MAX_PATH_LENGTH];
   strncpy(requested_path, buffer, MAX_PATH_LENGTH);

   pthread_mutex_lock(&storage_mutex);
   for (int i = 0; i < storage_server_count; i++) {
      for (int j = 0; j < storage_servers[i].path_count; j++) {
         if (strcmp(storage_servers[i].paths[j], requested_path) == 0) {
            snprintf(buffer, BUFFER_SIZE, "Storage IP: %s, Storage Port: %d",
                     storage_servers[i].ip, storage_servers[i].port);
            send(sock, buffer, strlen(buffer), 0);
            pthread_mutex_unlock(&storage_mutex);
            close(sock);
            return NULL;
         }
      }
   }
   pthread_mutex_unlock(&storage_mutex);

   snprintf(buffer, BUFFER_SIZE, "Error: Path not found");
   send(sock, buffer, strlen(buffer), 0);
   close(sock);
   return NULL;
}

int main(int argc, char* argv[]) {
   if (argc < 2) {
      printf("Usage: %s <port>\n", argv[0]);
      return 1;
   }

   int port = atoi(argv[1]);
   int naming_sock, client_sock;
   struct sockaddr_in naming_addr, client_addr;
   socklen_t client_addr_len = sizeof(client_addr);

   pthread_mutex_init(&storage_mutex, NULL);

   naming_sock = socket(AF_INET, SOCK_STREAM, 0);
   naming_addr.sin_family = AF_INET;
   naming_addr.sin_addr.s_addr = INADDR_ANY;
   naming_addr.sin_port = htons(port);

   bind(naming_sock, (struct sockaddr*)&naming_addr, sizeof(naming_addr));
   listen(naming_sock, 10);
   printf("Naming Server listening on port %d...\n", port);

   while (1) {
      client_sock = accept(naming_sock, (struct sockaddr*)&client_addr, &client_addr_len);
      printf("Connection accepted.\n");

      char buffer[BUFFER_SIZE];
      memset(buffer, 0, BUFFER_SIZE);
      recv(client_sock, buffer, BUFFER_SIZE, 0);

      // Check connection type
      if (strncmp(buffer, "CLIENT", 6) == 0) {
         printf("Client connected.\n");
         int* client_sock_ptr = malloc(sizeof(int));
         *client_sock_ptr = client_sock;
         pthread_t client_thread;
         pthread_create(&client_thread, NULL, handle_client, client_sock_ptr);
         pthread_detach(client_thread);
      } 
      else if (strncmp(buffer, "STORAGE", 7) == 0) {
         printf("Storage server connected.\n");
         char ip[INET_ADDRSTRLEN];
         int port;
         char paths[BUFFER_SIZE]; // Buffer to receive paths

         // Get the registration data from the storage server
         if (recv(client_sock, buffer, BUFFER_SIZE, 0) > 0) {
            printf("Received registration from Storage Server: %s\n", buffer);

            if (sscanf(buffer, "STORAGE %s %d %[^\n]", ip, &port, paths) == 3) {
               pthread_mutex_lock(&storage_mutex);
               if (storage_server_count < MAX_STORAGE_SERVERS) {
                  // Dynamically allocate memory for new server information
                  StorageServerInfo* new_server = malloc(sizeof(StorageServerInfo));

                  // Store IP and port
                  strcpy(new_server->ip, ip);
                  new_server->port = port;
                  new_server->sock = client_sock; // Save socket for future communication
                  new_server->path_count = 0; // Initialize path count

                  // Tokenize the received paths and store them
                  char* token = strtok(paths, ",");
                  while (token != NULL && new_server->path_count < MAX_PATHS_PER_SERVER) {
                        strncpy(new_server->paths[new_server->path_count], token, MAX_PATH_LENGTH);
                        new_server->path_count++;
                        token = strtok(NULL, ",");
                  }

                  printf("Storage server registered: %s:%d with %d paths.\n", ip, port, new_server->path_count);
                  // Spawn a thread to handle future communication with this storage server
                  pthread_t storage_thread;
                  pthread_create(&storage_thread, NULL, handle_storage_server, new_server);
                  pthread_detach(storage_thread);
                  storage_server_count++;
               } 
               else {
                  printf("Maximum storage servers reached.\n");
                  close(client_sock); // Close socket if max reached
               }
               pthread_mutex_unlock(&storage_mutex);
            }
         }
      } 
      else {
         printf("Unknown connection type.\n");
         close(client_sock);
      }
   }

   pthread_mutex_destroy(&storage_mutex);
   close(naming_sock);
   return 0;
}
