#include "utils/cache.h"
#include "utils/logger.h"
#include <math.h>

/* Estimate memory size of a string */
static size_t string_size(const char* str) {
    return str ? strlen(str) + 1 : 0;
}

/* Estimate memory size of a json_value */
static size_t json_value_size(json_value_t* value) {
    if (!value) return 0;
    
    size_t size = sizeof(json_value_t);
    
    switch (value->type) {
        case JSON_STRING:
            size += string_size(value->value.string);
            break;
            
        case JSON_ARRAY:
            for (size_t i = 0; i < value->value.array.size; i++) {
                size += json_value_size(value->value.array.items[i]);
            }
            size += sizeof(json_value_t*) * value->value.array.capacity;
            break;
            
        case JSON_OBJECT:
            for (size_t i = 0; i < value->value.object.size; i++) {
                size += string_size(value->value.object.entries[i].key);
                size += json_value_size(value->value.object.entries[i].value);
            }
            size += sizeof(json_object_entry_t) * value->value.object.capacity;
            break;
            
        default:
            /* Other types have fixed size already accounted for in json_value_t */
            break;
    }
    
    return size;
}

/* Create a new cache entry */
static cache_entry_t* create_entry(const char* key, json_value_t* value, time_t ttl) {
    cache_entry_t* entry = (cache_entry_t*)malloc(sizeof(cache_entry_t));
    if (!entry) return NULL;
    
    entry->key = strdup(key);
    entry->value = json_clone(value);
    entry->timestamp = time(NULL);
    entry->expires = (ttl > 0) ? (entry->timestamp + ttl) : 0;
    entry->flags = CACHE_FLAG_NONE;
    entry->access_count = 1;
    entry->next = NULL;
    entry->prev = NULL;
    
    /* Calculate entry size */
    entry->size = sizeof(cache_entry_t) + string_size(key) + json_value_size(value);
    
    return entry;
}

/* Free a cache entry */
static void free_entry(cache_entry_t* entry) {
    if (entry) {
        if (entry->key) free(entry->key);
        if (entry->value) json_free(entry->value);
        free(entry);
    }
}

/* Move entry to the front of the list (for LRU) */
static void move_to_front(cache_t* cache, cache_entry_t* entry) {
    if (!cache || !entry || entry == cache->head) return;
    
    /* Remove from current position */
    if (entry->prev) entry->prev->next = entry->next;
    if (entry->next) entry->next->prev = entry->prev;
    
    /* If it was the tail, update tail */
    if (entry == cache->tail) {
        cache->tail = entry->prev;
    }
    
    /* Move to front */
    entry->next = cache->head;
    entry->prev = NULL;
    if (cache->head) cache->head->prev = entry;
    cache->head = entry;
    
    /* If this is the only entry, it's also the tail */
    if (!cache->tail) {
        cache->tail = entry;
    }
}

/* Move entry based on access frequency (for LFU) */
static void reposition_by_frequency(cache_t* cache, cache_entry_t* entry) {
    if (!cache || !entry) return;
    
    /* Remove from current position */
    if (entry->prev) entry->prev->next = entry->next;
    if (entry->next) entry->next->prev = entry->prev;
    
    /* If it was the head or tail, update those pointers */
    if (entry == cache->head) cache->head = entry->next;
    if (entry == cache->tail) cache->tail = entry->prev;
    
    /* Find new position based on access count (higher count = closer to front) */
    cache_entry_t* current = cache->head;
    cache_entry_t* prev = NULL;
    
    while (current && current->access_count >= entry->access_count) {
        prev = current;
        current = current->next;
    }
    
    if (prev) {
        /* Insert after prev */
        entry->next = prev->next;
        entry->prev = prev;
        prev->next = entry;
        if (entry->next) entry->next->prev = entry;
        else cache->tail = entry; /* If inserted at the end */
    } else {
        /* Insert at the front */
        entry->next = cache->head;
        entry->prev = NULL;
        if (cache->head) cache->head->prev = entry;
        cache->head = entry;
        if (!cache->tail) cache->tail = entry;
    }
}

/* Find entry by key */
static cache_entry_t* find_entry(cache_t* cache, const char* key) {
    cache_entry_t* entry = cache->head;
    
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    
    return NULL;
}

/* Evict least recently/frequently used entry, or oldest for FIFO */
static void evict_entry(cache_t* cache) {
    if (!cache || !cache->tail) return;
    
    /* The tail is the least recently used entry (LRU) or the oldest entry (FIFO) */
    cache_entry_t* to_evict = NULL;
    
    if (cache->config.type == CACHE_TYPE_LFU) {
        /* For LFU, find entry with lowest access count */
        cache_entry_t* entry = cache->head;
        to_evict = entry;
        size_t min_count = entry ? entry->access_count : 0;
        
        while (entry) {
            if (entry->access_count < min_count) {
                min_count = entry->access_count;
                to_evict = entry;
            }
            entry = entry->next;
        }
    } else {
        /* For LRU and FIFO, evict from the tail */
        to_evict = cache->tail;
    }
    
    if (to_evict) {
        /* Remove from list */
        if (to_evict->prev) to_evict->prev->next = to_evict->next;
        if (to_evict->next) to_evict->next->prev = to_evict->prev;
        
        /* Update head/tail if needed */
        if (to_evict == cache->head) cache->head = to_evict->next;
        if (to_evict == cache->tail) cache->tail = to_evict->prev;
        
        /* Update stats */
        cache->size--;
        cache->byte_size -= to_evict->size;
        cache->evictions++;
        
        /* Free entry */
        free_entry(to_evict);
    }
}

/* Create a new cache */
cache_t* cache_create(cache_config_t* config) {
    LOG_DEBUG("Creating new cache instance");

    cache_t* cache = (cache_t*)malloc(sizeof(cache_t));
    if (!cache) {
        LOG_ERROR("Failed to allocate memory for cache");
        return NULL;
    }

    /* Initialize cache */
    memset(cache, 0, sizeof(cache_t));

    /* Set configuration */
    if (config) {
        LOG_DEBUG("Using provided cache configuration: capacity=%zu, ttl=%ld, type=%d, max_memory=%.2f MB, cleanup=%d s",
                 config->capacity, config->default_ttl, config->type,
                 config->max_memory_mb, config->cleanup_interval);
        memcpy(&cache->config, config, sizeof(cache_config_t));
    } else {
        /* Default configuration */
        LOG_DEBUG("Using default cache configuration");
        cache->config.capacity = 1000;
        cache->config.default_ttl = 0; /* No expiration by default */
        cache->config.type = CACHE_TYPE_LRU;
        cache->config.max_memory_mb = 0; /* No memory limit by default */
        cache->config.cleanup_interval = 60; /* Clean up every minute */

        LOG_DEBUG("Default cache configuration: capacity=1000, ttl=0, type=LRU, max_memory=0 MB, cleanup=60 s");
    }

    /* Initialize mutex */
    int mutex_result = pthread_mutex_init(&cache->lock, NULL);
    if (mutex_result != 0) {
        LOG_ERROR("Failed to initialize cache mutex: %s", strerror(mutex_result));
        free(cache);
        return NULL;
    }

    /* Set last cleanup time */
    cache->last_cleanup = time(NULL);

    const char* type_str = "unknown";
    switch (cache->config.type) {
        case CACHE_TYPE_LRU: type_str = "LRU"; break;
        case CACHE_TYPE_LFU: type_str = "LFU"; break;
        case CACHE_TYPE_FIFO: type_str = "FIFO"; break;
    }

    LOG_INFO("Cache created successfully: %s policy, capacity: %zu entries",
             type_str, cache->config.capacity);
    return cache;
}

/* Destroy cache and free all entries */
void cache_destroy(cache_t* cache) {
    if (!cache) {
        LOG_DEBUG("Attempt to destroy NULL cache, ignoring");
        return;
    }

    LOG_DEBUG("Destroying cache instance with %zu entries (%.2f KB)",
             cache->size, cache->byte_size / 1024.0);

    /* Get stats for logging */
    cache_stats_t stats = cache_get_stats(cache);

    /* Clear all entries */
    cache_clear(cache);

    /* Destroy mutex */
    pthread_mutex_destroy(&cache->lock);

    LOG_INFO("Cache destroyed. Stats: hits=%zu, misses=%zu, hit_ratio=%.2f%%",
            stats.hits, stats.misses, stats.hit_ratio * 100.0);

    /* Free cache itself */
    free(cache);
}

/* Set/update cache configuration */
void cache_set_config(cache_t* cache, cache_config_t* config) {
    if (!cache || !config) return;
    
    pthread_mutex_lock(&cache->lock);
    
    /* Save old capacity */
    size_t old_capacity = cache->config.capacity;
    
    /* Update configuration */
    memcpy(&cache->config, config, sizeof(cache_config_t));
    
    /* If capacity was reduced, evict entries to meet new capacity */
    if (config->capacity < old_capacity) {
        while (cache->size > config->capacity) {
            evict_entry(cache);
        }
    }
    
    pthread_mutex_unlock(&cache->lock);
}

/* Clear all entries in the cache */
void cache_clear(cache_t* cache) {
    LOG_DEBUG("Clearing all entries from cache");

    if (!cache) {
        LOG_ERROR("Attempt to clear NULL cache, ignoring");
        return;
    }

    pthread_mutex_lock(&cache->lock);

    /* Log cache state before clearing */
    LOG_INFO("Cache state before clearing: entries=%zu, memory=%.2f KB",
            cache->size, cache->byte_size / 1024.0);

    /* Free all entries */
    size_t entry_count = 0;
    cache_entry_t* entry = cache->head;
    while (entry) {
        cache_entry_t* next = entry->next;
        LOG_TRACE("Freeing cache entry %zu: key='%s', size=%zu bytes",
                 ++entry_count, entry->key, entry->size);
        free_entry(entry);
        entry = next;
    }

    /* Reset cache state */
    cache->head = NULL;
    cache->tail = NULL;
    cache->size = 0;
    cache->byte_size = 0;

    LOG_INFO("Cache cleared successfully, freed %zu entries", entry_count);

    pthread_mutex_unlock(&cache->lock);
}

/* Run cache cleanup to remove expired entries */
void cache_run_cleanup(cache_t* cache) {
    LOG_DEBUG("Running cache cleanup");

    if (!cache) {
        LOG_ERROR("Attempt to cleanup NULL cache, ignoring");
        return;
    }

    pthread_mutex_lock(&cache->lock);

    time_t now = time(NULL);
    time_t elapsed = now - cache->last_cleanup;

    LOG_DEBUG("Time since last cleanup: %ld seconds (cleanup interval: %d seconds)",
             elapsed, cache->config.cleanup_interval);

    /* Update last cleanup time */
    cache->last_cleanup = now;

    /* Log cache state before cleanup */
    size_t initial_size = cache->size;
    size_t initial_byte_size = cache->byte_size;

    /* Remove expired entries */
    size_t expired_count = 0;
    cache_entry_t* entry = cache->head;
    while (entry) {
        cache_entry_t* next = entry->next;

        /* Check if entry has expired */
        if (entry->expires > 0 && entry->expires <= now) {
            LOG_TRACE("Removing expired entry: key='%s', expired %ld seconds ago",
                     entry->key, now - entry->expires);

            /* Remove from list */
            if (entry->prev) entry->prev->next = entry->next;
            if (entry->next) entry->next->prev = entry->prev;

            /* Update head/tail if needed */
            if (entry == cache->head) cache->head = entry->next;
            if (entry == cache->tail) cache->tail = entry->prev;

            /* Update stats */
            cache->size--;
            cache->byte_size -= entry->size;

            /* Free entry */
            free_entry(entry);
            expired_count++;
        }

        entry = next;
    }

    size_t memory_freed = initial_byte_size - cache->byte_size;

    if (expired_count > 0) {
        LOG_INFO("Cache cleanup removed %zu expired entries (%.2f KB)",
                expired_count, memory_freed / 1024.0);
    } else {
        LOG_DEBUG("No expired entries found during cleanup");
    }

    LOG_DEBUG("Cache state after cleanup: entries=%zu (-%zu), memory=%.2f KB (-%zu bytes)",
             cache->size, initial_size - cache->size,
             cache->byte_size / 1024.0, memory_freed);

    pthread_mutex_unlock(&cache->lock);
}

/* Put a value in the cache */
int cache_put(cache_t* cache, const char* key, json_value_t* value, time_t ttl) {
    LOG_DEBUG("Cache put: key='%s', ttl=%ld", key ? key : "NULL", ttl);

    if (!cache || !key || !value) {
        LOG_ERROR("Invalid parameters for cache_put: cache=%p, key=%s, value=%p",
                 (void*)cache, key ? key : "NULL", (void*)value);
        return 0;
    }

    pthread_mutex_lock(&cache->lock);

    /* Check if entry already exists */
    cache_entry_t* existing = find_entry(cache, key);

    if (existing) {
        LOG_TRACE("Updating existing cache entry for key '%s'", key);

        /* Update existing entry */
        size_t old_size = existing->size;

        /* Free old value */
        if (existing->value) json_free(existing->value);

        /* Set new value */
        existing->value = json_clone(value);
        existing->timestamp = time(NULL);
        existing->expires = (ttl > 0) ? (existing->timestamp + ttl) :
                          (cache->config.default_ttl > 0) ? (existing->timestamp + cache->config.default_ttl) : 0;
        existing->flags = CACHE_FLAG_NONE;
        existing->access_count++;

        /* Update size */
        existing->size = sizeof(cache_entry_t) + string_size(key) + json_value_size(value);
        cache->byte_size += (existing->size - old_size);

        /* Move entry based on cache type */
        if (cache->config.type == CACHE_TYPE_LRU) {
            LOG_TRACE("Moving entry to front (LRU policy)");
            move_to_front(cache, existing);
        } else if (cache->config.type == CACHE_TYPE_LFU) {
            LOG_TRACE("Repositioning entry by frequency (LFU policy): count=%u", existing->access_count);
            reposition_by_frequency(cache, existing);
        }
        /* For FIFO, we don't reposition on update */

        LOG_DEBUG("Cache entry updated: key='%s', new_size=%zu bytes, expires=%ld",
                 key, existing->size, existing->expires);
    } else {
        LOG_TRACE("Creating new cache entry for key '%s'", key);

        /* Check if cleanup is needed before adding new entry */
        time_t now = time(NULL);
        if ((now - cache->last_cleanup) >= cache->config.cleanup_interval) {
            LOG_DEBUG("Running cleanup before adding new entry (interval elapsed)");
            cache_run_cleanup(cache);
        }

        /* Check capacity and evict if necessary */
        if (cache->config.capacity > 0 && cache->size >= cache->config.capacity) {
            LOG_DEBUG("Cache at capacity (%zu entries), evicting entry", cache->config.capacity);
            evict_entry(cache);
        }

        /* Check memory limit and evict if necessary */
        while (cache->config.max_memory_mb > 0 &&
              (cache->byte_size / (1024.0 * 1024.0)) >= cache->config.max_memory_mb &&
              cache->size > 0) {
            LOG_DEBUG("Cache at memory limit (%.2f MB), evicting entry",
                    cache->config.max_memory_mb);
            evict_entry(cache);
        }

        /* Create new entry */
        time_t effective_ttl = ttl > 0 ? ttl : cache->config.default_ttl;
        cache_entry_t* entry = create_entry(key, value, effective_ttl);
        if (!entry) {
            LOG_ERROR("Failed to create cache entry for key '%s'", key);
            pthread_mutex_unlock(&cache->lock);
            return 0;
        }

        /* Add entry to the list */
        if (cache->head) {
            /* Add to front */
            LOG_TRACE("Adding entry to front of cache");
            entry->next = cache->head;
            cache->head->prev = entry;
            cache->head = entry;
        } else {
            /* First entry */
            LOG_TRACE("Adding first entry to cache");
            cache->head = entry;
            cache->tail = entry;
        }

        /* Update stats */
        cache->size++;
        cache->byte_size += entry->size;

        LOG_DEBUG("New cache entry added: key='%s', size=%zu bytes, expires=%s",
                 key, entry->size,
                 entry->expires ? ctime(&entry->expires) : "never");
    }

    pthread_mutex_unlock(&cache->lock);

    LOG_INFO("Cache put successful: key='%s', cache_size=%zu entries, memory=%.2f KB",
            key, cache->size, cache->byte_size / 1024.0);
    return 1;
}

/* Get a value from the cache */
json_value_t* cache_get(cache_t* cache, const char* key) {
    LOG_DEBUG("Cache get: key='%s'", key ? key : "NULL");

    if (!cache || !key) {
        LOG_ERROR("Invalid parameters for cache_get: cache=%p, key=%s",
                 (void*)cache, key ? key : "NULL");
        return NULL;
    }

    pthread_mutex_lock(&cache->lock);

    /* Find entry */
    cache_entry_t* entry = find_entry(cache, key);

    if (entry) {
        /* Check if entry has expired */
        time_t now = time(NULL);
        if (entry->expires > 0 && entry->expires <= now) {
            LOG_DEBUG("Cache entry expired: key='%s', expired_at=%ld, now=%ld",
                     key, entry->expires, now);

            /* Entry has expired, remove it */
            if (entry->prev) entry->prev->next = entry->next;
            if (entry->next) entry->next->prev = entry->prev;

            /* Update head/tail if needed */
            if (entry == cache->head) cache->head = entry->next;
            if (entry == cache->tail) cache->tail = entry->prev;

            /* Update stats */
            cache->size--;
            cache->byte_size -= entry->size;
            cache->misses++;

            LOG_TRACE("Removing expired entry: key='%s', miss_count=%zu", key, cache->misses);

            /* Free entry */
            free_entry(entry);

            pthread_mutex_unlock(&cache->lock);
            return NULL;
        }

        /* Update access count and timestamp */
        entry->access_count++;

        LOG_TRACE("Cache hit: key='%s', access_count=%u", key, entry->access_count);

        /* Move entry based on cache type */
        if (cache->config.type == CACHE_TYPE_LRU) {
            LOG_TRACE("Moving entry to front (LRU policy)");
            move_to_front(cache, entry);
        } else if (cache->config.type == CACHE_TYPE_LFU) {
            LOG_TRACE("Repositioning entry by frequency (LFU policy): count=%u", entry->access_count);
            reposition_by_frequency(cache, entry);
        }
        /* For FIFO, we don't reposition on access */

        /* Clone value to return */
        json_value_t* result = json_clone(entry->value);
        if (!result) {
            LOG_ERROR("Failed to clone value for cache key '%s'", key);
        }

        /* Update stats */
        cache->hits++;

        LOG_DEBUG("Cache hit successful: key='%s', hit_count=%zu, hit_ratio=%.2f%%",
                 key, cache->hits,
                 100.0 * cache->hits / (cache->hits + cache->misses));

        pthread_mutex_unlock(&cache->lock);
        return result;
    } else {
        /* Entry not found */
        cache->misses++;
        LOG_DEBUG("Cache miss: key='%s', miss_count=%zu", key, cache->misses);
        pthread_mutex_unlock(&cache->lock);
        return NULL;
    }
}

/* Remove a value from the cache */
int cache_remove(cache_t* cache, const char* key) {
    if (!cache || !key) return 0;
    
    pthread_mutex_lock(&cache->lock);
    
    /* Find entry */
    cache_entry_t* entry = find_entry(cache, key);
    
    if (entry) {
        /* Remove from list */
        if (entry->prev) entry->prev->next = entry->next;
        if (entry->next) entry->next->prev = entry->prev;
        
        /* Update head/tail if needed */
        if (entry == cache->head) cache->head = entry->next;
        if (entry == cache->tail) cache->tail = entry->prev;
        
        /* Update stats */
        cache->size--;
        cache->byte_size -= entry->size;
        
        /* Free entry */
        free_entry(entry);
        
        pthread_mutex_unlock(&cache->lock);
        return 1;
    }
    
    pthread_mutex_unlock(&cache->lock);
    return 0;
}

/* Check if a key exists in the cache */
int cache_has(cache_t* cache, const char* key) {
    if (!cache || !key) return 0;
    
    pthread_mutex_lock(&cache->lock);
    
    /* Find entry */
    cache_entry_t* entry = find_entry(cache, key);
    
    if (entry) {
        /* Check if entry has expired */
        time_t now = time(NULL);
        if (entry->expires > 0 && entry->expires <= now) {
            pthread_mutex_unlock(&cache->lock);
            return 0;
        }
        
        pthread_mutex_unlock(&cache->lock);
        return 1;
    }
    
    pthread_mutex_unlock(&cache->lock);
    return 0;
}

/* Update entry's expiration time */
int cache_touch(cache_t* cache, const char* key, time_t ttl) {
    if (!cache || !key) return 0;
    
    pthread_mutex_lock(&cache->lock);
    
    /* Find entry */
    cache_entry_t* entry = find_entry(cache, key);
    
    if (entry) {
        /* Update expiration */
        time_t now = time(NULL);
        entry->expires = (ttl > 0) ? (now + ttl) : 
                       (cache->config.default_ttl > 0) ? (now + cache->config.default_ttl) : 0;
        
        pthread_mutex_unlock(&cache->lock);
        return 1;
    }
    
    pthread_mutex_unlock(&cache->lock);
    return 0;
}

/* Get all keys in the cache */
size_t cache_get_keys(cache_t* cache, char*** keys) {
    if (!cache || !keys) return 0;
    
    pthread_mutex_lock(&cache->lock);
    
    /* Allocate keys array */
    *keys = (char**)malloc(sizeof(char*) * cache->size);
    if (!*keys) {
        pthread_mutex_unlock(&cache->lock);
        return 0;
    }
    
    /* Copy keys */
    size_t count = 0;
    cache_entry_t* entry = cache->head;
    
    while (entry) {
        /* Check if entry has expired */
        time_t now = time(NULL);
        if (!(entry->expires > 0 && entry->expires <= now)) {
            (*keys)[count++] = strdup(entry->key);
        }
        entry = entry->next;
    }
    
    pthread_mutex_unlock(&cache->lock);
    
    return count;
}

/* Get cache statistics */
cache_stats_t cache_get_stats(cache_t* cache) {
    cache_stats_t stats = {0};
    
    if (!cache) return stats;
    
    pthread_mutex_lock(&cache->lock);
    
    stats.capacity = cache->config.capacity;
    stats.size = cache->size;
    stats.hits = cache->hits;
    stats.misses = cache->misses;
    stats.evictions = cache->evictions;
    stats.byte_size = cache->byte_size;
    
    /* Calculate hit ratio */
    size_t total = stats.hits + stats.misses;
    stats.hit_ratio = total > 0 ? ((double)stats.hits / total) : 0.0;
    
    pthread_mutex_unlock(&cache->lock);
    
    return stats;
}

/* Get cache statistics as JSON */
json_value_t* cache_get_stats_json(cache_t* cache) {
    if (!cache) return NULL;
    
    cache_stats_t stats = cache_get_stats(cache);
    
    /* Create JSON object */
    json_value_t* json = json_create_object();
    if (!json) return NULL;
    
    /* Add statistics */
    json_object_set(json, "capacity", json_create_integer(stats.capacity));
    json_object_set(json, "size", json_create_integer(stats.size));
    json_object_set(json, "hits", json_create_integer(stats.hits));
    json_object_set(json, "misses", json_create_integer(stats.misses));
    json_object_set(json, "evictions", json_create_integer(stats.evictions));
    json_object_set(json, "hit_ratio", json_create_number(stats.hit_ratio));
    json_object_set(json, "memory_bytes", json_create_number(stats.byte_size));
    json_object_set(json, "memory_mb", json_create_number(stats.byte_size / (1024.0 * 1024.0)));
    
    /* Add cache type */
    const char* type_str = "unknown";
    switch (cache->config.type) {
        case CACHE_TYPE_LRU:
            type_str = "lru";
            break;
        case CACHE_TYPE_LFU:
            type_str = "lfu";
            break;
        case CACHE_TYPE_FIFO:
            type_str = "fifo";
            break;
    }
    json_object_set(json, "type", json_create_string(type_str));
    
    /* Add TTL */
    json_object_set(json, "default_ttl", json_create_integer(cache->config.default_ttl));
    
    /* Add memory limit */
    json_object_set(json, "max_memory_mb", 
                 json_create_number(cache->config.max_memory_mb));
    
    return json;
}