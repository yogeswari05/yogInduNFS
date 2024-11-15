#include "headers.h"
#include "helper.h"
#include "namingServer.h"

NamingServer naming_server;

// Hash function
unsigned int hash(const char *key) {
   unsigned int hash = 0;
   while (*key) {
      hash = (hash * 31) + *key++;
   }
   return hash % HASH_TABLE_SIZE;
}

// Initialize the hash map
void initialize_hash_map(HashMap *map) {
   for (int i = 0; i < HASH_TABLE_SIZE; i++) {
      map->table[i] = NULL;
   }
   pthread_mutex_init(&map->lock, NULL);
}

// Add a path-to-server mapping
void hash_map_insert(HashMap *map, const char *path, StorageServer *server) {
   unsigned int index = hash(path);

   pthread_mutex_lock(&map->lock);
   HashNode *new_node = malloc(sizeof(HashNode));
   strcpy(new_node->path, path);
   new_node->server = server;
   new_node->next = map->table[index];
   map->table[index] = new_node;
   pthread_mutex_unlock(&map->lock);
}

// Find a storage server by path
StorageServer* hash_map_find(HashMap *map, const char *path) {
   unsigned int index = hash(path);

   pthread_mutex_lock(&map->lock);
   HashNode *current = map->table[index];
   while (current) {
      if (strcmp(current->path, path) == 0) {
         pthread_mutex_unlock(&map->lock);
         return current->server;
      }
      current = current->next;
   }
   pthread_mutex_unlock(&map->lock);
   return NULL; // Path not found
}

// Function to print the entire hash map
void hash_map_print(HashMap *map) {
   pthread_mutex_lock(&map->lock);

   printf("Hash Map Contents:\n");
   for (int i = 0; i < HASH_TABLE_SIZE; i++) {
      HashNode *current = map->table[i];
      while (current != NULL) {
         printf("Path: %s, Server: %s:%d\n", current->path,
                  current->server->ip_address, current->server->client_port);
         current = current->next;
      }
   }

   pthread_mutex_unlock(&map->lock);
}

void handle_storage_server_registration(int client_socket) {
   printf("Storage Server registration initiated\n");
   char buffer[BUFFER_SIZE];
   StorageServer new_ss;
   new_ss.num_paths = 0;

   // Receive storage server details
   ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
   if (bytes_received > 0) {
      buffer[bytes_received] = '\0';

      // Parse registration message
      char paths_str[BUFFER_SIZE];
      int num_paths;

      // Parse the basic information (IP, nm_port, client_port, num_paths)
      // Parse the basic information (IP, nm_port, server_port, client_port, num_paths)
      int parsed_fields = sscanf(buffer, "%s %d %d %d %d %[^\n]", 
         new_ss.ip_address, 
         &new_ss.nm_port,  
         &new_ss.server_port, 
         &new_ss.client_port, 
         &num_paths, 
         paths_str);

      if (parsed_fields < 5) {
         const char *error = "Invalid registration format";
         send(client_socket, error, strlen(error), 0);
         return;
      }

       // Parse accessible paths (space-separated, not comma-separated)
      char *path = strtok(paths_str, " ");
      while (path != NULL && new_ss.num_paths < 10) {
         strncpy(new_ss.accessible_paths[new_ss.num_paths], path, MAX_PATH_LENGTH - 1);
         new_ss.accessible_paths[new_ss.num_paths][MAX_PATH_LENGTH - 1] = '\0';  // Null-terminate
         new_ss.num_paths++;
         path = strtok(NULL, " ");
      }

      new_ss.socket = client_socket;
      new_ss.is_active = 1;

      // Add to storage servers list
      pthread_mutex_lock(&naming_server.lock);
      if (naming_server.num_storage_servers < MAX_STORAGE_SERVERS) {
         naming_server.storage_servers[naming_server.num_storage_servers++] = new_ss;
         printf("Storage Server registered: %s:%d\n", new_ss.ip_address, new_ss.nm_port);
         printf("Accessible paths:\n");
         for (int i = 0; i < new_ss.num_paths; i++) {
            printf("  %s\n", new_ss.accessible_paths[i]);
         }
         // Add paths to hash map
         for (int i = 0; i < new_ss.num_paths; i++){
            hash_map_insert(&naming_server.path_to_server_map,
                            new_ss.accessible_paths[i],
                            &naming_server.storage_servers[naming_server.num_storage_servers - 1]);
         }

         printf("Storage Server registered: %s:%d\n", new_ss.ip_address, new_ss.nm_port);

         // Send acknowledgment
         const char *ack = "Registration successful";
         send(client_socket, ack, strlen(ack), 0);
      } 
      else {
         const char *error = "Maximum number of storage servers reached";
         send(client_socket, error, strlen(error), 0);
      }
      pthread_mutex_unlock(&naming_server.lock);
      // Print the entire hash map to verify
      hash_map_print(&naming_server.path_to_server_map);
   } 
   else {
      perror("Failed to receive registration message");
   }
}


void handle_client_request(int client_socket) {
   printf("Client request\n");
   char buffer[BUFFER_SIZE];
   
   while (1) {
      memset(buffer, 0, BUFFER_SIZE);
      ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
      
      if (bytes_received <= 0) {
         // printf("")
         // Client disconnected
         break;
      }      
      buffer[bytes_received] = '\0';

      printf(" buffer: %s\n",buffer);
      
      // Parse client request
      char command[32];
      char path[MAX_PATH_LENGTH];
      sscanf(buffer, "%s %s", command, path);      
      if (strcmp(command, "GET_SERVER") == 0) {
         printf("entered\n");
         // Find appropriate storage server
         pthread_mutex_lock(&naming_server.lock);
         StorageServer *server = hash_map_find(&naming_server.path_to_server_map, path);

         if (server) {
            printf("storage Server found\n");
            char response[BUFFER_SIZE];
            sprintf(response, "%s %d", server->ip_address, server->client_port);
            if(send(client_socket, response, strlen(response), 0) < 0){
               perror("Failed to send server info to client");
            }
         } 
         else {
            printf("No storage server found\n");
            const char *error = "No server found for the requested path";
            send(client_socket, error, strlen(error), 0);
         }
         pthread_mutex_unlock(&naming_server.lock);
      }
   }   
   close(client_socket);
}

void* connection_handler(void* socket_desc) {
   int client_socket = *(int*)socket_desc;
   free(socket_desc);
   
   // First message determines if it's a storage server or client
   char buffer[BUFFER_SIZE];
   ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
   
   if (bytes_received > 0) {
      buffer[bytes_received] = '\0';
      printf("Received message: %s\n", buffer);
      if (strcmp(buffer, "STORAGE_SERVER") == 0) {
         handle_storage_server_registration(client_socket);
         printf("Storage Server registration..\n");
      } 
      else if (strcmp(buffer, "CLIENT") == 0) {
         handle_client_request(client_socket);
      }
   }
   else{
      perror("recv failed");
   }
   
   return NULL;
}

int main(int argc, char *argv[]) {
   if (argc != 2) {
      printf("Usage: %s <port>\n", argv[0]);
      return 1;
   }

   char ip_address[16] = {0};
   int port = atoi(argv[1]);

   // Get the local IP address
   get_local_ip(ip_address);

   printf("Naming Server will use IP Address: %s and Port: %d\n", ip_address, port);

   int server_socket;
   struct sockaddr_in server_addr;

   // Initialize naming server
   pthread_mutex_init(&naming_server.lock, NULL);
   naming_server.num_storage_servers = 0;

   // Create socket
   server_socket = socket(AF_INET, SOCK_STREAM, 0);
   if (server_socket < 0) {
      perror("Socket creation failed");
      return 1;
   }

   // Configure server address
   memset(&server_addr, 0, sizeof(server_addr));
   server_addr.sin_family = AF_INET;
   server_addr.sin_addr.s_addr = inet_addr(ip_address); // Use the retrieved local IP address
   server_addr.sin_port = htons(port);

   // Bind socket
   if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
      perror("Bind failed");
      return 1;
   }

   // Listen for connections
   if (listen(server_socket, MAX_STORAGE_SERVERS + MAX_CLIENTS) < 0) {
      perror("Listen failed");
      return 1;
   }

   printf("Naming Server started on %s:%d\n", ip_address, port);

   while (1) {
      struct sockaddr_in client_addr;
      socklen_t addr_len = sizeof(client_addr);
      int *client_socket = malloc(sizeof(int));
      *client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len);

      if (*client_socket < 0) {
         perror("Accept failed");
         free(client_socket);
         continue;
      }

      printf("New connection from %s:%d\n",
            inet_ntoa(client_addr.sin_addr),
            ntohs(client_addr.sin_port));

      // Create thread to handle connection
      pthread_t thread_id;
      if (pthread_create(&thread_id, NULL, connection_handler, (void *)client_socket) < 0) {
         perror("Thread creation failed");
         free(client_socket);
         continue;
      } 
      else {
         printf("Thread created\n");
      }

      pthread_detach(thread_id);
   }

   close(server_socket);
   pthread_mutex_destroy(&naming_server.lock);
   return 0;
}