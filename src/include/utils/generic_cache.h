#ifndef GENERIC_CACHE_H
#define GENERIC_CACHE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <pthread.h>

/* Generic LRU cache for arbitrary data
 * Used by B+tree and other performance-critical components
 */

typedef struct generic_cache_entry {
    void* key;
    size_t key_size;
    void* value;
    size_t value_size;
    struct generic_cache_entry* next;
    struct generic_cache_entry* prev;
    struct generic_cache_entry* hash_next;
} generic_cache_entry_t;

typedef struct generic_cache {
    size_t capacity;
    size_t size;
    
    /* LRU list */
    generic_cache_entry_t* head;
    generic_cache_entry_t* tail;
    
    /* Hash table */
    generic_cache_entry_t** buckets;
    size_t bucket_count;
    
    /* Statistics */
    uint64_t hits;
    uint64_t misses;
    uint64_t evictions;
    
    /* Lock */
    pthread_rwlock_t lock;
    
    /* Custom key comparison */
    int (*key_compare)(const void* a, const void* b, size_t size);
    uint32_t (*key_hash)(const void* key, size_t size);
} generic_cache_t;

/* Create and destroy cache */
generic_cache_t* generic_cache_create(size_t capacity);
void generic_cache_destroy(generic_cache_t* cache);

/* Cache operations */
void* generic_cache_get(generic_cache_t* cache, const void* key, size_t key_size);
int generic_cache_put(generic_cache_t* cache, const void* key, size_t key_size,
                     const void* value, size_t value_size);
void generic_cache_remove(generic_cache_t* cache, const void* key, size_t key_size);
void generic_cache_clear(generic_cache_t* cache);

/* Statistics */
void generic_cache_stats(generic_cache_t* cache, uint64_t* hits, 
                        uint64_t* misses, uint64_t* evictions);

#endif /* GENERIC_CACHE_H */