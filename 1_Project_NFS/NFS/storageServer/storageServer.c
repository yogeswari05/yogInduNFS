// storage_server.c (Updated to handle requests from both Naming Server and multiple Clients using threads)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUFFER_SIZE 1024
#define MAX_PATHS 10

// List of paths the storage server is responsible for
char paths[MAX_PATHS][BUFFER_SIZE];
int num_paths = 0;

// Structure to hold client request handling parameters
typedef struct {
   int client_sock;
} ClientRequestArgs;

// Function to handle a client request for a CRUD operation
void* handle_client_request(void* args) {
   ClientRequestArgs* client_args = (ClientRequestArgs*)args;
   int client_sock = client_args->client_sock;
   free(client_args);

   char buffer[BUFFER_SIZE];
   memset(buffer, 0, BUFFER_SIZE);

   // Receive the client's CRUD request
   recv(client_sock, buffer, BUFFER_SIZE, 0);
   printf("Received client request: %s\n", buffer);

   // Example of handling a READ operation
   if (strncmp(buffer, "READ", 4) == 0) {
      char requested_path[BUFFER_SIZE];
      sscanf(buffer, "READ %s", requested_path);

      // Check if the requested path exists in the storage server's list
      int found = 0;
      for (int i = 0; i < num_paths; i++) {
         if (strcmp(paths[i], requested_path) == 0) {
            found = 1;
            break;
         }
      }

      // Respond based on whether the path is found or not
      if (found) {
         snprintf(buffer, BUFFER_SIZE, "Data from file: %s - Example Data Content\n", requested_path);
      } 
      else {
         snprintf(buffer, BUFFER_SIZE, "Error: File not found\n");
      }
      send(client_sock, buffer, strlen(buffer), 0);
   }
   // Add more handling for CREATE, UPDATE, DELETE based on your requirements...

   close(client_sock);
   return NULL;
}

// Function to handle incoming requests from the Naming Server
void* listen_for_naming_server(void* args) {
   int naming_sock = *((int*)args);
   char buffer[BUFFER_SIZE];

   while (1) {
      memset(buffer, 0, BUFFER_SIZE);
      int bytes_received = recv(naming_sock, buffer, BUFFER_SIZE, 0);
      if (bytes_received <= 0) {
         printf("Connection closed by Naming Server.\n");
         break; // Exit if the naming server closes the connection
      }
      printf("Received from Naming Server: %s\n", buffer);

      // Handle different types of requests from the naming server
      // Example: Updating paths or responding to requests
      if (strncmp(buffer, "UPDATE_PATH", 11) == 0) {
         // Logic to update paths
         char new_path[BUFFER_SIZE];
         sscanf(buffer, "UPDATE_PATH %s", new_path);
         strncpy(paths[num_paths], new_path, BUFFER_SIZE);
         num_paths++;
         printf("Updated paths list with: %s\n", new_path);
      }
      // Handle other naming server commands as needed...
   }

   close(naming_sock);
   return NULL;
}


// Function to register with the Naming Server
void register_with_naming_server(const char* naming_server_ip, int naming_server_port, int storage_server_port) {
   int naming_sock = socket(AF_INET, SOCK_STREAM, 0);
   struct sockaddr_in naming_addr;
   naming_addr.sin_family = AF_INET;
   naming_addr.sin_port = htons(naming_server_port);
   inet_pton(AF_INET, naming_server_ip, &naming_addr.sin_addr);

   // Connect to the Naming Server
   if (connect(naming_sock, (struct sockaddr*)&naming_addr, sizeof(naming_addr)) == -1) {
      perror("Could not connect to Naming Server");
      exit(1);
   }

   // Prepare the registration message
   char buffer[BUFFER_SIZE];
   snprintf(buffer, BUFFER_SIZE, "STORAGE %s %d ", naming_server_ip, storage_server_port);

   // Add the list of paths to the registration message
   for (int i = 0; i < num_paths; i++) {
      strcat(buffer, paths[i]);
      if (i < num_paths - 1) {
         strcat(buffer, ",");
      }
   }
   // Send the registration information to the Naming Server
   send(naming_sock, buffer, strlen(buffer), 0);
   printf("Registered with Naming Server: %s\n", buffer);

   // Create a thread to listen for messages from the naming server
   pthread_t naming_thread;
   pthread_create(&naming_thread, NULL, listen_for_naming_server, &naming_sock);
   pthread_detach(naming_thread); // Detach the thread if you don't need to join it later

   return 0; // Indicate success
}

// Main function
int main(int argc, char* argv[]) {
   if (argc < 4) {
      printf("Usage: %s <ns_ip> <ns_port> <ss_port> <paths...>\n", argv[0]);
      return 1;
   }

   const char* naming_server_ip = argv[1];
   int naming_server_port = atoi(argv[2]);
   int storage_server_port = atoi(argv[3]);

   // Read the list of paths from command line arguments
   num_paths = argc - 4;
   for (int i = 0; i < num_paths; i++) {
      strncpy(paths[i], argv[4 + i], BUFFER_SIZE);
   }

   // Register with the Naming Server
   register_with_naming_server(naming_server_ip, naming_server_port, storage_server_port);

   // Set up to listen for Client requests
   int storage_sock, client_sock;
   struct sockaddr_in storage_addr, client_addr;
   socklen_t client_addr_len = sizeof(client_addr);

   storage_sock = socket(AF_INET, SOCK_STREAM, 0);
   storage_addr.sin_family = AF_INET;
   storage_addr.sin_addr.s_addr = INADDR_ANY;
   storage_addr.sin_port = htons(storage_server_port);

   bind(storage_sock, (struct sockaddr*)&storage_addr, sizeof(storage_addr));
   listen(storage_sock, 10);
   printf("Storage Server listening on port %d...\n", storage_server_port);

   while (1) {
      // Accept connection from a client
      client_sock = accept(storage_sock, (struct sockaddr*)&client_addr, &client_addr_len);
      printf("Accepted client connection.\n");

      // Spawn a thread to handle the client's request
      ClientRequestArgs* client_args = malloc(sizeof(ClientRequestArgs));
      client_args->client_sock = client_sock;
      pthread_t client_thread;
      pthread_create(&client_thread, NULL, handle_client_request, client_args);
      pthread_detach(client_thread);
   }

   close(storage_sock);
   return 0;
}
