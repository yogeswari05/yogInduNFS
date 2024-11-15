// client.c
#include "headers.h"

#define BUFFER_SIZE 4096
#define MAX_PATH_LENGTH 256

typedef struct {
   char ip[16];
   int port;
   int socket;  // Store the socket connection to the storage server
} ServerInfo;

// Global variables for persistent connections
ServerInfo current_server;
int naming_server_socket = -1;

// Function to connect to naming server once
int connect_to_naming_server(const char* nm_ip, int nm_port) {
   if (naming_server_socket != -1) {
      return naming_server_socket;  // Return existing connection
   }
   naming_server_socket = socket(AF_INET, SOCK_STREAM, 0);
   struct sockaddr_in nm_addr;

   memset(&nm_addr, 0, sizeof(nm_addr));
   nm_addr.sin_family = AF_INET;
   nm_addr.sin_addr.s_addr = inet_addr(nm_ip);
   nm_addr.sin_port = htons(nm_port);

   if (connect(naming_server_socket, (struct sockaddr*)&nm_addr, sizeof(nm_addr)) < 0) {
      perror("Connection to naming server failed");
      naming_server_socket = -1;
      return -1;
   }
   // Identify as client (only once)
   const char* client_type = "CLIENT";
   send(naming_server_socket, client_type, strlen(client_type), 0);

   return naming_server_socket;
}

// Function to get storage server details from naming server
ServerInfo get_storage_server(const char* nm_ip, int nm_port, const char* path, int nm_socket) {
   ServerInfo server_info;
   memset(&server_info, 0, sizeof(ServerInfo));

   // Send path request to naming server
   char request[BUFFER_SIZE];
   sprintf(request, "GET_SERVER %s", path);
   send(nm_socket, request, strlen(request), 0);

   // Receive server info from naming server
   char response[BUFFER_SIZE];
   ssize_t bytes_received = recv(nm_socket, response, BUFFER_SIZE - 1, 0);
   if (bytes_received <= 0) {
      printf("Failed to receive response from naming server\n");
      return server_info;
   }

   response[bytes_received] = '\0';
   printf("(client)Received response: %s\n", response);
   sscanf(response, "%s %d", server_info.ip, &server_info.port);
   return server_info;
}

// Create file or directory on storage server
void create_item(ServerInfo server, const char* path, int is_directory) {
   char buffer[BUFFER_SIZE];

   sprintf(buffer, "CREATE %s%s", path, is_directory ? "/" : "");
   send(server.socket, buffer, strlen(buffer), 0);

   memset(buffer, 0, BUFFER_SIZE);
   recv(server.socket, buffer, BUFFER_SIZE - 1, 0);
   printf("Create response: %s\n", buffer);
}


// Cleanup connections before exit
void cleanup_connections() {
   if (naming_server_socket != -1) {
      close(naming_server_socket);
   }
   if (current_server.socket != -1) {
      close(current_server.socket);
   }
}

// Function to connect to the storage server
int connect_to_storage_server(ServerInfo *server){
   printf("Connecting to storage server %s:%d\n", server->ip, server->port);
   if (server->socket != -1){
      return server->socket; // Return existing connection if already connected
   }

   // Create socket for storage server connection
   server->socket = socket(AF_INET, SOCK_STREAM, 0);
   if (server->socket < 0){
      perror("Error creating socket for storage server");
      return -1;
   }


   struct sockaddr_in server_addr;
   memset(&server_addr, 0, sizeof(server_addr));
   server_addr.sin_family = AF_INET;
   server_addr.sin_addr.s_addr = inet_addr(server->ip);
   server_addr.sin_port = htons(server->port);

   if (connect(server->socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
      perror("Error connecting to storage server");
      return -1;
   }
   else{
      printf("Connected to storage server\n");
   }
   printf("Connected to storage server\n");
   return server->socket;
}

// Function to read the file from the storage server
void read_file(int server_socket, const char *file_path) {
   // Send the file path to the server to request the file
   if (send(server_socket, file_path, strlen(file_path) + 1, 0) < 0) {
      perror("Failed to send file path to server");
      close(server_socket);
      return;
   }
   else{
      printf("Requesting ss to read file: %s\n", file_path);
   }

   // Buffer to receive the file content
   char buffer[BUFFER_SIZE];
   ssize_t bytes_received;

   // Receive the file content from the server
   while ((bytes_received = recv(server_socket, buffer, sizeof(buffer) - 1, 0)) > 0) {
      buffer[bytes_received] = '\0'; // Null-terminate the received data for string operations

      // Check for the "END_OF_FILE" marker
      if (strstr(buffer, "END_OF_FILE") != NULL) {
         // Print up to the "END_OF_FILE" marker and break
         char *end_marker = strstr(buffer, "END_OF_FILE");
         fwrite(buffer, 1, end_marker - buffer, stdout);
         printf("\nFile transfer complete\n");
         break;
      }
      // Print the received data
      fwrite(buffer, 1, bytes_received, stdout);
   }
   if (bytes_received < 0) {
      perror("Error receiving file content");
   }
   close(server_socket);
}

void write_file(int server_socket, const char *file_path, const char *data) {
   // Send the file path to the server
   if (send(server_socket, file_path, strlen(file_path) + 1, 0) < 0) {
      perror("Failed to send file path to server");
      close(server_socket);
      return;
   }
   // Send the data to be written
   if (send(server_socket, data, strlen(data) + 1, 0) < 0) {
      perror("Failed to send data to server");
      close(server_socket);
      return;
   }
   // Check server's response
   char response[256];
   ssize_t bytes_received = recv(server_socket, response, sizeof(response), 0);
   if (bytes_received > 0) {
      response[bytes_received] = '\0';
      printf("Server Response: %s\n", response);
   } 
   else {
      perror("Failed to receive server response");
   }
   close(server_socket);
}

void get_file_info(int server_socket, const char *file_path) {
   // Send the file path to the server
   if (send(server_socket, file_path, strlen(file_path) + 1, 0) < 0) {
      perror("Failed to send file path to server");
      close(server_socket);
      return;
   }
   // Receive file information from the server
   char response[256];
   ssize_t bytes_received = recv(server_socket, response, sizeof(response), 0);
   if (bytes_received > 0) {
      response[bytes_received] = '\0';
      printf("File Info: %s\n", response);
   } else {
      perror("Failed to receive file info");
   }
   close(server_socket);
}

void stream_audio_file(int server_socket, const char *file_path) {
   // Send the file path to the server
   if (send(server_socket, file_path, strlen(file_path) + 1, 0) < 0) {
      perror("Failed to send file path to server");
      close(server_socket);
      return;
   }
   // Stream audio data from the server
   char buffer[BUFFER_SIZE];
   ssize_t bytes_received;

   printf("Streaming audio...\n");
   while ((bytes_received = recv(server_socket, buffer, sizeof(buffer), 0)) > 0) {
      fwrite(buffer, 1, bytes_received, stdout);  // Assuming stdout is redirected to a player
   }
   if (bytes_received < 0) {
      perror("Error streaming audio file");
   }
    else {
      printf("\nAudio streaming complete\n");
   }
   close(server_socket);
}



int main(int argc, char *argv[]) {
   if (argc != 3) {
      printf("Usage: %s <naming_server_ip> <naming_server_port>\n", argv[0]);
      return 1;
   }

   char* nm_ip = argv[1];
   int nm_port = atoi(argv[2]);

   int nm_socket = connect_to_naming_server(nm_ip, nm_port);

   // Initial connection to the naming server
   if ( nm_socket < 0) {
      printf("Unable to connect to naming server\n");
      return 1;
   }

   printf("Connected to naming server\n");

   while (1) {
      printf("\nEnter command: ");
      char line[BUFFER_SIZE];
      char command[32];
      char path[MAX_PATH_LENGTH];

      if (fgets(line, BUFFER_SIZE, stdin) == NULL) break;

      command[0] = '\0';
      path[0] = '\0';

      //parsing
      sscanf(line, "%s %s", command, path);

      if (strlen(command) == 0) continue;

      if (strcmp(command, "EXIT") == 0) {
         break;
      }
       // Handle READ command
      if (strcmp(command, "READ") == 0) {
         // Get storage server details first
         ServerInfo storage_server = get_storage_server(nm_ip, nm_port, path, nm_socket);
         if (storage_server.socket == -1) {
            printf("Failed to get storage server details\n");
            continue;
         }
         int server_socket = connect_to_storage_server(&storage_server);
         if (server_socket == -1) {
            printf("Failed to connect to storage server\n");
            continue;
         }
         else{
            printf("(main) Connected to storage server\n");
         }
         read_file(server_socket, path);
      } 
      else if (strcmp(command, "STREAM") == 0) {
         ServerInfo storage_server = get_storage_server(nm_ip, nm_port, path, nm_socket);
         if (storage_server.socket == -1) {
            printf("Failed to get storage server details\n");
            continue;
         }
         int server_socket = connect_to_storage_server(&storage_server);
         if (server_socket == -1) {
            printf("Failed to connect to storage server\n");
            continue;
         }
         stream_audio_file(server_socket, path);
      } 
      else {
         printf("Unknown command\n");
      }
   }

   cleanup_connections();
   return 0;
   return 0;
}
