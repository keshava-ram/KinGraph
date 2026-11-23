#ifndef HASHMAP_H
#define HASHMAP_H

#include <stdbool.h>
#include <stddef.h>

#define DEFAULT_HASHMAP_CAPACITY 127

/**
 * Node in the hash table collision chain (Linked List).
 */
typedef struct HashEntry {
    char *key;                 // Unique person ID
    void *value;               // Generic pointer (e.g. Person* or GraphNode*)
    struct HashEntry *next;    // Collision chain pointer
} HashEntry;

/**
 * Hash Table with Separate Chaining.
 */
typedef struct HashMap {
    HashEntry **buckets;       // Array of bucket pointers
    size_t capacity;           // Number of buckets
    size_t size;               // Number of key-value pairs stored
    size_t collisions;         // Cumulative collision counter (for DSA analysis)
} HashMap;

/**
 * Creates a new HashMap with given initial capacity.
 * If capacity == 0, DEFAULT_HASHMAP_CAPACITY is used.
 */
HashMap* hashmap_create(size_t capacity);

/**
 * Frees the HashMap.
 * If free_values is true, free() will be called on each value pointer.
 */
void hashmap_destroy(HashMap *map, bool free_values);

/**
 * Inserts or updates a key-value pair.
 * Returns true on success, false on memory allocation failure.
 */
bool hashmap_put(HashMap *map, const char *key, void *value);

/**
 * Retrieves the value associated with key.
 * Returns NULL if key is not found.
 */
void* hashmap_get(const HashMap *map, const char *key);

/**
 * Removes the key-value pair associated with key.
 * Returns the removed value pointer, or NULL if not found.
 */
void* hashmap_remove(HashMap *map, const char *key);

/**
 * Returns true if the key exists in the hash table.
 */
bool hashmap_contains(const HashMap *map, const char *key);

/**
 * Returns the number of entries in the hash table.
 */
size_t hashmap_size(const HashMap *map);

/**
 * Collects all values in the hash map into a newly allocated array.
 * Caller is responsible for freeing the returned array (*values_out), but not the values themselves.
 */
void hashmap_get_all(const HashMap *map, void ***values_out, size_t *count_out);

/**
 * Calculates hash statistics for DSA demonstration (max chain length, load factor, collisions).
 */
void hashmap_get_stats(const HashMap *map, size_t *total_keys, size_t *collisions, 
                      size_t *max_chain_len, double *load_factor);

#endif /* HASHMAP_H */
