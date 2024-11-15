#ifndef _NS_H_
#define _NS_H_

#define MAX_STORAGE_SERVERS 10
#define MAX_CLIENTS 50
#define BUFFER_SIZE 1024
#define MAX_PATH_LENGTH 256
#define HASH_TABLE_SIZE 100 // Size of the hash map

typedef struct {
   char ip_address[16];
   int nm_port;
   int client_port;
   int server_port;
   char accessible_paths[10][MAX_PATH_LENGTH];
   int num_paths;
   int socket;
   int is_active;
} StorageServer;

typedef struct HashNode {
    char path[MAX_PATH_LENGTH];        // Key: Path
    StorageServer *server;             // Value: Pointer to StorageServer
    struct HashNode *next;             // Linked list for collision handling
} HashNode;

typedef struct {
    HashNode *table[HASH_TABLE_SIZE];  // Hash table array
    pthread_mutex_t lock;              // Mutex for thread safety
} HashMap;


typedef struct {
    StorageServer storage_servers[MAX_STORAGE_SERVERS];
    int num_storage_servers;
    HashMap path_to_server_map;        // Hash map for path-to-server mapping
    pthread_mutex_t lock;
} NamingServer;

#endif