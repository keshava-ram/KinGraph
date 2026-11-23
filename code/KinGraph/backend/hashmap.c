#include "hashmap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * DJB2 Hash Function
 * An efficient string hashing algorithm by Dan Bernstein.
 */
static unsigned long hash_string(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = (unsigned char)*str++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash;
}

HashMap* hashmap_create(size_t capacity) {
    if (capacity == 0) {
        capacity = DEFAULT_HASHMAP_CAPACITY;
    }
    HashMap *map = (HashMap*)malloc(sizeof(HashMap));
    if (!map) return NULL;

    map->capacity = capacity;
    map->size = 0;
    map->collisions = 0;
    map->buckets = (HashEntry**)calloc(capacity, sizeof(HashEntry*));
    if (!map->buckets) {
        free(map);
        return NULL;
    }

    return map;
}

void hashmap_destroy(HashMap *map, bool free_values) {
    if (!map) return;

    for (size_t i = 0; i < map->capacity; i++) {
        HashEntry *entry = map->buckets[i];
        while (entry) {
            HashEntry *next = entry->next;
            if (entry->key) free(entry->key);
            if (free_values && entry->value) free(entry->value);
            free(entry);
            entry = next;
        }
    }

    free(map->buckets);
    free(map);
}

bool hashmap_put(HashMap *map, const char *key, void *value) {
    if (!map || !key) return false;

    unsigned long h = hash_string(key);
    size_t index = h % map->capacity;

    HashEntry *entry = map->buckets[index];
    if (entry != NULL) {
        // A bucket collision occurred (another entry already resides at this index)
        map->collisions++;
    }

    // Traverse chain to see if key already exists (update scenario)
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            entry->value = value; // update existing
            return true;
        }
        entry = entry->next;
    }

    // Key not found in chain, insert new node at head of chain
    HashEntry *new_node = (HashEntry*)malloc(sizeof(HashEntry));
    if (!new_node) return false;

    new_node->key = strdup(key);
    if (!new_node->key) {
        free(new_node);
        return false;
    }
    new_node->value = value;
    new_node->next = map->buckets[index];
    map->buckets[index] = new_node;
    map->size++;

    return true;
}

void* hashmap_get(const HashMap *map, const char *key) {
    if (!map || !key) return NULL;

    unsigned long h = hash_string(key);
    size_t index = h % map->capacity;

    HashEntry *entry = map->buckets[index];
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            return entry->value;
        }
        entry = entry->next;
    }
    return NULL;
}

void* hashmap_remove(HashMap *map, const char *key) {
    if (!map || !key) return NULL;

    unsigned long h = hash_string(key);
    size_t index = h % map->capacity;

    HashEntry *prev = NULL;
    HashEntry *curr = map->buckets[index];

    while (curr) {
        if (strcmp(curr->key, key) == 0) {
            if (prev) {
                prev->next = curr->next;
            } else {
                map->buckets[index] = curr->next;
            }
            void *val = curr->value;
            free(curr->key);
            free(curr);
            map->size--;
            return val;
        }
        prev = curr;
        curr = curr->next;
    }
    return NULL;
}

bool hashmap_contains(const HashMap *map, const char *key) {
    return hashmap_get(map, key) != NULL;
}

size_t hashmap_size(const HashMap *map) {
    return map ? map->size : 0;
}

void hashmap_get_all(const HashMap *map, void ***values_out, size_t *count_out) {
    if (!map || !values_out || !count_out) return;

    *count_out = map->size;
    if (map->size == 0) {
        *values_out = NULL;
        return;
    }

    void **arr = (void**)malloc(sizeof(void*) * map->size);
    if (!arr) {
        *count_out = 0;
        *values_out = NULL;
        return;
    }

    size_t idx = 0;
    for (size_t i = 0; i < map->capacity; i++) {
        HashEntry *entry = map->buckets[i];
        while (entry) {
            arr[idx++] = entry->value;
            entry = entry->next;
        }
    }
    *values_out = arr;
}

void hashmap_get_stats(const HashMap *map, size_t *total_keys, size_t *collisions, 
                      size_t *max_chain_len, double *load_factor) {
    if (!map) return;

    if (total_keys) *total_keys = map->size;
    if (collisions) *collisions = map->collisions;

    size_t max_len = 0;
    for (size_t i = 0; i < map->capacity; i++) {
        size_t len = 0;
        HashEntry *entry = map->buckets[i];
        while (entry) {
            len++;
            entry = entry->next;
        }
        if (len > max_len) max_len = len;
    }
    if (max_chain_len) *max_chain_len = max_len;
    if (load_factor) *load_factor = (double)map->size / (double)map->capacity;
}
