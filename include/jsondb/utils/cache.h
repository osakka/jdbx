#ifndef CACHE_H
#define CACHE_H

#include "utils/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

/* Cache entry flags */
#define CACHE_FLAG_NONE    0x00
#define CACHE_FLAG_EXPIRED 0x01
#define CACHE_FLAG_DIRTY   0x02

/* Cache types */
typedef enum {
    CACHE_TYPE_LRU,     /* Least Recently Used */
    CACHE_TYPE_LFU,     /* Least Frequently Used */
    CACHE_TYPE_FIFO     /* First In, First Out */
} cache_type_t;

/* Cache statistics */
typedef struct {
    size_t capacity;     /* Maximum number of entries */
    size_t size;         /* Current number of entries */
    size_t hits;         /* Number of cache hits */
    size_t misses;       /* Number of cache misses */
    size_t evictions;    /* Number of entries evicted */
    double hit_ratio;    /* Hit ratio (hits / (hits + misses)) */
    double byte_size;    /* Estimated memory usage in bytes */
} cache_stats_t;

/* Cache entry */
typedef struct cache_entry {
    char* key;                   /* Cache key */
    json_value_t* value;         /* Cached value (JSON) */
    time_t timestamp;            /* Entry creation/update time */
    time_t expires;              /* Expiration time (0 for no expiration) */
    unsigned int flags;          /* Entry flags */
    size_t size;                 /* Estimated entry size in bytes */
    unsigned int access_count;   /* Number of times entry was accessed */
    struct cache_entry* next;    /* Next entry in the list */
    struct cache_entry* prev;    /* Previous entry in the list */
} cache_entry_t;

/* Cache configuration */
typedef struct {
    size_t capacity;             /* Maximum number of entries */
    time_t default_ttl;          /* Default time-to-live in seconds (0 for no expiration) */
    cache_type_t type;           /* Cache eviction policy */
    double max_memory_mb;        /* Maximum memory usage in MB (0 for no limit) */
    int cleanup_interval;        /* Cleanup interval in seconds */
} cache_config_t;

/* Cache instance */
typedef struct cache {
    cache_config_t config;       /* Cache configuration */
    cache_entry_t* head;         /* Head of the entry list */
    cache_entry_t* tail;         /* Tail of the entry list */
    size_t size;                 /* Current number of entries */
    size_t byte_size;            /* Estimated memory usage in bytes */
    size_t hits;                 /* Cache hit counter */
    size_t misses;               /* Cache miss counter */
    size_t evictions;            /* Cache eviction counter */
    pthread_mutex_t lock;        /* Cache mutex */
    time_t last_cleanup;         /* Last cleanup timestamp */
} cache_t;

/* Cache function prototypes */

/* Cache creation and management */
cache_t* cache_create(cache_config_t* config);
void cache_destroy(cache_t* cache);
void cache_set_config(cache_t* cache, cache_config_t* config);
void cache_clear(cache_t* cache);
void cache_run_cleanup(cache_t* cache);

/* Cache operations */
int cache_put(cache_t* cache, const char* key, json_value_t* value, time_t ttl);
json_value_t* cache_get(cache_t* cache, const char* key);
int cache_remove(cache_t* cache, const char* key);
int cache_has(cache_t* cache, const char* key);
int cache_touch(cache_t* cache, const char* key, time_t ttl);
size_t cache_get_keys(cache_t* cache, char*** keys);

/* Cache statistics and diagnostics */
cache_stats_t cache_get_stats(cache_t* cache);
json_value_t* cache_get_stats_json(cache_t* cache);

#endif /* CACHE_H */