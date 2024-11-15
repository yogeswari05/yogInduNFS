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
   else{
      printf(" recv success \n");
      printf("response1 : %s\n", response);
   }


      printf("response2 : %s\n", response);
   response[bytes_received] = '\0';
      printf("response3 : %s\n", response);
   sscanf(response, "%s %d", server_info.ip, &server_info.port);
      printf("response4 : %s\n", response);
   printf("response, %s %d\n", server_info.ip, server_info.port);
   // printf("response : %s\n", response);


   return server_info;
}

// Rest of the functions for file operations (upload, download, etc.) remain unchanged.

// Upload file to storage server
void upload_file(ServerInfo server, const char* local_path, const char* remote_path) {
   char buffer[BUFFER_SIZE];

   // Send write command
   sprintf(buffer, "WRITE %s", remote_path);
   send(server.socket, buffer, strlen(buffer), 0);

   // Open local file
   FILE* file = fopen(local_path, "rb");
   if (file == NULL) {
      perror("Error opening local file");
      return;
   }

   // Send file content
   size_t bytes_read;
   while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
      send(server.socket, buffer, bytes_read, 0);
   }

   fclose(file);

   // Receive confirmation
   memset(buffer, 0, BUFFER_SIZE);
   recv(server.socket, buffer, BUFFER_SIZE - 1, 0);
   printf("Upload response: %s\n", buffer);
}

// Download file from storage server
void download_file(ServerInfo server, const char* remote_path, const char* local_path) {
   char buffer[BUFFER_SIZE];

   sprintf(buffer, "READ %s", remote_path);
   send(server.socket, buffer, strlen(buffer), 0);

   FILE* file = fopen(local_path, "wb");
   if (file == NULL) {
      perror("Error creating local file");
      return;
   }

   ssize_t bytes_received;
   while ((bytes_received = recv(server.socket, buffer, BUFFER_SIZE, 0)) > 0) {
      fwrite(buffer, 1, bytes_received, file);
      if (bytes_received < BUFFER_SIZE) break;
   }

   fclose(file);
   printf("File downloaded successfully\n");
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

// Delete file or directory from storage server
void delete_item(ServerInfo server, const char* path, int nm_socket) {
   char buffer[BUFFER_SIZE];


   memset(buffer, 0, BUFFER_SIZE);
   // recv(server.socket, buffer, BUFFER_SIZE - 1, 0);
   if(recv(nm_socket, buffer, BUFFER_SIZE - 1, 0) == -1){
      printf("recv failed\n");
   }
   printf("Delete response: %s\n", buffer);

   sprintf(buffer, "DELETE %s", path);
   send(server.socket, buffer, strlen(buffer), 0);
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
int connect_to_storage_server(ServerInfo *server)
{
   if (server->socket != -1)
   {
      return server->socket; // Return existing connection if already connected
   }

   // Create socket for storage server connection
   server->socket = socket(AF_INET, SOCK_STREAM, 0);
   if (server->socket < 0)
   {
      perror("Error creating socket for storage server");
      return -1;
   }

   struct sockaddr_in server_addr;
   memset(&server_addr, 0, sizeof(server_addr));
   server_addr.sin_family = AF_INET;
   server_addr.sin_addr.s_addr = inet_addr(server->ip);
   server_addr.sin_port = htons(server->port);

   if (connect(server->socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
   {
      perror("Error connecting to storage server");
      return -1;
   }

   return server->socket;
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

   // Main loop to handle user commands
   while (1) {
      printf("\nEnter command: ");
      char line[BUFFER_SIZE];
      char command[32];
      char path1[MAX_PATH_LENGTH];
      char path2[MAX_PATH_LENGTH];

      if (fgets(line, BUFFER_SIZE, stdin) == NULL) break;

      command[0] = '\0';
      path1[0] = '\0';
      path2[0] = '\0';

      //parsing
      sscanf(line, "%s %s %s", command, path1, path2);

      if (strlen(command) == 0) continue;

      if (strcmp(command, "exit") == 0) {
         break;
      }
      // Handle commands based on the user's input
      if (strcmp(command, "upload") == 0) {
         // Get storage server details first
         ServerInfo storage_server = get_storage_server(nm_ip, nm_port, path1, nm_socket);
         if (storage_server.socket == -1) {
            printf("Failed to get storage server details\n");
            continue;
         }
         
         // Connect to the storage server
         int server_socket = connect_to_storage_server(&storage_server);
         if (server_socket == -1) {
            printf("Failed to connect to storage server\n");
            continue;
         }

         upload_file(storage_server, path1, path2);
      } 
      else if (strcmp(command, "download") == 0) {
         // Get storage server details first
         ServerInfo storage_server = get_storage_server(nm_ip, nm_port, path1, nm_socket);
         if (storage_server.socket == -1) {
            printf("Failed to get storage server details\n");
            continue;
         }

         // Connect to the storage server
         int server_socket = connect_to_storage_server(&storage_server);
         if (server_socket == -1) {
            printf("Failed to connect to storage server\n");
            continue;
         }

         download_file(storage_server, path1, path2);
      } 
      else if (strcmp(command, "create_file") == 0) {
         // Get storage server details first
         ServerInfo storage_server = get_storage_server(nm_ip, nm_port, path1, nm_socket);
         if (storage_server.socket == -1) {
            printf("Failed to get storage server details\n");
            continue;
         }

         // Connect to the storage server
         int server_socket = connect_to_storage_server(&storage_server);
         if (server_socket == -1) {
            printf("Failed to connect to storage server\n");
            continue;
         }

         create_item(storage_server, path1, 0); // 0 = file
      } 
      else if (strcmp(command, "create_dir") == 0) {
         // Get storage server details first
         ServerInfo storage_server = get_storage_server(nm_ip, nm_port, path1, nm_socket);
         if (storage_server.socket == -1) {
            printf("Failed to get storage server details\n");
            continue;
         }

         // Connect to the storage server
         int server_socket = connect_to_storage_server(&storage_server);
         if (server_socket == -1) {
            printf("Failed to connect to storage server\n");
            continue;
         }

         create_item(storage_server, path1, 1); // 1 = directory
      } 
      else if (strcmp(command, "delete") == 0) {
         // Get storage server details first
         ServerInfo storage_server = get_storage_server(nm_ip, nm_port, path1, nm_socket);
         if (storage_server.socket == -1) {
            printf("Failed to get storage server details\n");
            continue;
         }

         // Connect to the storage server
         int server_socket = connect_to_storage_server(&storage_server);
         if (server_socket == -1) {
            printf("Failed to connect to storage server\n");
            continue;
         }

         delete_item(storage_server, path1, nm_socket);
      } 
      else {
         printf("Unknown command\n");
      }
   }

   cleanup_connections();
   return 0;
   return 0;
}
