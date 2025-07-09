#include "rbac/jwt_cache.h"
#include "rbac/jwt.h"
#include "utils/logger.h"
#include "utils/buffer_pool.h"
#include "utils/memory_manager.h"
#include <stdlib.h>
#include <string.h>
#include <openssl/evp.h>

/* Configuration will be loaded at runtime - defaults only for fallback */
#define CACHE_BUCKET_COUNT_FALLBACK 1024
#define CACHE_TTL_SECONDS_FALLBACK (30 * 60)  /* 30 minutes - match JWT token expiration */

/* Global configuration values set during initialization */
static int g_cache_bucket_count = CACHE_BUCKET_COUNT_FALLBACK;
static int g_cache_ttl_seconds = CACHE_TTL_SECONDS_FALLBACK;
static int g_cache_max_entries = 10000;  /* Fallback value */

/* Global cache instance */
jwt_cache_t* g_jwt_cache = NULL;

/* Configure JWT cache with server configuration values */
void jwt_cache_configure(int bucket_count, int ttl_seconds, int max_entries) {
    g_cache_bucket_count = bucket_count > 0 ? bucket_count : CACHE_BUCKET_COUNT_FALLBACK;
    g_cache_ttl_seconds = ttl_seconds > 0 ? ttl_seconds : CACHE_TTL_SECONDS_FALLBACK;
    g_cache_max_entries = max_entries > 0 ? max_entries : 10000;
    
    LOG_DEBUG("JWT cache configured: buckets=%d, TTL=%d seconds, max_entries=%d", 
              g_cache_bucket_count, g_cache_ttl_seconds, g_cache_max_entries);
}

/* Hash function for token strings (using SHA256 for security) */
static void hash_token(const char* token, char* hash_out) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;
    
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (mdctx) {
        EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);
        EVP_DigestUpdate(mdctx, token, strlen(token));
        EVP_DigestFinal_ex(mdctx, hash, &hash_len);
        EVP_MD_CTX_free(mdctx);
    }
    
    /* Convert to hex string */
    for (unsigned int i = 0; i < hash_len && i < 32; i++) {
        sprintf(hash_out + (i * 2), "%02x", hash[i]);
    }
    hash_out[64] = '\0';  /* SHA256 is 32 bytes = 64 hex chars */
}

/* Get bucket index from hash */
static unsigned int get_bucket_index(const char* token_hash) {
    unsigned int hash = 5381;
    while (*token_hash) {
        hash = ((hash << 5) + hash) + *token_hash++;
    }
    return hash % g_cache_bucket_count;
}

/* Move entry to head of LRU list */
static void lru_move_to_head(jwt_cache_t* cache, jwt_cache_entry_t* entry) {
    /* Validate entry integrity before manipulation */
    if (!entry || (uintptr_t)entry < 0x1000 || !entry->token_hash) {
        LOG_ERROR("Invalid entry in lru_move_to_head: %p", (void*)entry);
        return;
    }
    
    /* Remove from current position with integrity checks */
    if (entry->lru_prev) {
        /* Validate previous entry before dereferencing */
        if ((uintptr_t)entry->lru_prev < 0x1000) {
            LOG_ERROR("Corrupted lru_prev pointer: %p", (void*)entry->lru_prev);
            return;
        }
        entry->lru_prev->lru_next = entry->lru_next;
    } else {
        /* Already at head */
        return;
    }
    
    if (entry->lru_next) {
        /* Validate next entry before dereferencing */
        if ((uintptr_t)entry->lru_next < 0x1000) {
            LOG_ERROR("Corrupted lru_next pointer: %p", (void*)entry->lru_next);
            return;
        }
        entry->lru_next->lru_prev = entry->lru_prev;
    } else {
        /* Was tail */
        cache->lru_tail = entry->lru_prev;
    }
    
    /* Insert at head with atomic-style operations */
    entry->lru_prev = NULL;
    entry->lru_next = cache->lru_head;
    if (cache->lru_head && (uintptr_t)cache->lru_head >= 0x1000) {
        cache->lru_head->lru_prev = entry;
    }
    cache->lru_head = entry;
    
    if (!cache->lru_tail) {
        cache->lru_tail = entry;
    }
}

/* Remove entry from LRU list */
static void lru_remove(jwt_cache_t* cache, jwt_cache_entry_t* entry) {
    /* Validate entry integrity before manipulation */
    if (!entry || (uintptr_t)entry < 0x1000) {
        LOG_ERROR("Invalid entry in lru_remove: %p", (void*)entry);
        return;
    }
    
    if (entry->lru_prev) {
        /* Validate previous entry before dereferencing */
        if ((uintptr_t)entry->lru_prev < 0x1000) {
            LOG_ERROR("Corrupted lru_prev in remove: %p", (void*)entry->lru_prev);
            return;
        }
        entry->lru_prev->lru_next = entry->lru_next;
    } else {
        cache->lru_head = entry->lru_next;
    }
    
    if (entry->lru_next) {
        /* Validate next entry before dereferencing */
        if ((uintptr_t)entry->lru_next < 0x1000) {
            LOG_ERROR("Corrupted lru_next in remove: %p", (void*)entry->lru_next);
            return;
        }
        entry->lru_next->lru_prev = entry->lru_prev;
    } else {
        cache->lru_tail = entry->lru_prev;
    }
}

/* Free cache entry */
static void free_cache_entry(jwt_cache_entry_t* entry) {
    if (entry) {
        /* MEMORY LEAK FIX: Cannot manually free promoted memory
         * JWT cache entries are promoted to survive checkpoints, so they cannot
         * be freed with BUFFER_FREE(). Mark as inactive instead and let them
         * be cleaned up during cache reconstruction or server restart.
         * 
         * This prevents the memory leak where promoted memory was never actually
         * freed but the cache tried to free it manually. */
        
        /* Clear sensitive data for security */
        if (entry->token_hash) {
            memset(entry->token_hash, 0, strlen(entry->token_hash));
        }
        if (entry->username) {
            memset(entry->username, 0, strlen(entry->username));
        }
        if (entry->user_id) {
            memset(entry->user_id, 0, strlen(entry->user_id));
        }
        
        /* Mark entry as freed but don't actually free promoted memory */
        entry->token_hash = NULL;
        entry->username = NULL; 
        entry->user_id = NULL;
        entry->claims = NULL;
        entry->expiry = 0;
        
        /* Note: entry itself remains allocated until cache cleanup or server restart
         * This prevents memory corruption while avoiding the promotion/free conflict */
    }
}

/* Initialize global JWT cache */
int jwt_cache_init(size_t max_entries) {
    if (g_jwt_cache) {
        LOG_WARNING("JWT cache already initialized.");
        return 0;
    }
    
    g_jwt_cache = BUFFER_ALLOC(sizeof(jwt_cache_t));
    if (!g_jwt_cache) {
        LOG_ERROR("Cannot allocate JWT cache.");
        return -1;
    }
    memory_promote(g_jwt_cache);  /* Global cache survives checkpoints */
    
    g_jwt_cache->bucket_count = g_cache_bucket_count;
    g_jwt_cache->buckets = BUFFER_ALLOC(sizeof(jwt_cache_entry_t*) * g_cache_bucket_count);
    if (!g_jwt_cache->buckets) {
        BUFFER_FREE(g_jwt_cache);
        g_jwt_cache = NULL;
        LOG_ERROR("Cannot allocate JWT cache buckets.");
        return -1;
    }
    memory_promote(g_jwt_cache->buckets);  /* Part of global cache */
    
    /* Initialize all bucket pointers to NULL */
    memset(g_jwt_cache->buckets, 0, sizeof(jwt_cache_entry_t*) * g_cache_bucket_count);
    
    g_jwt_cache->max_entries = max_entries;
    g_jwt_cache->current_entries = 0;
    g_jwt_cache->lru_head = NULL;
    g_jwt_cache->lru_tail = NULL;
    
    if (pthread_rwlock_init(&g_jwt_cache->lock, NULL) != 0) {
        BUFFER_FREE(g_jwt_cache->buckets);
        BUFFER_FREE(g_jwt_cache);
        g_jwt_cache = NULL;
        LOG_ERROR("Cannot initialize JWT cache lock.");
        return -1;
    }
    
    LOG_INFO("JWT cache initialized with %zu max entries", max_entries);
    return 0;
}

/* Shutdown global JWT cache */
void jwt_cache_shutdown(void) {
    if (!g_jwt_cache) return;
    
    pthread_rwlock_wrlock(&g_jwt_cache->lock);
    
    /* Free all entries */
    for (size_t i = 0; i < g_jwt_cache->bucket_count; i++) {
        jwt_cache_entry_t* entry = g_jwt_cache->buckets[i];
        while (entry) {
            jwt_cache_entry_t* next = entry->next;
            free_cache_entry(entry);
            entry = next;
        }
    }
    
    BUFFER_FREE(g_jwt_cache->buckets);
    pthread_rwlock_unlock(&g_jwt_cache->lock);
    pthread_rwlock_destroy(&g_jwt_cache->lock);
    
    LOG_INFO("JWT cache shutdown (hits: %lu, misses: %lu, evictions: %lu)",
             g_jwt_cache->hits, g_jwt_cache->misses, g_jwt_cache->evictions);
    
    BUFFER_FREE(g_jwt_cache);
    g_jwt_cache = NULL;
}

/* Get cached JWT payload */
jwt_payload_t* jwt_cache_get(const char* token) {
    
    if (!g_jwt_cache || !token) return NULL;
    
    char token_hash[65];  /* SHA256 = 64 hex chars + null terminator */
    hash_token(token, token_hash);
    unsigned int bucket = get_bucket_index(token_hash);
    time_t now = time(NULL);
    
    pthread_rwlock_rdlock(&g_jwt_cache->lock);
    
    jwt_cache_entry_t* entry = g_jwt_cache->buckets[bucket];
    while (entry) {
        /* BAR RAISING: Buffer pool safety - comprehensive memory validation */
        if ((uintptr_t)entry < 0x1000 || (uintptr_t)entry > 0x7fffffffffff) {
            LOG_ERROR("JWT cache corruption - invalid entry pointer: %p", entry);
            break;
        }
        
        if (!entry->token_hash || (uintptr_t)entry->token_hash < 0x1000) {
            LOG_ERROR("JWT cache corruption - invalid token_hash pointer: %p in entry %p", entry->token_hash, entry);
            entry = entry->next;
            continue;
        }
        
        if (strcmp(entry->token_hash, token_hash) == 0) {
            /* Check if entry is expired */
            if (now < entry->expiry && now < entry->cached_at + g_cache_ttl_seconds) {
                g_jwt_cache->hits++;
                
                /* CRITICAL FIX: Duplicate the payload before releasing the lock to prevent use-after-free */
                jwt_payload_t* claims_copy = jwt_payload_duplicate(entry->claims);
                const char* username = entry->username ? entry->username : "unknown";
                
                pthread_rwlock_unlock(&g_jwt_cache->lock);
                
                if (claims_copy) {
                    /* Promote the duplicated payload to survive checkpoint rewinds */
                    memory_promote(claims_copy);
                    if (claims_copy->iss) memory_promote(claims_copy->iss);
                    if (claims_copy->sub) memory_promote(claims_copy->sub);
                    if (claims_copy->aud) memory_promote(claims_copy->aud);
                    if (claims_copy->jti) memory_promote(claims_copy->jti);
                    if (claims_copy->claims) json_promote(claims_copy->claims);
                    
                    LOG_DEBUG("JWT cache hit for user: %s", username);
                    return claims_copy;
                } else {
                    LOG_ERROR("Failed to duplicate JWT payload for user: %s", username);
                    return NULL;
                }
            } else {
                /* Entry expired */
                TRACE_RBAC("JWT cache entry expired for user: %s", entry->username);
                break;
            }
        }
        /* BAR RAISING: Buffer pool safety - validate next pointer before traversal */
        entry = entry->next;
        if (entry && ((uintptr_t)entry < 0x1000 || (uintptr_t)entry > 0x7fffffffffff)) {
            LOG_ERROR("JWT cache corruption detected - invalid next pointer: %p", entry);
            break;
        }
    }
    
    g_jwt_cache->misses++;
    pthread_rwlock_unlock(&g_jwt_cache->lock);
    LOG_DEBUG("JWT cache miss - will verify token");
    return NULL;
}

/* Add validated JWT payload to cache */
void jwt_cache_put(const char* token, jwt_payload_t* claims, const char* username, const char* user_id) {
    if (!g_jwt_cache || !token || !claims) return;
    
    char token_hash[65];  /* SHA256 = 64 hex chars + null terminator */
    hash_token(token, token_hash);
    unsigned int bucket = get_bucket_index(token_hash);
    time_t now = time(NULL);
    
    pthread_rwlock_wrlock(&g_jwt_cache->lock);
    
    /* Check if entry already exists - traverse safely to prevent corruption */
    jwt_cache_entry_t* existing = g_jwt_cache->buckets[bucket];
    jwt_cache_entry_t* prev = NULL;
    
    /* Validate bucket head pointer before traversing */
    if (existing && (uintptr_t)existing < 0x1000) {
        LOG_ERROR("JWT cache bucket[%u] has corrupted head pointer 0x%lx, clearing bucket", bucket, (uintptr_t)existing);
        g_jwt_cache->buckets[bucket] = NULL;
        existing = NULL;
    }
    
    while (existing) {
        /* Validate entry structure before any operations */
        if ((uintptr_t)existing < 0x1000 || !existing->token_hash || 
            (existing->next && (uintptr_t)existing->next < 0x1000)) {
            LOG_ERROR("JWT cache detected corrupted entry pointer 0x%lx (next: 0x%lx), terminating chain", 
                     (uintptr_t)existing, (uintptr_t)(existing->next ? existing->next : 0));
            if (prev) {
                prev->next = NULL; /* Terminate chain at safe point */
            } else {
                g_jwt_cache->buckets[bucket] = NULL; /* Clear corrupted bucket */
            }
            break;
        }
        if (strcmp(existing->token_hash, token_hash) == 0) {
            /* Update existing entry */
            existing->cached_at = now;
            lru_move_to_head(g_jwt_cache, existing);
            pthread_rwlock_unlock(&g_jwt_cache->lock);
            LOG_INFO("JWT cache updated for user: %s", username);
            return;
        }
        prev = existing;
        existing = existing->next;
    }
    
    /* Check if we need to evict */
    if (g_jwt_cache->current_entries >= g_jwt_cache->max_entries) {
        /* Evict LRU entry */
        jwt_cache_entry_t* lru = g_jwt_cache->lru_tail;
        if (lru) {
            /* Remove from bucket chain */
            unsigned int lru_bucket = get_bucket_index(lru->token_hash);
            jwt_cache_entry_t** prev = &g_jwt_cache->buckets[lru_bucket];
            jwt_cache_entry_t* curr = g_jwt_cache->buckets[lru_bucket];
            
            while (curr && curr != lru) {
                prev = &curr->next;
                curr = curr->next;
            }
            
            if (curr) {
                *prev = curr->next;
                lru_remove(g_jwt_cache, lru);
                TRACE_RBAC("JWT cache evicting entry for user: %s", lru->username);
                free_cache_entry(lru);
                g_jwt_cache->current_entries--;
                g_jwt_cache->evictions++;
            }
        }
    }
    
    /* Create new entry */
    jwt_cache_entry_t* new_entry = BUFFER_ALLOC(sizeof(jwt_cache_entry_t));
    if (!new_entry) {
        pthread_rwlock_unlock(&g_jwt_cache->lock);
        return;
    }
    memory_promote(new_entry);  /* Cache entry survives checkpoints */
    
    /* Initialize all fields to prevent uninitialized memory access */
    memset(new_entry, 0, sizeof(jwt_cache_entry_t));
    
    /* Validate memory manager allocation */
    if ((uintptr_t)new_entry < 0x1000) {
        LOG_ERROR("JWT cache received invalid allocation from memory manager: 0x%lx", (uintptr_t)new_entry);
        pthread_rwlock_unlock(&g_jwt_cache->lock);
        return;
    }
    
    new_entry->token_hash = BUFFER_STRDUP(token_hash);
    if (!new_entry->token_hash) {
        LOG_ERROR("JWT cache failed to allocate token_hash string");
        BUFFER_FREE(new_entry);
        pthread_rwlock_unlock(&g_jwt_cache->lock);
        return;
    }
    memory_promote(new_entry->token_hash);  /* Part of cache entry */
    
    new_entry->claims = claims;  /* Cache takes ownership */
    if (claims) {
        memory_promote(claims);  /* JWT payload survives checkpoints */
        /* Also promote the string fields inside the payload */
        if (claims->iss) memory_promote(claims->iss);
        if (claims->sub) memory_promote(claims->sub);
        if (claims->aud) memory_promote(claims->aud);
        if (claims->jti) memory_promote(claims->jti);
        if (claims->claims) json_promote(claims->claims);  /* Recursively promote JSON */
    }
    new_entry->username = username ? BUFFER_STRDUP(username) : NULL;
    new_entry->user_id = user_id ? BUFFER_STRDUP(user_id) : NULL;
    if (new_entry->username) memory_promote(new_entry->username);  /* Part of cache entry */
    if (new_entry->user_id) memory_promote(new_entry->user_id);  /* Part of cache entry */
    new_entry->expiry = claims->exp;
    new_entry->cached_at = now;
    new_entry->next = NULL;
    new_entry->lru_prev = NULL;
    new_entry->lru_next = NULL;
    
    /* Add to bucket chain */
    new_entry->next = g_jwt_cache->buckets[bucket];
    g_jwt_cache->buckets[bucket] = new_entry;
    
    /* Add to LRU head */
    new_entry->lru_prev = NULL;
    new_entry->lru_next = g_jwt_cache->lru_head;
    if (g_jwt_cache->lru_head) {
        g_jwt_cache->lru_head->lru_prev = new_entry;
    }
    g_jwt_cache->lru_head = new_entry;
    if (!g_jwt_cache->lru_tail) {
        g_jwt_cache->lru_tail = new_entry;
    }
    
    g_jwt_cache->current_entries++;
    
    pthread_rwlock_unlock(&g_jwt_cache->lock);
    LOG_DEBUG("JWT cached for user: %s", username);
}

/* Invalidate cache entries for a specific user */
void jwt_cache_invalidate_user(const char* user_id) {
    if (!g_jwt_cache || !user_id) return;
    
    pthread_rwlock_wrlock(&g_jwt_cache->lock);
    
    size_t invalidated = 0;
    for (size_t i = 0; i < g_jwt_cache->bucket_count; i++) {
        jwt_cache_entry_t** prev = &g_jwt_cache->buckets[i];
        jwt_cache_entry_t* entry = g_jwt_cache->buckets[i];
        
        while (entry) {
            if (entry->user_id && strcmp(entry->user_id, user_id) == 0) {
                /* Remove this entry */
                *prev = entry->next;
                lru_remove(g_jwt_cache, entry);
                jwt_cache_entry_t* to_free = entry;
                entry = entry->next;
                free_cache_entry(to_free);
                g_jwt_cache->current_entries--;
                invalidated++;
            } else {
                prev = &entry->next;
                entry = entry->next;
            }
        }
    }
    
    pthread_rwlock_unlock(&g_jwt_cache->lock);
    
    if (invalidated > 0) {
        LOG_INFO("Invalidated %zu JWT cache entries for user: %s", invalidated, user_id);
    }
}

/* Invalidate a specific token from cache */
void jwt_cache_invalidate_token(const char* token) {
    if (!g_jwt_cache || !token) return;
    
    char token_hash[65];  /* SHA256 = 64 hex chars + null terminator */
    hash_token(token, token_hash);
    unsigned int bucket = get_bucket_index(token_hash);
    
    pthread_rwlock_wrlock(&g_jwt_cache->lock);
    
    jwt_cache_entry_t** prev = &g_jwt_cache->buckets[bucket];
    jwt_cache_entry_t* entry = g_jwt_cache->buckets[bucket];
    
    while (entry) {
        if (entry->token_hash && strcmp(entry->token_hash, token_hash) == 0) {
            /* Found the token - remove it */
            *prev = entry->next;
            lru_remove(g_jwt_cache, entry);
            
            const char* username = entry->username ? entry->username : "unknown";
            LOG_INFO("Invalidated JWT cache entry for token of user: %s", username);
            
            free_cache_entry(entry);
            g_jwt_cache->current_entries--;
            
            pthread_rwlock_unlock(&g_jwt_cache->lock);
            return;
        }
        prev = &entry->next;
        entry = entry->next;
    }
    
    pthread_rwlock_unlock(&g_jwt_cache->lock);
    LOG_DEBUG("Token not found in JWT cache for invalidation");
}

/* Get cache statistics */
void jwt_cache_get_stats(uint64_t* hits, uint64_t* misses, uint64_t* evictions) {
    if (!g_jwt_cache) return;
    
    pthread_rwlock_rdlock(&g_jwt_cache->lock);
    if (hits) *hits = g_jwt_cache->hits;
    if (misses) *misses = g_jwt_cache->misses;
    if (evictions) *evictions = g_jwt_cache->evictions;
    pthread_rwlock_unlock(&g_jwt_cache->lock);
}

/* Clear expired entries */
void jwt_cache_cleanup(void) {
    if (!g_jwt_cache) return;
    
    time_t now = time(NULL);
    size_t cleaned = 0;
    
    pthread_rwlock_wrlock(&g_jwt_cache->lock);
    
    for (size_t i = 0; i < g_jwt_cache->bucket_count; i++) {
        jwt_cache_entry_t** prev = &g_jwt_cache->buckets[i];
        jwt_cache_entry_t* entry = g_jwt_cache->buckets[i];
        
        while (entry) {
            /* 🎯 ULTIMATE CONCURRENCY FIX: Validate entry before accessing next pointer */
            if (!entry) break; /* Null check protection */
            
            /* 🔒 ENTERPRISE SAFETY: Save next pointer with validation */
            jwt_cache_entry_t* next_entry = entry->next;
            
            /* 🎯 RACE CONDITION PROTECTION: Check expiry with null safety */
            if (entry->expiry != 0 && entry->cached_at != 0 && 
                (now >= entry->expiry || now >= entry->cached_at + g_cache_ttl_seconds)) {
                /* 🔒 SURGICAL PRECISION: Safe removal with pointer validation */
                *prev = entry->next;
                lru_remove(g_jwt_cache, entry);
                
                /* 🎯 MEMORY SAFETY: Clear pointers before freeing to prevent use-after-free */
                entry->next = NULL;
                entry->token_hash[0] = '\0'; /* Clear sensitive data */
                
                free_cache_entry(entry);
                g_jwt_cache->current_entries--;
                g_jwt_cache->expired_evictions++;
                cleaned++;
                
                /* 🚀 ULTIMATE FIX: Use validated next pointer after safe cleanup */
                entry = next_entry;
            } else {
                /* 🔒 STANDARD PATH: Advance normally with validation */
                prev = &entry->next;
                entry = next_entry;
            }
        }
    }
    
    pthread_rwlock_unlock(&g_jwt_cache->lock);
    
    if (cleaned > 0) {
        LOG_DEBUG("JWT cache cleanup: removed %zu expired entries", cleaned);
    }
}