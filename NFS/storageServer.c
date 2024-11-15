#include "headers.h"
#include "helper.h"
#include "storageServer.h"

// Add this function to get all accessible paths
void get_accessible_paths(const char* base_path, char paths[][MAX_PATH_LENGTH], int* num_paths) {
   DIR* dir = opendir(base_path);
   if (dir == NULL) {
      printf("Failed to open directory: %s\n", base_path);
      return;
   }
   struct dirent* entry;
   char full_path[MAX_PATH_LENGTH];

   while ((entry = readdir(dir)) != NULL) {
      // Skip . and .. directories
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
         continue;
      }
      // Construct full path for the file/directory
      snprintf(full_path, MAX_PATH_LENGTH, "%s/%s", base_path, entry->d_name);

      if (entry->d_type == DT_REG) {
         snprintf(paths[*num_paths], MAX_PATH_LENGTH, "%s", full_path);
         (*num_paths)++;
      } else if (entry->d_type == DT_DIR) {
         get_accessible_paths(full_path, paths, num_paths);
      }
   }
   closedir(dir);
}

void handle_create(const char* path) {
   FILE* file = fopen(path, "w");
   if (file != NULL) {
      fclose(file);
      printf("Created file: %s\n", path);
   } else {
      perror("File creation failed");
   }
}

void handle_delete(const char* path) {
   if (remove(path) == 0) {
      printf("Deleted: %s\n", path);
   } else {
      perror("Delete failed");
   }
}

void handle_read(int client_socket, const char* path) {
   char buffer[BUFFER_SIZE];
   FILE* file = fopen(path, "rb");

   if (file == NULL) {
      const char* error = "Error opening file";
      send(client_socket, error, strlen(error), 0);
      return;
   }

   size_t bytes_read;
   while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
      send(client_socket, buffer, bytes_read, 0);
   }

   fclose(file);
}

void handle_write(int client_socket, const char* path) {
   char buffer[BUFFER_SIZE];
   FILE* file = fopen(path, "wb");

   if (file == NULL) {
      const char* error = "Error creating file";
      send(client_socket, error, strlen(error), 0);
      return;
   }

   ssize_t bytes_received;
   while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
      fwrite(buffer, 1, bytes_received, file);
      if (bytes_received < BUFFER_SIZE) break;
   }

   fclose(file);
   const char* success = "File written successfully";
   send(client_socket, success, strlen(success), 0);
}

void* handle_client(void* arg) {
   ClientHandler* handler = (ClientHandler*)arg;
   char buffer[BUFFER_SIZE];

   while (1) {
      memset(buffer, 0, BUFFER_SIZE);
      ssize_t bytes_received = recv(handler->client_socket, buffer, BUFFER_SIZE - 1, 0);

      if (bytes_received <= 0) break;

      buffer[bytes_received] = '\0';

      char command[32];
      char path[MAX_PATH_LENGTH];
      sscanf(buffer, "%s %s", command, path);

      // Construct full path
      char full_path[MAX_PATH_LENGTH];
      snprintf(full_path, sizeof(full_path), "%s", path);

      if (strcmp(command, "CREATE") == 0) {
         handle_create(full_path);
      } else if (strcmp(command, "DELETE") == 0) {
         handle_delete(full_path);
      } else if (strcmp(command, "READ") == 0) {
         handle_read(handler->client_socket, full_path);
      } else if (strcmp(command, "WRITE") == 0) {
         handle_write(handler->client_socket, full_path);
      }
   }

   close(handler->client_socket);
   free(handler);
   return NULL;
}

void* handle_naming_server(void* arg) {
   NamingServerHandler* handler = (NamingServerHandler*)arg;
   char buffer[BUFFER_SIZE];

   while (1) {
      memset(buffer, 0, BUFFER_SIZE);
      ssize_t bytes_received = recv(handler->nm_socket, buffer, BUFFER_SIZE - 1, 0);

      if (bytes_received <= 0) {
         perror("Lost connection to naming server");
         break;
      }

      buffer[bytes_received] = '\0';
      printf("Message from Naming Server: %s\n", buffer);

      // Handle commands from naming server if any
   }

   close(handler->nm_socket);
   free(handler);
   return NULL;
}

int main(int argc, char *argv[]) {
   if (argc < 6){
      printf("Usage: %s <naming_server_ip> <naming_server_port> <ss_port> <port_for_clients> <base_path>\n", argv[0]);
      return 1;
   }

   char *nm_ip = argv[1];           // Naming server IP
   int nm_port = atoi(argv[2]);     // Naming server port
   int sn_server_port = atoi(argv[3]); // Storage server port
   int client_port = atoi(argv[4]); // Client communication port

   // Get local IP address
   char server_ip[16] = {0};
   get_local_ip(server_ip);

   // Connect to Naming Server
   int nm_socket = socket(AF_INET, SOCK_STREAM, 0);
   if (nm_socket < 0) {
      perror("Socket creation failed");
      return 1;
   }
   
   // Bind to the specific source IP and port
   struct sockaddr_in source_addr;
   memset(&source_addr, 0, sizeof(source_addr));
   source_addr.sin_family = AF_INET;
   source_addr.sin_addr.s_addr = inet_addr(server_ip); // Use the server's IP
   source_addr.sin_port = htons(sn_server_port);       // Use the specified port

   if (bind(nm_socket, (struct sockaddr *)&source_addr, sizeof(source_addr)) < 0) {
      perror("Bind to source IP and port failed");
      return 1;
   }

   // Configure Naming Server address
   struct sockaddr_in nm_addr;
   memset(&nm_addr, 0, sizeof(nm_addr));
   nm_addr.sin_family = AF_INET;
   nm_addr.sin_addr.s_addr = inet_addr(nm_ip);
   nm_addr.sin_port = htons(nm_port);

   if (connect(nm_socket, (struct sockaddr *)&nm_addr, sizeof(nm_addr)) < 0) {
      perror("Connection to Naming Server failed");
      return 1;
   }

   // Send registration type
   const char *reg_type = "STORAGE_SERVER";
   if (send(nm_socket, reg_type, strlen(reg_type), 0) < 0) {
      perror("Initial registration send failed");
   } else {
      printf("Registration type sent to Naming Server\n");
   }

   // Create and send registration message
   char reg_msg[BUFFER_SIZE];
   char paths_str[BUFFER_SIZE] = "";

    // Create an array to store accessible paths
   char accessible_paths[100][MAX_PATH_LENGTH];
   int num_paths = 0;

   // Loop through all the remaining arguments to process base_path and directories/files
   printf("Count of files/directories: %d\n", argc - 5);
   for (int i = 5; i < argc; i++) {
      char *path = argv[i];
      printf("Processing path: %s\n", path);
      // Try to open the directory
      DIR *dir = opendir(path);
      if (dir != NULL) {
         // It's a directory, so get all files in the directory
         get_accessible_paths(path, accessible_paths, &num_paths);
         closedir(dir);
      } else {
         // It's a file, so just add the file path to the accessible paths array
         snprintf(accessible_paths[num_paths], MAX_PATH_LENGTH, "%s", path);
         num_paths++;
      }
   }
   // Print out all accessible paths
   printf("\nAll accessible paths:\n");
   for (int i = 0; i < num_paths; i++) {
      printf("%s\n", accessible_paths[i]);
   }
   // Start with the number of paths as the first part of the message
   sprintf(reg_msg, "%s %d %d %d %d", server_ip, nm_port, sn_server_port, client_port, num_paths);

   // Append each path to the message
   for (int i = 0; i < num_paths; i++) {
      strcat(reg_msg, " ");
      strcat(reg_msg, accessible_paths[i]);
   }
   
   ssize_t bytesSent = send(nm_socket, reg_msg, strlen(reg_msg), 0);
   if (bytesSent < 0) {
      perror("Registration send failed");
   } 
   else {
      printf("Storage Server registered. Message: %s\n", reg_msg);
   }

   // Start Naming Server handler thread
   NamingServerHandler *nm_handler = malloc(sizeof(NamingServerHandler));
   nm_handler->nm_socket = nm_socket;

   pthread_t nm_thread_id;
   pthread_create(&nm_thread_id, NULL, handle_naming_server, nm_handler);

   // Start Client Server
   int server_socket = socket(AF_INET, SOCK_STREAM, 0);
   if (server_socket < 0) {
      perror("Socket creation failed");
      return 1;
   }

   struct sockaddr_in server_addr;
   memset(&server_addr, 0, sizeof(server_addr));
   server_addr.sin_family = AF_INET;
   server_addr.sin_addr.s_addr = INADDR_ANY;
   server_addr.sin_port = htons(client_port);

   if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
      perror("Bind failed");
      return 1;
   }

   if (listen(server_socket, MAX_CLIENTS) < 0) {
      perror("Listen failed");
      return 1;
   }

   printf("Storage Server started. Listening for clients on port %d\n", client_port);

   while (1) {
      struct sockaddr_in client_addr;
      socklen_t addr_len = sizeof(client_addr);
      int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len);

      if (client_socket < 0) {
         perror("Accept failed");
         continue;
      }

      ClientHandler *handler = malloc(sizeof(ClientHandler));
      handler->client_socket = client_socket;

      pthread_t thread_id;
      pthread_create(&thread_id, NULL, handle_client, handler);
      pthread_detach(thread_id);
   }

   close(server_socket);
   close(nm_socket);
   return 0;
}