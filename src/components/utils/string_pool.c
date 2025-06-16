/**
 * High-performance string interning implementation for JDBX
 * 
 * Uses a hash table with chaining for fast O(1) lookups and insertions.
 * Thread-safe with fine-grained locking per bucket to minimize contention.
 */

#include "utils/string_pool.h"
#include "utils/buffer_pool.h"
#include "utils/logger.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>

/* Hash table entry for chaining */
typedef struct pool_entry {
    struct pool_entry* next;
    interned_string_t istr;
    char data[];  /* Flexible array member for string data */
} pool_entry_t;

/* Hash table bucket with fine-grained locking */
typedef struct {
    pool_entry_t* head;
    pthread_mutex_t lock;
} hash_bucket_t;

/* String pool structure */
struct string_pool {
    hash_bucket_t* buckets;
    size_t bucket_count;
    size_t bucket_mask;     /* For fast modulo: bucket_count - 1 */
    
    /* Limits and counters */
    size_t max_strings;
    atomic_size_t current_strings;
    atomic_size_t total_memory;
    
    /* Statistics */
    atomic_uint_fast64_t hit_count;
    atomic_uint_fast64_t miss_count;
    
    /* Global lock for structure changes */
    pthread_mutex_t global_lock;
};

/* Global string pools */
string_pool_t* g_json_keys_pool = NULL;
string_pool_t* g_doc_ids_pool = NULL;
string_pool_t* g_collection_names_pool = NULL;

/* Fast hash function (FNV-1a variant) */
static uint32_t hash_string(const char* str, size_t len) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < len; i++) {
        hash ^= (uint8_t)str[i];
        hash *= 16777619u;
    }
    return hash;
}

/* Check if a number is power of 2 */
static int is_power_of_2(size_t n) {
    return n > 0 && (n & (n - 1)) == 0;
}

/* Create a new string pool */
string_pool_t* string_pool_create(size_t initial_capacity, size_t max_strings) {
    if (initial_capacity == 0 || !is_power_of_2(initial_capacity)) {
        initial_capacity = 1024;  /* Default to 1024 buckets */
    }
    
    string_pool_t* pool = BUFFER_ALLOC(sizeof(string_pool_t));
    if (!pool) return NULL;
    
    /* Allocate bucket array */
    pool->buckets = BUFFER_ALLOC(initial_capacity * sizeof(hash_bucket_t));
    if (!pool->buckets) {
        buffer_pool_free(pool);
        return NULL;
    }
    
    /* Initialize buckets */
    for (size_t i = 0; i < initial_capacity; i++) {
        pool->buckets[i].head = NULL;
        if (pthread_mutex_init(&pool->buckets[i].lock, NULL) != 0) {
            /* Cleanup previously initialized mutexes */
            for (size_t j = 0; j < i; j++) {
                pthread_mutex_destroy(&pool->buckets[j].lock);
            }
            buffer_pool_free(pool->buckets);
            buffer_pool_free(pool);
            return NULL;
        }
    }
    
    pool->bucket_count = initial_capacity;
    pool->bucket_mask = initial_capacity - 1;
    pool->max_strings = max_strings;
    
    atomic_store(&pool->current_strings, 0);
    atomic_store(&pool->total_memory, sizeof(string_pool_t) + initial_capacity * sizeof(hash_bucket_t));
    atomic_store(&pool->hit_count, 0);
    atomic_store(&pool->miss_count, 0);
    
    if (pthread_mutex_init(&pool->global_lock, NULL) != 0) {
        for (size_t i = 0; i < initial_capacity; i++) {
            pthread_mutex_destroy(&pool->buckets[i].lock);
        }
        buffer_pool_free(pool->buckets);
        buffer_pool_free(pool);
        return NULL;
    }
    
    return pool;
}

/* Destroy a string pool */
void string_pool_destroy(string_pool_t* pool) {
    if (!pool) return;
    
    /* Free all entries */
    for (size_t i = 0; i < pool->bucket_count; i++) {
        hash_bucket_t* bucket = &pool->buckets[i];
        
        pthread_mutex_lock(&bucket->lock);
        
        pool_entry_t* entry = bucket->head;
        while (entry) {
            pool_entry_t* next = entry->next;
            buffer_pool_free(entry);
            entry = next;
        }
        
        pthread_mutex_unlock(&bucket->lock);
        pthread_mutex_destroy(&bucket->lock);
    }
    
    pthread_mutex_destroy(&pool->global_lock);
    buffer_pool_free(pool->buckets);
    buffer_pool_free(pool);
}

/* Intern a string with known length */
interned_string_t* string_pool_intern_len(string_pool_t* pool, const char* str, size_t len) {
    if (!pool || !str || len == 0) return NULL;
    
    /* Check max strings limit */
    if (pool->max_strings > 0 && atomic_load(&pool->current_strings) >= pool->max_strings) {
        return NULL;
    }
    
    uint32_t hash = hash_string(str, len);
    size_t bucket_idx = hash & pool->bucket_mask;
    hash_bucket_t* bucket = &pool->buckets[bucket_idx];
    
    pthread_mutex_lock(&bucket->lock);
    
    /* Look for existing entry */
    pool_entry_t* entry = bucket->head;
    while (entry) {
        if (entry->istr.hash == hash && 
            strlen(entry->istr.str) == len &&
            memcmp(entry->istr.str, str, len) == 0) {
            
            /* Found existing - increment reference count */
            entry->istr.ref_count++;
            atomic_fetch_add(&pool->hit_count, 1);
            
            pthread_mutex_unlock(&bucket->lock);
            return &entry->istr;
        }
        entry = entry->next;
    }
    
    /* Not found - create new entry */
    size_t entry_size = sizeof(pool_entry_t) + len + 1;
    entry = BUFFER_ALLOC(entry_size);
    if (!entry) {
        pthread_mutex_unlock(&bucket->lock);
        return NULL;
    }
    
    /* Initialize entry */
    entry->next = bucket->head;
    entry->istr.hash = hash;
    entry->istr.ref_count = 1;
    
    /* Copy string data */
    memcpy(entry->data, str, len);
    entry->data[len] = '\0';
    entry->istr.str = entry->data;
    
    /* Add to bucket */
    bucket->head = entry;
    
    pthread_mutex_unlock(&bucket->lock);
    
    /* Update counters */
    atomic_fetch_add(&pool->current_strings, 1);
    atomic_fetch_add(&pool->total_memory, entry_size);
    atomic_fetch_add(&pool->miss_count, 1);
    
    return &entry->istr;
}

/* Intern a string */
interned_string_t* string_pool_intern(string_pool_t* pool, const char* str) {
    if (!str) return NULL;
    return string_pool_intern_len(pool, str, strlen(str));
}

/* Release an interned string */
void string_pool_release(string_pool_t* pool, interned_string_t* istr) {
    if (!pool || !istr || istr->ref_count == 0) return;
    
    uint32_t hash = istr->hash;
    size_t bucket_idx = hash & pool->bucket_mask;
    hash_bucket_t* bucket = &pool->buckets[bucket_idx];
    
    pthread_mutex_lock(&bucket->lock);
    
    /* Find the entry (we need the pool_entry_t, not just interned_string_t) */
    pool_entry_t** entry_ptr = &bucket->head;
    while (*entry_ptr) {
        pool_entry_t* entry = *entry_ptr;
        if (&entry->istr == istr) {
            /* Found it - decrement reference count */
            entry->istr.ref_count--;
            
            /* Remove from bucket if ref count reaches zero */
            if (entry->istr.ref_count == 0) {
                *entry_ptr = entry->next;
                
                size_t entry_size = sizeof(pool_entry_t) + strlen(entry->istr.str) + 1;
                atomic_fetch_sub(&pool->current_strings, 1);
                atomic_fetch_sub(&pool->total_memory, entry_size);
                
                buffer_pool_free(entry);
            }
            break;
        }
        entry_ptr = &(*entry_ptr)->next;
    }
    
    pthread_mutex_unlock(&bucket->lock);
}

/* Look up a string without interning */
interned_string_t* string_pool_lookup(string_pool_t* pool, const char* str) {
    if (!pool || !str) return NULL;
    
    size_t len = strlen(str);
    uint32_t hash = hash_string(str, len);
    size_t bucket_idx = hash & pool->bucket_mask;
    hash_bucket_t* bucket = &pool->buckets[bucket_idx];
    
    pthread_mutex_lock(&bucket->lock);
    
    pool_entry_t* entry = bucket->head;
    while (entry) {
        if (entry->istr.hash == hash && 
            strlen(entry->istr.str) == len &&
            memcmp(entry->istr.str, str, len) == 0) {
            
            pthread_mutex_unlock(&bucket->lock);
            return &entry->istr;
        }
        entry = entry->next;
    }
    
    pthread_mutex_unlock(&bucket->lock);
    return NULL;
}

/* Get string pool statistics */
void string_pool_get_stats(string_pool_t* pool, size_t* total_strings, 
                          size_t* total_memory, uint64_t* hit_count, uint64_t* miss_count) {
    if (!pool) return;
    
    if (total_strings) *total_strings = atomic_load(&pool->current_strings);
    if (total_memory) *total_memory = atomic_load(&pool->total_memory);
    if (hit_count) *hit_count = atomic_load(&pool->hit_count);
    if (miss_count) *miss_count = atomic_load(&pool->miss_count);
}

/* Reset statistics */
void string_pool_reset_stats(string_pool_t* pool) {
    if (!pool) return;
    
    atomic_store(&pool->hit_count, 0);
    atomic_store(&pool->miss_count, 0);
}

/* Initialize global string pools */
int string_pools_init(void) {
    g_json_keys_pool = string_pool_create(512, 10000);  /* Common JSON keys */
    if (!g_json_keys_pool) return -1;
    
    g_doc_ids_pool = string_pool_create(1024, 50000);  /* Document IDs */
    if (!g_doc_ids_pool) {
        string_pool_destroy(g_json_keys_pool);
        g_json_keys_pool = NULL;
        return -1;
    }
    
    g_collection_names_pool = string_pool_create(64, 1000);  /* Collection names */
    if (!g_collection_names_pool) {
        string_pool_destroy(g_json_keys_pool);
        string_pool_destroy(g_doc_ids_pool);
        g_json_keys_pool = NULL;
        g_doc_ids_pool = NULL;
        return -1;
    }
    
    if (g_logger) {
        LOG_INFO("String pools initialized: keys=%zu, docs=%zu, collections=%zu buckets", 
                 g_json_keys_pool->bucket_count,
                 g_doc_ids_pool->bucket_count, 
                 g_collection_names_pool->bucket_count);
    }
    
    return 0;
}

/* Cleanup global string pools */
void string_pools_cleanup(void) {
    if (g_json_keys_pool) {
        string_pool_destroy(g_json_keys_pool);
        g_json_keys_pool = NULL;
    }
    
    if (g_doc_ids_pool) {
        string_pool_destroy(g_doc_ids_pool);
        g_doc_ids_pool = NULL;
    }
    
    if (g_collection_names_pool) {
        string_pool_destroy(g_collection_names_pool);
        g_collection_names_pool = NULL;
    }
    
    if (g_logger) {
        LOG_INFO("String pools cleaned up.");
    }
}