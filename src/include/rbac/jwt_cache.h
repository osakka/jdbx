#ifndef JWT_CACHE_H
#define JWT_CACHE_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <pthread.h>
#include "rbac/jwt.h"

/* JWT Cache entry */
typedef struct jwt_cache_entry {
    char* token_hash;           /* SHA256 hash of token for security */
    jwt_payload_t* claims;       /* Cached validated claims */
    char* username;             /* Cached username for quick access */
    char* user_id;              /* Cached user ID */
    time_t expiry;              /* Token expiry time */
    time_t cached_at;           /* When this entry was cached */
    struct jwt_cache_entry* next; /* Hash collision chain */
    struct jwt_cache_entry* lru_prev; /* LRU double-linked list */
    struct jwt_cache_entry* lru_next;
} jwt_cache_entry_t;

/* JWT Cache structure */
typedef struct jwt_cache {
    jwt_cache_entry_t** buckets;    /* Hash table buckets */
    size_t bucket_count;            /* Number of buckets */
    size_t max_entries;             /* Maximum cache entries */
    size_t current_entries;         /* Current number of entries */
    jwt_cache_entry_t* lru_head;    /* LRU list head (most recent) */
    jwt_cache_entry_t* lru_tail;    /* LRU list tail (least recent) */
    pthread_rwlock_t lock;          /* Read-write lock for thread safety */
    
    /* Statistics */
    uint64_t hits;
    uint64_t misses;
    uint64_t evictions;
    uint64_t expired_evictions;
} jwt_cache_t;

/* Global JWT cache instance */
extern jwt_cache_t* g_jwt_cache;

/* Configure JWT cache with server configuration values */
void jwt_cache_configure(int bucket_count, int ttl_seconds, int max_entries);

/* Initialize global JWT cache */
int jwt_cache_init(size_t max_entries);

/* Shutdown global JWT cache */
void jwt_cache_shutdown(void);

/* Get cached JWT payload (returns NULL if not found or expired) */
jwt_payload_t* jwt_cache_get(const char* token);

/* Add validated JWT payload to cache */
void jwt_cache_put(const char* token, jwt_payload_t* claims, const char* username, const char* user_id);

/* Invalidate cache entries for a specific user */
void jwt_cache_invalidate_user(const char* user_id);

/* Invalidate a specific token from cache */
void jwt_cache_invalidate_token(const char* token);

/* Get cache statistics */
void jwt_cache_get_stats(uint64_t* hits, uint64_t* misses, uint64_t* evictions);

/* Clear expired entries (called periodically) */
void jwt_cache_cleanup(void);

#endif /* JWT_CACHE_H */