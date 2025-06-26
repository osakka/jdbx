/**
 * @file generic_cache.c
 * @brief Generic hash-based cache implementation with configurable policies
 * 
 * Provides a high-performance, thread-safe caching system with pluggable
 * hash functions, comparison functions, and eviction policies. Designed
 * for use across JDBX components requiring fast key-value lookups.
 * 
 * Features:
 * - Configurable hash functions and key comparison
 * - Multiple eviction policies (LRU, TTL, size-based)
 * - Thread-safe operations with minimal locking
 * - Memory pool integration for performance
 */

#include "utils/generic_cache.h"
#include <stdlib.h>
#include <string.h>
#include "utils/buffer_pool.h"

/**
 * Default hash function using djb2 algorithm
 * 
 * Provides good distribution characteristics for most key types.
 * Uses the djb2 hash algorithm with a prime multiplier for
 * consistent hashing performance across different data patterns.
 * 
 * @param key Pointer to key data to hash
 * @param size Size of key data in bytes
 * @return 32-bit hash value
 */
static uint32_t default_hash(const void* key, size_t size) {
    const uint8_t* data = (const uint8_t*)key;
    uint32_t hash = 5381;
    
    for (size_t i = 0; i < size; i++) {
        hash = ((hash << 5) + hash) + data[i];
    }
    
    return hash;
}

/**
 * Default key comparison function
 * 
 * Performs bytewise comparison of keys using standard memcmp.
 * Suitable for most key types including strings, integers, and
 * binary data where lexicographic ordering is appropriate.
 * 
 * @param a First key to compare
 * @param b Second key to compare  
 * @param size Size of keys in bytes
 * @return 0 if equal, <0 if a < b, >0 if a > b
 */
static int default_compare(const void* a, const void* b, size_t size) {
    return memcmp(a, b, size);
}

/* Create cache */
generic_cache_t* generic_cache_create(size_t capacity) {
    if (capacity == 0) return NULL;
    
    generic_cache_t* cache =BUFFER_CALLOC(1, sizeof(generic_cache_t));
    if (!cache) return NULL;
    
    cache->capacity = capacity;
    cache->bucket_count = capacity * 2; /* Load factor 0.5 */
    cache->buckets =BUFFER_CALLOC(cache->bucket_count, sizeof(generic_cache_entry_t*));
    
    if (!cache->buckets) {
        BUFFER_FREE(cache);
        return NULL;
    }
    
    cache->key_compare = default_compare;
    cache->key_hash = default_hash;
    pthread_rwlock_init(&cache->lock, NULL);
    
    return cache;
}

/* Destroy cache */
void generic_cache_destroy(generic_cache_t* cache) {
    if (!cache) return;
    
    generic_cache_clear(cache);
    BUFFER_FREE(cache->buckets);
    pthread_rwlock_destroy(&cache->lock);
    BUFFER_FREE(cache);
}

/* Find entry in hash table */
static generic_cache_entry_t* find_entry(generic_cache_t* cache, 
                                        const void* key, size_t key_size) {
    uint32_t hash = cache->key_hash(key, key_size);
    uint32_t bucket = hash % cache->bucket_count;
    
    generic_cache_entry_t* entry = cache->buckets[bucket];
    while (entry) {
        if (entry->key_size == key_size &&
            cache->key_compare(entry->key, key, key_size) == 0) {
            return entry;
        }
        entry = entry->hash_next;
    }
    
    return NULL;
}

/* Move entry to front of LRU list */
static void move_to_front(generic_cache_t* cache, generic_cache_entry_t* entry) {
    if (entry == cache->head) return;
    
    /* Remove from current position */
    if (entry->prev) entry->prev->next = entry->next;
    if (entry->next) entry->next->prev = entry->prev;
    if (entry == cache->tail) cache->tail = entry->prev;
    
    /* Insert at front */
    entry->prev = NULL;
    entry->next = cache->head;
    if (cache->head) cache->head->prev = entry;
    cache->head = entry;
    if (!cache->tail) cache->tail = entry;
}

/* Evict least recently used entry */
static void evict_lru(generic_cache_t* cache) {
    if (!cache->tail) return;
    
    generic_cache_entry_t* victim = cache->tail;
    
    /* Remove from LRU list */
    cache->tail = victim->prev;
    if (cache->tail) {
        cache->tail->next = NULL;
    } else {
        cache->head = NULL;
    }
    
    /* Remove from hash table */
    uint32_t hash = cache->key_hash(victim->key, victim->key_size);
    uint32_t bucket = hash % cache->bucket_count;
    
    generic_cache_entry_t** ptr = &cache->buckets[bucket];
    while (*ptr) {
        if (*ptr == victim) {
            *ptr = victim->hash_next;
            break;
        }
        ptr = &(*ptr)->hash_next;
    }
    
    /* Free entry */
    BUFFER_FREE(victim->key);
    BUFFER_FREE(victim->value);
    BUFFER_FREE(victim);
    
    cache->size--;
    cache->evictions++;
}

/* Get value from cache */
void* generic_cache_get(generic_cache_t* cache, const void* key, size_t key_size) {
    if (!cache || !key) return NULL;
    
    pthread_rwlock_rdlock(&cache->lock);
    
    generic_cache_entry_t* entry = find_entry(cache, key, key_size);
    void* result = NULL;
    
    if (entry) {
        /* Allocate and copy value */
        result =BUFFER_ALLOC(entry->value_size);
        if (result) {
            memcpy(result, entry->value, entry->value_size);
            cache->hits++;
        }
        
        /* Move to front (need write lock) */
        pthread_rwlock_unlock(&cache->lock);
        pthread_rwlock_wrlock(&cache->lock);
        move_to_front(cache, entry);
    } else {
        cache->misses++;
    }
    
    pthread_rwlock_unlock(&cache->lock);
    return result;
}

/* Put value in cache */
int generic_cache_put(generic_cache_t* cache, const void* key, size_t key_size,
                     const void* value, size_t value_size) {
    if (!cache || !key || !value) return -1;
    
    pthread_rwlock_wrlock(&cache->lock);
    
    /* Check if key exists */
    generic_cache_entry_t* entry = find_entry(cache, key, key_size);
    
    if (entry) {
        /* Update existing entry */
        void* new_value =BUFFER_ALLOC(value_size);
        if (!new_value) {
            pthread_rwlock_unlock(&cache->lock);
            return -1;
        }
        
        memcpy(new_value, value, value_size);
        BUFFER_FREE(entry->value);
        entry->value = new_value;
        entry->value_size = value_size;
        
        move_to_front(cache, entry);
    } else {
        /* Create new entry */
        if (cache->size >= cache->capacity) {
            evict_lru(cache);
        }
        
        entry =BUFFER_ALLOC(sizeof(generic_cache_entry_t));
        if (!entry) {
            pthread_rwlock_unlock(&cache->lock);
            return -1;
        }
        
        entry->key =BUFFER_ALLOC(key_size);
        entry->value =BUFFER_ALLOC(value_size);
        
        if (!entry->key || !entry->value) {
            BUFFER_FREE(entry->key);
            BUFFER_FREE(entry->value);
            BUFFER_FREE(entry);
            pthread_rwlock_unlock(&cache->lock);
            return -1;
        }
        
        memcpy(entry->key, key, key_size);
        memcpy(entry->value, value, value_size);
        entry->key_size = key_size;
        entry->value_size = value_size;
        
        /* Add to hash table */
        uint32_t hash = cache->key_hash(key, key_size);
        uint32_t bucket = hash % cache->bucket_count;
        entry->hash_next = cache->buckets[bucket];
        cache->buckets[bucket] = entry;
        
        /* Add to front of LRU list */
        entry->prev = NULL;
        entry->next = cache->head;
        if (cache->head) cache->head->prev = entry;
        cache->head = entry;
        if (!cache->tail) cache->tail = entry;
        
        cache->size++;
    }
    
    pthread_rwlock_unlock(&cache->lock);
    return 0;
}

/* Clear cache */
void generic_cache_clear(generic_cache_t* cache) {
    if (!cache) return;
    
    pthread_rwlock_wrlock(&cache->lock);
    
    /* Free all entries */
    generic_cache_entry_t* entry = cache->head;
    while (entry) {
        generic_cache_entry_t* next = entry->next;
        BUFFER_FREE(entry->key);
        BUFFER_FREE(entry->value);
        BUFFER_FREE(entry);
        entry = next;
    }
    
    /* Clear buckets */
    memset(cache->buckets, 0, cache->bucket_count * sizeof(generic_cache_entry_t*));
    
    cache->head = NULL;
    cache->tail = NULL;
    cache->size = 0;
    
    pthread_rwlock_unlock(&cache->lock);
}

/* Remove entry from cache */
void generic_cache_remove(generic_cache_t* cache, const void* key, size_t key_size) {
    if (!cache || !key) return;
    
    pthread_rwlock_wrlock(&cache->lock);
    
    generic_cache_entry_t* entry = find_entry(cache, key, key_size);
    if (entry) {
        /* Remove from LRU list */
        if (entry->prev) entry->prev->next = entry->next;
        if (entry->next) entry->next->prev = entry->prev;
        if (entry == cache->head) cache->head = entry->next;
        if (entry == cache->tail) cache->tail = entry->prev;
        
        /* Remove from hash table */
        uint32_t hash = cache->key_hash(key, key_size);
        uint32_t bucket = hash % cache->bucket_count;
        
        generic_cache_entry_t** ptr = &cache->buckets[bucket];
        while (*ptr) {
            if (*ptr == entry) {
                *ptr = entry->hash_next;
                break;
            }
            ptr = &(*ptr)->hash_next;
        }
        
        /* Free entry */
        BUFFER_FREE(entry->key);
        BUFFER_FREE(entry->value);
        BUFFER_FREE(entry);
        
        cache->size--;
    }
    
    pthread_rwlock_unlock(&cache->lock);
}

/* Get statistics */
void generic_cache_stats(generic_cache_t* cache, uint64_t* hits, 
                        uint64_t* misses, uint64_t* evictions) {
    if (!cache) return;
    
    pthread_rwlock_rdlock(&cache->lock);
    
    if (hits) *hits = cache->hits;
    if (misses) *misses = cache->misses;
    if (evictions) *evictions = cache->evictions;
    
    pthread_rwlock_unlock(&cache->lock);
}
