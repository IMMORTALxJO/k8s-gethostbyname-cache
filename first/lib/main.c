#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dlfcn.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

// Configuration
#define CACHE_MAX_SIZE 100
#define CACHE_ITEM_TTL 30 // seconds
#define CACHE_ITME_MAX_IP_ADDRESSES 10 // Maximum number of addresses per hostname

// Cache item structure
typedef struct cache_item {
    char *hostname;
    struct hostent result;
    char *aliases[2]; // Simplification: assuming one alias
    char *addr_list[CACHE_ITME_MAX_IP_ADDRESSES + 1]; // Assuming a maximum of CACHE_ITME_MAX_IP_ADDRESSES addresses
    time_t timestamp;
    int addr_count; // Number of addresses
} cache_item;

// Hash map node
typedef struct node {
    cache_item *item;
    struct node *next;
} node;

// Global variables
node *cache_map[CACHE_MAX_SIZE] = {NULL};
int cache_count = 0;

// Function prototypes
struct hostent *cached_gethostbyname_r(const char *name, struct hostent *ret, char *buf, size_t buflen, int *h_errnop);

// Hash function for simplicity
unsigned long hash(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash % CACHE_MAX_SIZE;
}

// Insert into cache
void cache_insert(const char *hostname, struct hostent *result) {
    unsigned long index = hash(hostname);
    node *new_node = (node *)malloc(sizeof(node));
    cache_item *new_item = (cache_item *)malloc(sizeof(cache_item));

    // Simplified copy, for demonstration
    new_item->hostname = strdup(hostname);
    new_item->result = *result; // Shallow copy, for simplicity
    new_item->timestamp = time(NULL);
    new_item->addr_count = 0;
    while (result->h_addr_list[new_item->addr_count] != NULL && new_item->addr_count < CACHE_ITME_MAX_IP_ADDRESSES) {
        new_item->addr_list[new_item->addr_count] = result->h_addr_list[new_item->addr_count];
        new_item->addr_count++;
    }
    new_item->addr_list[new_item->addr_count] = NULL; // Null-terminate the list

    new_node->item = new_item;
    new_node->next = cache_map[index];
    cache_map[index] = new_node;

    if (cache_count >= CACHE_MAX_SIZE) {
        fprintf(stderr, "Cache full, consider increasing CACHE_MAX_SIZE\n");
    } else {
        cache_count++;
    }
    fprintf(stderr, "Added to cache: %s\n", hostname);
}

// Search in cache
cache_item *cache_search(const char *hostname) {
    unsigned long index = hash(hostname);
    node *current = cache_map[index];

    while (current != NULL) {
        if (strcmp(current->item->hostname, hostname) == 0) {
            if (difftime(time(NULL), current->item->timestamp) < CACHE_ITEM_TTL) {
                fprintf(stderr, "Cache hit: %s\n", hostname);
                return current->item;
            } else {
                fprintf(stderr, "Cache item expired: %s\n", hostname);
                return NULL;
            }
        }
        current = current->next;
    }
    fprintf(stderr, "Cache miss: %s\n", hostname);
    return NULL;
}

// Wrapper function
int gethostbyname_r(const char *name, struct hostent *ret, char *buf, size_t buflen, struct hostent **result, int *h_errnop) {
    static int (*original_gethostbyname_r)(const char *, struct hostent *, char *, size_t, struct hostent **, int *) = NULL;

    if (!original_gethostbyname_r) {
        original_gethostbyname_r = dlsym(RTLD_NEXT, "gethostbyname_r");
    }

    // Try to get from cache
    cache_item *item = cache_search(name);
    if (item) {
        // Choose a random address from the list
        srand(time(NULL) ^ getpid()); // Seed the random number generator
        int random_index = rand() % item->addr_count;
        item->result.h_addr_list[0] = item->addr_list[random_index];
        item->result.h_addr_list[1] = NULL; // Terminate the list after the chosen address

        *ret = item->result; // Simplified, should actually deep copy
        *result = ret;
        return 0;
    }

    // Call the original function if not in cache
    int res = original_gethostbyname_r(name, ret, buf, buflen, result, h_errnop);
    if (res == 0 && *result != NULL) {
        cache_insert(name, *result);
    }
    return res;
}
