// client.c (Updated for multiple requests)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

void perform_crud_operations(int storage_sock) {
   char buffer[BUFFER_SIZE];
   while (1) {
      memset(buffer, 0, BUFFER_SIZE);
      printf("Enter operation (e.g., READ <path>, CREATE <path>, EXIT to quit): ");
      fgets(buffer, BUFFER_SIZE, stdin);
      
      // If the user wants to exit, break the loop
      if (strncmp(buffer, "EXIT", 4) == 0) {
         printf("Exiting...\n");
         break;
      }
      
      // Send operation to the Storage Server
      send(storage_sock, buffer, strlen(buffer), 0);
      
      // Receive response from Storage Server
      memset(buffer, 0, BUFFER_SIZE);
      recv(storage_sock, buffer, BUFFER_SIZE, 0);
      printf("Response from Storage Server: %s\n", buffer);
   }
}

int main(int argc, char *argv[]) {
   if (argc < 3) {
      printf("Usage: %s <Naming Server IP> <Naming Server Port>\n", argv[0]);
      return 1;
   }

   char *naming_ip = argv[1];
   int naming_port = atoi(argv[2]);
   char buffer[BUFFER_SIZE], storage_ip[INET_ADDRSTRLEN];
   int storage_port;
   int naming_sock, storage_sock;
   struct sockaddr_in naming_addr, storage_addr;

   while (1) {
      // Step 1: Connect to Naming Server
      naming_sock = socket(AF_INET, SOCK_STREAM, 0);
      naming_addr.sin_family = AF_INET;
      naming_addr.sin_addr.s_addr = inet_addr(naming_ip);
      naming_addr.sin_port = htons(naming_port);

      connect(naming_sock, (struct sockaddr *)&naming_addr, sizeof(naming_addr));

      // Identify as CLIENT and request path information
      send(naming_sock, "CLIENT", strlen("CLIENT"), 0);
      sleep(1); // Ensure server processes identifier first

      // Get the desired file path from the user
      printf("Enter file path for operation (or type EXIT to quit): ");
      fgets(buffer, BUFFER_SIZE, stdin);
      if (strncmp(buffer, "EXIT", 4) == 0) {
         close(naming_sock);
         break;
      }

      // Remove newline character
      buffer[strcspn(buffer, "\n")] = 0;
      
      // Send path to Naming Server
      send(naming_sock, buffer, strlen(buffer), 0);

      // Receive Storage Server details from Naming Server
      memset(buffer, 0, BUFFER_SIZE);
      recv(naming_sock, buffer, BUFFER_SIZE, 0);
      printf("Received Storage Server Info: %s\n", buffer);

      if (sscanf(buffer, "Storage IP: %s, Client Port: %d", storage_ip, &storage_port) != 2) {
         printf("No valid storage server found for the given path.\n");
         close(naming_sock);
         continue;
      }
      close(naming_sock);

      // Step 2: Connect to Storage Server
      storage_sock = socket(AF_INET, SOCK_STREAM, 0);
      storage_addr.sin_family = AF_INET;
      storage_addr.sin_addr.s_addr = inet_addr(storage_ip);
      storage_addr.sin_port = htons(storage_port);

      if (connect(storage_sock, (struct sockaddr *)&storage_addr, sizeof(storage_addr)) < 0) {
         printf("Failed to connect to Storage Server.\n");
         continue;
      }

      // Perform multiple CRUD operations
      perform_crud_operations(storage_sock);

      // Close connection to the storage server after operations
      close(storage_sock);
   }

   return 0;
}
