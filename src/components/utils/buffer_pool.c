/**
 * High-performance buffer pool implementation
 * 
 * This implements thread-local buffer pools with multiple size classes
 * to reduce malloc/free overhead in hot paths.
 */

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>
#include "utils/logger.h"

/* Buffer pool configuration */
#define BUFFER_POOL_SIZES 4
#define POOL_SIZE_SMALL   512
#define POOL_SIZE_MEDIUM  4096
#define POOL_SIZE_LARGE   16384
#define POOL_SIZE_XLARGE  65536

#define BUFFERS_PER_POOL  32
#define MAX_FREE_BUFFERS  16

/* Buffer header for tracking */
typedef struct buffer_header {
    struct buffer_header* next;
    size_t size;
    size_t capacity;
    uint32_t magic;  /* For corruption detection */
} buffer_header_t;

#define BUFFER_MAGIC 0xBEEF3712

/* Pool for a specific size class */
typedef struct buffer_pool {
    buffer_header_t* free_list;
    size_t buffer_size;
    size_t allocated_count;
    size_t free_count;
    size_t hit_count;
    size_t miss_count;
    pthread_mutex_t lock;
} buffer_pool_t;

/* Thread-local buffer pools */
typedef struct thread_pools {
    buffer_pool_t pools[BUFFER_POOL_SIZES];
    int initialized;
} thread_pools_t;

/* Thread-local storage key */
static pthread_key_t tls_key;
static pthread_once_t tls_once = PTHREAD_ONCE_INIT;

/* Global statistics */
static struct {
    uint64_t total_allocations;
    uint64_t pool_hits;
    uint64_t pool_misses;
    pthread_mutex_t lock;
} global_stats = {
    .lock = PTHREAD_MUTEX_INITIALIZER
};

/* Size classes */
static const size_t pool_sizes[BUFFER_POOL_SIZES] = {
    POOL_SIZE_SMALL,
    POOL_SIZE_MEDIUM,
    POOL_SIZE_LARGE,
    POOL_SIZE_XLARGE
};

/* Cleanup function for thread exit */
static void cleanup_thread_pools(void* arg) {
    thread_pools_t* pools = (thread_pools_t*)arg;
    if (!pools) return;
    
    /* Free all buffers in all pools */
    for (int i = 0; i < BUFFER_POOL_SIZES; i++) {
        buffer_pool_t* pool = &pools->pools[i];
        
        pthread_mutex_lock(&pool->lock);
        
        buffer_header_t* buf = pool->free_list;
        while (buf) {
            buffer_header_t* next = buf->next;
            free(buf);
            buf = next;
        }
        
        pthread_mutex_unlock(&pool->lock);
        pthread_mutex_destroy(&pool->lock);
    }
    
    free(pools);
}

/* Initialize TLS key */
static void init_tls_key(void) {
    pthread_key_create(&tls_key, cleanup_thread_pools);
}

/* Get or create thread-local pools */
static thread_pools_t* get_thread_pools(void) {
    pthread_once(&tls_once, init_tls_key);
    
    thread_pools_t* pools = (thread_pools_t*)pthread_getspecific(tls_key);
    if (!pools) {
        pools = calloc(1, sizeof(thread_pools_t));
        if (!pools) return NULL;
        
        /* Initialize each pool */
        for (int i = 0; i < BUFFER_POOL_SIZES; i++) {
            buffer_pool_t* pool = &pools->pools[i];
            pool->buffer_size = pool_sizes[i];
            pthread_mutex_init(&pool->lock, NULL);
        }
        
        pools->initialized = 1;
        pthread_setspecific(tls_key, pools);
    }
    
    return pools;
}

/* Find the appropriate pool for a size */
static buffer_pool_t* find_pool_for_size(thread_pools_t* pools, size_t size) {
    for (int i = 0; i < BUFFER_POOL_SIZES; i++) {
        if (size <= pools->pools[i].buffer_size) {
            return &pools->pools[i];
        }
    }
    return NULL;
}

/* Allocate a buffer from the pool */
void* buffer_pool_alloc(size_t size) {
    if (size == 0) return NULL;
    
    thread_pools_t* pools = get_thread_pools();
    if (!pools) {
        /* Fall back to regular malloc */
        return malloc(size);
    }
    
    /* Find appropriate pool */
    buffer_pool_t* pool = find_pool_for_size(pools, size);
    if (!pool) {
        /* Size too large for pools */
        pthread_mutex_lock(&global_stats.lock);
        global_stats.total_allocations++;
        global_stats.pool_misses++;
        pthread_mutex_unlock(&global_stats.lock);
        
        return malloc(size);
    }
    
    pthread_mutex_lock(&pool->lock);
    
    buffer_header_t* buf = NULL;
    
    /* Try to get from free list */
    if (pool->free_list) {
        buf = pool->free_list;
        pool->free_list = buf->next;
        pool->free_count--;
        pool->hit_count++;
        
        /* Verify magic number */
        if (buf->magic != BUFFER_MAGIC) {
            if (g_logger) {
                LOG_ERROR("Buffer pool corruption detected!");
            }
            pthread_mutex_unlock(&pool->lock);
            abort();
        }
    } else {
        /* Allocate new buffer */
        buf = malloc(sizeof(buffer_header_t) + pool->buffer_size);
        if (buf) {
            buf->capacity = pool->buffer_size;
            buf->magic = BUFFER_MAGIC;
            pool->allocated_count++;
            pool->miss_count++;
        }
    }
    
    pthread_mutex_unlock(&pool->lock);
    
    if (!buf) return NULL;
    
    /* Update global stats */
    pthread_mutex_lock(&global_stats.lock);
    global_stats.total_allocations++;
    if (pool->free_list) {
        global_stats.pool_hits++;
    } else {
        global_stats.pool_misses++;
    }
    pthread_mutex_unlock(&global_stats.lock);
    
    /* Initialize buffer */
    buf->next = NULL;
    buf->size = size;
    
    /* Return pointer after header */
    return (char*)buf + sizeof(buffer_header_t);
}

/* Return a buffer to the pool */
void buffer_pool_free(void* ptr) {
    if (!ptr) return;
    
    /* Get buffer header */
    buffer_header_t* buf = (buffer_header_t*)((char*)ptr - sizeof(buffer_header_t));
    
    /* Verify magic number */
    if (buf->magic != BUFFER_MAGIC) {
        /* Not a pooled buffer, use regular free */
        free(ptr);
        return;
    }
    
    thread_pools_t* pools = get_thread_pools();
    if (!pools) {
        /* No pools, use regular free */
        free(buf);
        return;
    }
    
    /* Find the pool this buffer belongs to */
    buffer_pool_t* pool = NULL;
    for (int i = 0; i < BUFFER_POOL_SIZES; i++) {
        if (buf->capacity == pools->pools[i].buffer_size) {
            pool = &pools->pools[i];
            break;
        }
    }
    
    if (!pool) {
        /* Not from our pools */
        free(buf);
        return;
    }
    
    pthread_mutex_lock(&pool->lock);
    
    /* Check if we have too many free buffers */
    if (pool->free_count >= MAX_FREE_BUFFERS) {
        /* Free this buffer instead of pooling it */
        pool->allocated_count--;
        pthread_mutex_unlock(&pool->lock);
        free(buf);
        return;
    }
    
    /* Clear the buffer content for security */
    memset((char*)buf + sizeof(buffer_header_t), 0, buf->size);
    
    /* Add to free list */
    buf->next = pool->free_list;
    pool->free_list = buf;
    pool->free_count++;
    
    pthread_mutex_unlock(&pool->lock);
}

/* Reallocate a buffer */
void* buffer_pool_realloc(void* ptr, size_t new_size) {
    if (!ptr) {
        return buffer_pool_alloc(new_size);
    }
    
    if (new_size == 0) {
        buffer_pool_free(ptr);
        return NULL;
    }
    
    /* Get buffer header */
    buffer_header_t* buf = (buffer_header_t*)((char*)ptr - sizeof(buffer_header_t));
    
    /* Check if it's a pooled buffer */
    if (buf->magic != BUFFER_MAGIC) {
        /* Not pooled, use regular realloc */
        return realloc(ptr, new_size);
    }
    
    /* If new size fits in current buffer, just update size */
    if (new_size <= buf->capacity) {
        buf->size = new_size;
        return ptr;
    }
    
    /* Need a larger buffer */
    void* new_ptr = buffer_pool_alloc(new_size);
    if (!new_ptr) return NULL;
    
    /* Copy old data */
    memcpy(new_ptr, ptr, buf->size);
    
    /* Free old buffer */
    buffer_pool_free(ptr);
    
    return new_ptr;
}

/* Duplicate a string using buffer pool */
char* buffer_pool_strdup(const char* str) {
    if (!str) return NULL;
    
    size_t len = strlen(str) + 1;
    char* new_str = buffer_pool_alloc(len);
    if (!new_str) return NULL;
    
    memcpy(new_str, str, len);
    return new_str;
}

/* Get pool statistics */
void buffer_pool_get_stats(uint64_t* total_allocs, uint64_t* pool_hits, uint64_t* pool_misses) {
    pthread_mutex_lock(&global_stats.lock);
    if (total_allocs) *total_allocs = global_stats.total_allocations;
    if (pool_hits) *pool_hits = global_stats.pool_hits;
    if (pool_misses) *pool_misses = global_stats.pool_misses;
    pthread_mutex_unlock(&global_stats.lock);
}

/* Reset pool statistics */
void buffer_pool_reset_stats(void) {
    pthread_mutex_lock(&global_stats.lock);
    global_stats.total_allocations = 0;
    global_stats.pool_hits = 0;
    global_stats.pool_misses = 0;
    pthread_mutex_unlock(&global_stats.lock);
}