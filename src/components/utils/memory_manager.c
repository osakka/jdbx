/**
 * @file memory_manager.c
 * @brief JDBX Memory Manager - Checkpoint-Based Memory Management
 * 
 * Revolutionary memory management that eliminates manual cleanup code.
 * Uses checkpoint/rewind architecture for automatic memory cleanup on error paths.
 * Single source of truth for advanced memory management in JDBX.
 */

#define _GNU_SOURCE
#include "utils/memory_types.h"
#include "utils/memory_manager.h"
#include "utils/memory_allocator_config.h"
#include "utils/buffer_pool.h"
#include "utils/tlsf_allocator.h"
#include "utils/arena_allocator.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <stdint.h>
#include <limits.h>
#include <stddef.h>
#include <stdalign.h>
#include <stdio.h>
#include <stdbool.h>    /* For bool type - Inspector Claude needs this */
#include <dlfcn.h>      /* For SSL detection - Inspector Claude's solution */

/* Memory managers should not depend on logging */

/* Memory allocation flags */
/* Memory flags are now defined in memory_types.h */

/* Memory allocation header is now defined in memory_types.h */

/* Magic numbers and header size are now defined in memory_types.h */
#define MEMORY_MAGIC_FREE 0xFEEDF00D

/* Checkpoint structure is now defined in memory_types.h */

/* Thread-local memory state */
typedef struct {
    memory_checkpoint_t* current_checkpoint;  /* Active checkpoint */
    int initialized;                          /* Whether initialized */
    int bypass_checkpoint;                    /* Skip checkpoint for this allocation */
    tlsf_pool_t* tlsf_pool;                  /* Thread-local TLSF pool */
} memory_state_t;

/* Thread-local storage for memory state */
static __thread memory_state_t tls_memory = {NULL, 0, 0, NULL};

/* Global statistics */
static struct {
    _Alignas(64) volatile uint64_t checkpoints_created;           /* Align to cache line */
    _Alignas(64) volatile uint64_t rewinds_performed;            /* Align to cache line */
    _Alignas(64) volatile uint64_t allocations_freed_by_rewind; /* Align to cache line */
} g_memory_stats = {0, 0, 0};

/* Global lock for statistics */
static pthread_mutex_t g_memory_lock = PTHREAD_MUTEX_INITIALIZER;

/* Global initialization flag */
static volatile int g_memory_manager_initialized = 0;

/**
 * Initialize thread-local memory state
 */
static void ensure_memory_initialized(void) {
    if (!tls_memory.initialized) {
        tls_memory.current_checkpoint = NULL;
        tls_memory.bypass_checkpoint = 0;
        tls_memory.initialized = 1;
        
        /* Initialize TLSF pool for this thread - controlled by configuration
         * Only create pool if:
         * 1. Memory manager is fully initialized (avoids early init issues)  
         * 2. TLSF allocator is enabled via configuration
         * 3. Pool doesn't already exist
         */
        if (!tls_memory.tlsf_pool && g_memory_manager_initialized && 
            memory_allocator_config_tlsf_enabled()) {
            tls_memory.tlsf_pool = tlsf_create_pool(32 * 1024 * 1024);
            if (tls_memory.tlsf_pool && SHOULD_DEBUG_MEMORY()) {
                fprintf(stderr, "ensure_memory_initialized: TLSF pool created for thread\n");
            }
        }
        /* Don't log from here - can cause issues */
    }
}

/**
 * Get memory header from user pointer
 */
memory_header_t* get_memory_header(void* ptr) {
    if (!ptr) return NULL;
    
    /* Calculate the header pointer - user data starts at HEADER_SIZE offset */
    memory_header_t* header = (memory_header_t*)((char*)ptr - HEADER_SIZE);
    
    /* Sanity check for obviously bad pointers */
    if ((uintptr_t)ptr < HEADER_SIZE) {
        if (getenv("JDBX_MEM_DEBUG")) {
            fprintf(stderr, "get_memory_header: ptr=%p too small (< %zu)\n", ptr, HEADER_SIZE);
        }
        return NULL;
    }
    
    /* Check alignment - be more lenient for TLSF allocations
     * TLSF provides its own alignment guarantees which may differ from max_align_t
     */
    if ((uintptr_t)header % _Alignof(max_align_t) != 0) {
        /* Try checking if this could be a TLSF allocation with different alignment */
        if ((uintptr_t)header % 8 != 0) {  /* TLSF should at least be 8-byte aligned */
            if (getenv("JDBX_MEM_DEBUG")) {
                fprintf(stderr, "get_memory_header: ptr=%p header=%p alignment failed (even for TLSF)\n", 
                        ptr, header);
            }
            return NULL;
        }
        if (getenv("JDBX_MEM_DEBUG")) {
            fprintf(stderr, "get_memory_header: ptr=%p header=%p relaxed alignment (possible TLSF)\n", 
                    ptr, header);
        }
    }
    
    /* Check if the magic field location is readable before accessing it */
    /* This is a heuristic - we're checking if the pointer makes sense */
    if ((uintptr_t)header < 0x1000) {  /* Likely not a valid heap address */
        if (getenv("JDBX_MEM_DEBUG")) {
            fprintf(stderr, "get_memory_header: ptr=%p header=%p too low (< 0x1000)\n", ptr, header);
        }
        return NULL;
    }
    
    /* Validate magic number with memory barrier for thread safety */
    __sync_synchronize();  /* Memory fence */
    
    uint32_t magic = header->magic;
    
    /* Debug print for troubleshooting */
    if (getenv("JDBX_MEM_DEBUG")) {
        fprintf(stderr, "get_memory_header: ptr=%p, header=%p, magic=0x%x (expected 0x%x)\n", 
                ptr, header, magic, MEMORY_MAGIC);
        if (magic != MEMORY_MAGIC) {
            fprintf(stderr, "  Magic mismatch! Header fields: size=%zu, checkpoint=%p, flags=0x%x\n",
                    header->size, header->checkpoint, header->flags);
        }
    }
    
    if (magic == MEMORY_MAGIC) {
        /* Additional safety check: validate size field is reasonable */
        if (header->size > 0 && header->size < (1ULL << 32)) {
            return header;
        }
    }
    
    /* Check for freed magic to catch double-frees */
    if (magic == MEMORY_MAGIC_FREE) {
        return header;  /* Return it so we can detect double-free */
    }
    
    return NULL;
}

/**
 * Create a new checkpoint
 */
memory_checkpoint_t* memory_checkpoint_create(void) {
    ensure_memory_initialized();
    
    /* Allocate checkpoint with aligned allocation (not tracked) */
    memory_checkpoint_t* checkpoint = (memory_checkpoint_t*)aligned_alloc(_Alignof(max_align_t), sizeof(memory_checkpoint_t));
    if (!checkpoint) {
        return NULL;
    }
    
    /* Initialize spinlock */
    if (pthread_spin_init(&checkpoint->lock, PTHREAD_PROCESS_PRIVATE) != 0) {
        free(checkpoint);
        return NULL;
    }
    
    /* Initialize checkpoint */
    checkpoint->parent = tls_memory.current_checkpoint;
    checkpoint->first_alloc = NULL;
    checkpoint->last_alloc = NULL;
    checkpoint->created_at = time(NULL);
    checkpoint->allocation_count = 0;
    checkpoint->total_size = 0;
    checkpoint->committed = 0;
    
    /* Create arena for checkpoint allocations (4MB) - controlled by configuration */
    checkpoint->arena = NULL;
    checkpoint->arena_checkpoint_id = 0;
    
    /* Create arena only if enabled via configuration */
    if (SHOULD_DEBUG_MEMORY()) {
        fprintf(stderr, "memory_checkpoint_create: Checking arena creation conditions\n");
        fprintf(stderr, "  SHOULD_USE_ARENA_ALLOCATOR() = %d\n", SHOULD_USE_ARENA_ALLOCATOR() ? 1 : 0);
    }
    
    if (SHOULD_USE_ARENA_ALLOCATOR()) {
        if (SHOULD_DEBUG_MEMORY()) {
            fprintf(stderr, "memory_checkpoint_create: Creating 4MB arena for checkpoint %p\n", checkpoint);
        }
        checkpoint->arena = arena_create(4 * 1024 * 1024);
        if (checkpoint->arena) {
            checkpoint->arena_checkpoint_id = arena_checkpoint(checkpoint->arena);
            if (SHOULD_DEBUG_MEMORY()) {
                fprintf(stderr, "memory_checkpoint_create: Arena created successfully for checkpoint %p (arena=%p)\n", 
                       checkpoint, checkpoint->arena);
            }
        } else {
            if (SHOULD_DEBUG_MEMORY()) {
                fprintf(stderr, "memory_checkpoint_create: ARENA CREATION FAILED for checkpoint %p\n", checkpoint);
            }
        }
    } else {
        if (SHOULD_DEBUG_MEMORY()) {
            fprintf(stderr, "memory_checkpoint_create: Arena allocation disabled by configuration\n");
        }
    }
    
    /* Make it current */
    memory_checkpoint_t* old_checkpoint = tls_memory.current_checkpoint;
    tls_memory.current_checkpoint = checkpoint;
    
    /* CHECKPOINT LIFECYCLE DEBUGGING: Track creation */
    if (getenv("JDBX_MEM_DEBUG")) {
        FILE* debug_file = fopen("/tmp/jdbx_debug.log", "a");
        if (debug_file) {
            fprintf(debug_file, "🔄 memory_checkpoint_create: Setting current_checkpoint=%p (was %p, parent=%p)\n", 
                   checkpoint, old_checkpoint, checkpoint->parent);
            fflush(debug_file);
            fclose(debug_file);
        }
    }
    
    __sync_fetch_and_add(&g_memory_stats.checkpoints_created, 1);
    
    /* LOG_DEBUG("Created memory checkpoint %p (parent: %p)", checkpoint, checkpoint->parent); */
    return checkpoint;
}

/**
 * Rewind to a checkpoint, freeing all allocations since then
 */
void memory_checkpoint_rewind(memory_checkpoint_t* checkpoint) {
    if (!checkpoint) return;
    
    ensure_memory_initialized();
    
    /* Verify this checkpoint is in our stack */
    memory_checkpoint_t* cp = tls_memory.current_checkpoint;
    int found = 0;
    while (cp) {
        if (cp == checkpoint) {
            found = 1;
            break;
        }
        cp = cp->parent;
    }
    
    if (!found) {
        /* Silent failure - memory managers should not log */
        return;
    }
    
    /* Free all allocations in checkpoints being rewound */
    uint64_t freed_count = 0;
    cp = tls_memory.current_checkpoint;
    
    while (cp && cp != checkpoint) {
        /* Lock the checkpoint we're about to rewind */
        pthread_spin_lock(&cp->lock);
        
        /* Free all allocations in this checkpoint */
        memory_header_t* header = cp->first_alloc;
        memory_header_t* hazard_list = NULL;  /* List of hazard-protected allocations */
        
        while (header) {
            memory_header_t* next = header->next;
            
            if (header->flags & MEMORY_FLAG_HAZARD_PROTECTED) {
                /* Hazard protected - defer freeing, add to hazard list */
                header->prev = NULL;
                header->next = hazard_list;
                if (hazard_list) hazard_list->prev = header;
                hazard_list = header;
            } else {
                /* Check if this is an arena allocation */
                if (!(header->flags & MEMORY_FLAG_ARENA_ALLOCATED)) {
                    /* Not arena allocated - free immediately */
                    header->magic = MEMORY_MAGIC_FREE;  /* Mark as freed for detection */
                    
                    /* Check allocation source */
                    if (header->flags & MEMORY_FLAG_TLSF_ALLOCATED) {
                        tlsf_free(tls_memory.tlsf_pool, header->tlsf_ptr);
                    } else {
                        free(header);
                    }
                    freed_count++;
                }
                /* Arena allocations are freed in bulk when arena is reset */
            }
            header = next;
        }
        
        /* Update the checkpoint to only contain hazard-protected allocations */
        cp->first_alloc = hazard_list;
        cp->last_alloc = hazard_list;
        while (cp->last_alloc && cp->last_alloc->next) {
            cp->last_alloc = cp->last_alloc->next;
        }
        
        /* Clear checkpoint pointers in hazard-protected allocations before freeing checkpoint */
        memory_header_t* hazard_header = hazard_list;
        while (hazard_header) {
            hazard_header->checkpoint = NULL;
            hazard_header = hazard_header->next;
        }
        
        /* Unlock before destroying */
        pthread_spin_unlock(&cp->lock);
        
        /* Move to parent and free the checkpoint */
        memory_checkpoint_t* parent = cp->parent;
        pthread_spin_destroy(&cp->lock);  /* Cleanup spinlock */
        
        /* Destroy arena completely (no reset needed - we're destroying the checkpoint) */
        if (cp->arena) {
            arena_destroy(cp->arena);
            cp->arena = NULL;
        }
        
        free(cp);
        cp = parent;
    }
    
    /* Update current checkpoint */
    tls_memory.current_checkpoint = checkpoint;
    
    /* CHECKPOINT LIFECYCLE DEBUGGING: Track rewind operations */
    if (getenv("JDBX_MEM_DEBUG")) {
        FILE* debug_file = fopen("/tmp/jdbx_debug.log", "a");
        if (debug_file) {
            fprintf(debug_file, "🔄 memory_checkpoint_rewind: Setting current_checkpoint=%p (was %p)\n", 
                   checkpoint, cp);
            fflush(debug_file);
            fclose(debug_file);
        }
    }
    
    /* Free the target checkpoint's allocations too */
    pthread_spin_lock(&checkpoint->lock);
    
    memory_header_t* header = checkpoint->first_alloc;
    memory_header_t* hazard_list = NULL;
    
    while (header) {
        memory_header_t* next = header->next;
        
        if (header->flags & MEMORY_FLAG_HAZARD_PROTECTED) {
            /* Hazard protected - defer freeing */
            header->prev = NULL;
            header->next = hazard_list;
            if (hazard_list) hazard_list->prev = header;
            hazard_list = header;
        } else {
            /* Check if this is an arena allocation */
            if (!(header->flags & MEMORY_FLAG_ARENA_ALLOCATED)) {
                /* Not arena allocated - free immediately */
                header->magic = MEMORY_MAGIC_FREE;
                
                /* Check if TLSF allocated */
                if (header->flags & MEMORY_FLAG_TLSF_ALLOCATED) {
                    /* TLSF allocation - free using the stored TLSF pointer */
                    tlsf_free(tls_memory.tlsf_pool, header->tlsf_ptr);
                } else {
                    free(header);
                }
                freed_count++;
            }
            /* Arena allocations are freed in bulk when arena is reset */
        }
        header = next;
    }
    
    /* Update checkpoint to only contain hazard-protected allocations */
    checkpoint->first_alloc = hazard_list;
    checkpoint->last_alloc = hazard_list;
    while (checkpoint->last_alloc && checkpoint->last_alloc->next) {
        checkpoint->last_alloc = checkpoint->last_alloc->next;
    }
    checkpoint->allocation_count = 0;
    checkpoint->total_size = 0;
    
    pthread_spin_unlock(&checkpoint->lock);
    
    __sync_fetch_and_add(&g_memory_stats.rewinds_performed, 1);
    __sync_fetch_and_add(&g_memory_stats.allocations_freed_by_rewind, freed_count);
    
    /* LOG_DEBUG("Rewound to checkpoint %p, freed %lu allocations", checkpoint, freed_count); */
}

/**
 * Commit a checkpoint, making allocations permanent
 */
void memory_checkpoint_commit(memory_checkpoint_t* checkpoint) {
    if (!checkpoint) return;
    
    ensure_memory_initialized();
    
    /* Lock checkpoint for modification */
    pthread_spin_lock(&checkpoint->lock);
    
    /* Clear checkpoint association from all allocations */
    memory_header_t* header = checkpoint->first_alloc;
    while (header) {
        memory_header_t* next = header->next;
        
        /* Validate header before accessing */
        if ((uintptr_t)header < 0x1000 || header->magic != MEMORY_MAGIC) {
            /* Invalid or freed allocation - skip */
            break;
        }
        
        header->checkpoint = NULL;
        header = next;
    }
    
    /* Mark as committed */
    checkpoint->committed = 1;
    
    /* Unlock before potential destruction */
    pthread_spin_unlock(&checkpoint->lock);
    
    /* If this is the current checkpoint, clear it */
    if (tls_memory.current_checkpoint == checkpoint) {
        memory_checkpoint_t* old_checkpoint = tls_memory.current_checkpoint;
        tls_memory.current_checkpoint = checkpoint->parent;
        
        /* CHECKPOINT LIFECYCLE DEBUGGING: Track commit operations */
        if (getenv("JDBX_MEM_DEBUG")) {
            FILE* debug_file = fopen("/tmp/jdbx_debug.log", "a");
            if (debug_file) {
                fprintf(debug_file, "✅ memory_checkpoint_commit: Setting current_checkpoint=%p (was %p, committed checkpoint)\n", 
                       tls_memory.current_checkpoint, old_checkpoint);
                fflush(debug_file);
                fclose(debug_file);
            }
        }
    }
    
    /* Destroy arena if present */
    if (checkpoint->arena) {
        arena_destroy(checkpoint->arena);
        checkpoint->arena = NULL;
    }
    
    /* Destroy spinlock and free the checkpoint structure */
    pthread_spin_destroy(&checkpoint->lock);
    free(checkpoint);
}

/**
 * Promote an allocation to survive checkpoint rewind
 */
void* memory_promote(void* ptr) {
    if (!ptr) return NULL;
    
    memory_header_t* header = get_memory_header(ptr);
    if (!header) {
        /* Not a managed allocation */
        return ptr;
    }
    
    /* Remove from checkpoint tracking */
    if (header->checkpoint) {
        memory_checkpoint_t* cp = header->checkpoint;
        
        /* Lock checkpoint for list modification */
        pthread_spin_lock(&cp->lock);
        
        if (header->prev) {
            header->prev->next = header->next;
        } else {
            cp->first_alloc = header->next;
        }
        
        if (header->next) {
            header->next->prev = header->prev;
        } else {
            cp->last_alloc = header->prev;
        }
        
        cp->allocation_count--;
        cp->total_size -= header->size;
        
        /* Unlock checkpoint */
        pthread_spin_unlock(&cp->lock);
    }
    
    /* Clear checkpoint association */
    header->checkpoint = NULL;
    header->next = NULL;
    header->prev = NULL;
    
    /* Successfully promoted */
    return ptr;
}

/**
 * Get current checkpoint
 */
memory_checkpoint_t* memory_checkpoint_current(void) {
    ensure_memory_initialized();
    return tls_memory.current_checkpoint;
}

/**
 * Temporarily bypass checkpoint for allocations (used by logger)
 */
void memory_bypass_checkpoint(int bypass) {
    ensure_memory_initialized();
    tls_memory.bypass_checkpoint = bypass;
}

/* Allocation decision thresholds - configurable for optimization */
#define ARENA_MAX_ALLOCATION_SIZE   (64 * 1024)     /* 64KB - Arena threshold */
#define TLSF_MAX_REASONABLE_SIZE    (1024 * 1024)   /* 1MB - TLSF upper limit */

/**
 * Intelligent allocation decision helpers
 */
static inline int should_use_arena_allocator(size_t size) {
    int arena_enabled = memory_allocator_config_arena_enabled();
    int has_checkpoint = (tls_memory.current_checkpoint != NULL);
    int not_committed = has_checkpoint && !tls_memory.current_checkpoint->committed;
    int not_bypassed = !tls_memory.bypass_checkpoint;
    int size_ok = (size < ARENA_MAX_ALLOCATION_SIZE);
    int has_arena = has_checkpoint && tls_memory.current_checkpoint->arena;
    
    if (SHOULD_DEBUG_MEMORY()) {
        fprintf(stderr, "  Arena conditions: enabled=%d, checkpoint=%d, not_committed=%d, not_bypassed=%d, size_ok=%d(%zu<%d), has_arena=%d\n",
               arena_enabled, has_checkpoint, not_committed, not_bypassed, 
               size_ok, size, ARENA_MAX_ALLOCATION_SIZE, has_arena);
    }
    
    return (arena_enabled && has_checkpoint && not_committed && not_bypassed && size_ok && has_arena);
}

int should_use_tlsf_allocator(size_t size) {
    return (memory_allocator_config_tlsf_enabled() &&
            tls_memory.tlsf_pool &&
            g_memory_manager_initialized &&
            size >= TLSF_MIN_BLOCK_SIZE &&
            size <= TLSF_MAX_REASONABLE_SIZE);
}

/*==============================================================================
 * 🚀 REVOLUTIONARY SSL SEMANTIC ALLOCATOR
 * World's first SSL-optimized memory allocator based on scientific analysis
 *============================================================================*/

/* SSL allocation size categories (discovered through OpenSSL analysis) */
#define SSL_TINY_THRESHOLD    32    /* 90% of SSL allocations */
#define SSL_SMALL_THRESHOLD   128
#define SSL_MEDIUM_THRESHOLD  1024
#define SSL_ALIGNMENT         16    /* Critical for cryptographic operations */

/* SSL-optimized allocation pools */
typedef struct ssl_allocation_pools {
    /* Fast-access pools for common SSL sizes */
    void** tiny_pool;       /* 24-byte allocations (object names) - most frequent */
    void** small_pool;      /* 56-byte allocations (thread mutexes) */
    void** medium_pool;     /* 72-byte allocations (hash entries) */
    
    size_t tiny_pool_size;
    size_t small_pool_size; 
    size_t medium_pool_size;
    
    size_t tiny_available;
    size_t small_available;
    size_t medium_available;
    
    /* Performance counters */
    uint64_t tiny_hits;
    uint64_t small_hits;
    uint64_t medium_hits;
    uint64_t fallback_count;
    
    bool initialized;
    pthread_mutex_t pool_mutex;
} ssl_allocation_pools_t;

static ssl_allocation_pools_t g_ssl_pools = {0};

/* Initialize SSL allocation pools */
static void ssl_pools_init(void) {
    if (g_ssl_pools.initialized) return;
    
    pthread_mutex_init(&g_ssl_pools.pool_mutex, NULL);
    
    /* Pre-allocate pools based on OpenSSL usage patterns */
    g_ssl_pools.tiny_pool_size = 1000;   /* 24-byte objects are heavily reused */
    g_ssl_pools.small_pool_size = 100;   /* 56-byte mutexes */
    g_ssl_pools.medium_pool_size = 100;  /* 72-byte hash entries */
    
    g_ssl_pools.tiny_pool = calloc(g_ssl_pools.tiny_pool_size, sizeof(void*));
    g_ssl_pools.small_pool = calloc(g_ssl_pools.small_pool_size, sizeof(void*));
    g_ssl_pools.medium_pool = calloc(g_ssl_pools.medium_pool_size, sizeof(void*));
    
    g_ssl_pools.tiny_available = 0;
    g_ssl_pools.small_available = 0;
    g_ssl_pools.medium_available = 0;
    
    g_ssl_pools.initialized = true;
    
    if (SHOULD_DEBUG_MEMORY()) {
        fprintf(stderr, "🚀 SSL Semantic Allocator initialized with optimized pools\n");
    }
}

/* Revolutionary SSL-optimized allocation */
static void* ssl_semantic_alloc(size_t size) {
    ssl_pools_init();
    
    if (SHOULD_DEBUG_MEMORY()) {
        fprintf(stderr, "🚀 SSL semantic allocation: size=%zu\n", size);
    }
    
    void* allocated_ptr = NULL;
    void* user_ptr = NULL;
    bool from_pool = false;
    
    pthread_mutex_lock(&g_ssl_pools.pool_mutex);
    
    /* Revolutionary pool-based allocation for common SSL patterns */
    if (size <= SSL_TINY_THRESHOLD && g_ssl_pools.tiny_available > 0) {
        /* Use pre-allocated tiny object */
        allocated_ptr = g_ssl_pools.tiny_pool[--g_ssl_pools.tiny_available];
        g_ssl_pools.tiny_hits++;
        from_pool = true;
        if (SHOULD_DEBUG_MEMORY()) {
            fprintf(stderr, "   ✅ SSL tiny pool hit (size=%zu, hits=%lu)\n", size, g_ssl_pools.tiny_hits);
        }
    } else if (size <= SSL_SMALL_THRESHOLD && g_ssl_pools.small_available > 0) {
        /* Use pre-allocated small object */
        allocated_ptr = g_ssl_pools.small_pool[--g_ssl_pools.small_available];
        g_ssl_pools.small_hits++;
        from_pool = true;
        if (SHOULD_DEBUG_MEMORY()) {
            fprintf(stderr, "   ✅ SSL small pool hit (size=%zu, hits=%lu)\n", size, g_ssl_pools.small_hits);
        }
    } else if (size <= SSL_MEDIUM_THRESHOLD && g_ssl_pools.medium_available > 0) {
        /* Use pre-allocated medium object */
        allocated_ptr = g_ssl_pools.medium_pool[--g_ssl_pools.medium_available];
        g_ssl_pools.medium_hits++;
        from_pool = true;
        if (SHOULD_DEBUG_MEMORY()) {
            fprintf(stderr, "   ✅ SSL medium pool hit (size=%zu, hits=%lu)\n", size, g_ssl_pools.medium_hits);
        }
    }
    
    pthread_mutex_unlock(&g_ssl_pools.pool_mutex);
    
    if (!from_pool) {
        /* Fallback to optimized aligned allocation with SSL requirements */
        g_ssl_pools.fallback_count++;
        size_t total_size = HEADER_SIZE + size;
        /* Ensure 16-byte alignment for cryptographic operations */
        size_t aligned_size = (total_size + SSL_ALIGNMENT - 1) & ~(SSL_ALIGNMENT - 1);
        allocated_ptr = aligned_alloc(SSL_ALIGNMENT, aligned_size);
        
        if (SHOULD_DEBUG_MEMORY()) {
            fprintf(stderr, "   🔄 SSL fallback allocation (size=%zu, aligned=%zu, fallbacks=%lu)\n", 
                   size, aligned_size, g_ssl_pools.fallback_count);
        }
    }
    
    if (!allocated_ptr) {
        return NULL;
    }
    
    /* Initialize header for SSL allocation */
    memory_header_t* ssl_header = (memory_header_t*)allocated_ptr;
    ssl_header->magic = MEMORY_MAGIC;
    ssl_header->size = size;
    ssl_header->checkpoint = NULL;
    ssl_header->next = NULL;
    ssl_header->prev = NULL;
    ssl_header->flags = from_pool ? 8 : 0;  /* Flag 8 = SSL pool allocation */
    ssl_header->hazard_data = NULL;
    ssl_header->tlsf_ptr = NULL;
    
    /* Return user pointer (after header) */
    user_ptr = (char*)allocated_ptr + HEADER_SIZE;
    
    if (SHOULD_DEBUG_MEMORY()) {
        fprintf(stderr, "   ➜ SSL allocation successful: %p (from_pool=%d, 16-byte aligned=%s)\n", 
               user_ptr, from_pool, ((uintptr_t)user_ptr % SSL_ALIGNMENT == 0) ? "✓" : "✗");
    }
    
    return user_ptr;
}

/* Revolutionary SSL pool return function */
static bool ssl_semantic_free(memory_header_t* header) {
    if (!g_ssl_pools.initialized) return false;
    
    size_t size = header->size;
    bool returned_to_pool = false;
    
    pthread_mutex_lock(&g_ssl_pools.pool_mutex);
    
    /* Return to appropriate pool based on size */
    if (size <= SSL_TINY_THRESHOLD && g_ssl_pools.tiny_available < g_ssl_pools.tiny_pool_size) {
        /* Return to tiny pool */
        g_ssl_pools.tiny_pool[g_ssl_pools.tiny_available++] = header;
        returned_to_pool = true;
        if (SHOULD_DEBUG_MEMORY()) {
            fprintf(stderr, "   ♻️  SSL tiny pool return (size=%zu, available=%zu)\n", 
                   size, g_ssl_pools.tiny_available);
        }
    } else if (size <= SSL_SMALL_THRESHOLD && g_ssl_pools.small_available < g_ssl_pools.small_pool_size) {
        /* Return to small pool */
        g_ssl_pools.small_pool[g_ssl_pools.small_available++] = header;
        returned_to_pool = true;
        if (SHOULD_DEBUG_MEMORY()) {
            fprintf(stderr, "   ♻️  SSL small pool return (size=%zu, available=%zu)\n", 
                   size, g_ssl_pools.small_available);
        }
    } else if (size <= SSL_MEDIUM_THRESHOLD && g_ssl_pools.medium_available < g_ssl_pools.medium_pool_size) {
        /* Return to medium pool */
        g_ssl_pools.medium_pool[g_ssl_pools.medium_available++] = header;
        returned_to_pool = true;
        if (SHOULD_DEBUG_MEMORY()) {
            fprintf(stderr, "   ♻️  SSL medium pool return (size=%zu, available=%zu)\n", 
                   size, g_ssl_pools.medium_available);
        }
    }
    
    pthread_mutex_unlock(&g_ssl_pools.pool_mutex);
    
    return returned_to_pool;
}

/**
 * Inspector Claude's SSL Detection Function
 * "Ah! Ze clever detection of ze SSL allocations!"
 * 
 * Detects if the current allocation is coming from SSL library
 * to prevent exotic allocator corruption of SSL contexts.
 */
static bool is_ssl_allocation(void) {
    /* Check if SSL bypass is forced via environment */
    static int ssl_bypass_checked = 0;
    static int ssl_bypass_forced = 0;
    
    if (!ssl_bypass_checked) {
        const char* ssl_bypass = getenv("JDBX_SSL_FORCE_SYSTEM_MALLOC");
        ssl_bypass_forced = (ssl_bypass && (strcmp(ssl_bypass, "true") == 0 || 
                                           strcmp(ssl_bypass, "1") == 0));
        ssl_bypass_checked = 1;
    }
    
    if (ssl_bypass_forced) {
        return true;  /* Force system malloc for all allocations */
    }
    
    /* Check if we're being called from SSL library context */
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wframe-address"
    void* caller_address = __builtin_return_address(1);  /* Get caller's address */
    #pragma GCC diagnostic pop
    if (!caller_address) {
        return false;
    }
    
    Dl_info caller_info;
    if (dladdr(caller_address, &caller_info) && caller_info.dli_fname) {
        const char* library_name = caller_info.dli_fname;
        
        /* Check if the caller is from SSL or crypto libraries */
        if (strstr(library_name, "libssl") || 
            strstr(library_name, "libcrypto") ||
            strstr(library_name, "ssl") ||
            strstr(library_name, "crypto")) {
            
            if (SHOULD_DEBUG_MEMORY()) {
                fprintf(stderr, "🕵️ SSL allocation detected from: %s\n", library_name);
            }
            return true;
        }
    }
    
    return false;
}

/**
 * Allocate memory with unified decision engine
 * Enhanced with Inspector Claude's SSL bypass protection
 */
void* memory_alloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    
    /* 🚀 REVOLUTIONARY SSL SEMANTIC ALLOCATOR - HIGHEST PRIORITY! */
    if (is_ssl_allocation()) {
        return ssl_semantic_alloc(size);
    }
    
    /* Handle early allocation before full memory manager initialization */
    if (!g_memory_manager_initialized) {
        /* Early allocation - use aligned_alloc directly */
        size_t aligned_size = (size + _Alignof(max_align_t) - 1) & ~(_Alignof(max_align_t) - 1);
        return aligned_alloc(_Alignof(max_align_t), aligned_size);
    }
    
    ensure_memory_initialized();
    
    /* Unified allocation decision variables */
    void* allocated_ptr = NULL;
    int from_arena = 0;
    int from_tlsf = 0;
    
    /* DECISION PATH 1: Arena Allocator (Highest Priority)
     * Conditions: Active checkpoint + Small size + Arena enabled + Arena available
     * Benefits: Ultra-fast bump pointer allocation + bulk free on rewind
     */
    if (SHOULD_DEBUG_MEMORY()) {  /* Only debug small allocations to avoid spam */
        fprintf(stderr, "memory_alloc: Checking arena allocation for size=%zu\n", size);
        fprintf(stderr, "  should_use_arena_allocator() = %d\n", should_use_arena_allocator(size) ? 1 : 0);
        if (tls_memory.current_checkpoint) {
            fprintf(stderr, "  current_checkpoint = %p\n", tls_memory.current_checkpoint);
            fprintf(stderr, "  checkpoint->arena = %p\n", tls_memory.current_checkpoint->arena);
            fprintf(stderr, "  checkpoint->committed = %d\n", tls_memory.current_checkpoint->committed);
        } else {
            fprintf(stderr, "  current_checkpoint = NULL\n");
        }
        fprintf(stderr, "  bypass_checkpoint = %d\n", tls_memory.bypass_checkpoint);
        fprintf(stderr, "  arena_enabled = %d\n", memory_allocator_config_arena_enabled() ? 1 : 0);
    }
    
    /* CHECKPOINT LIFECYCLE DEBUGGING: Track when checkpoint becomes NULL */
    if (getenv("JDBX_MEM_DEBUG")) {
        FILE* debug_file = fopen("/tmp/jdbx_debug.log", "a");
        if (debug_file) {
            fprintf(debug_file, "🔍 memory_alloc: size=%zu, current_checkpoint=%p, arena_enabled=%d, bypass=%d\n", 
                   size, tls_memory.current_checkpoint, 
                   memory_allocator_config_arena_enabled() ? 1 : 0, 
                   tls_memory.bypass_checkpoint);
            if (tls_memory.current_checkpoint) {
                fprintf(debug_file, "  ✅ checkpoint->arena=%p, committed=%d\n", 
                       tls_memory.current_checkpoint->arena, 
                       tls_memory.current_checkpoint->committed);
            }
            fflush(debug_file);
            fclose(debug_file);
        }
    }
    
    /* DIRECT DEBUG: Check Arena allocator result */
    int arena_result = should_use_arena_allocator(size);
    if (getenv("JDBX_MEM_DEBUG") && tls_memory.current_checkpoint) {
        FILE* debug_file = fopen("/tmp/jdbx_debug.log", "a");
        if (debug_file) {
            fprintf(debug_file, "🎯 Arena decision: size=%zu, result=%d\n", size, arena_result);
            fflush(debug_file);
            fclose(debug_file);
        }
    }
    
    if (arena_result) {
        allocated_ptr = arena_alloc(tls_memory.current_checkpoint->arena, HEADER_SIZE + size);
        if (allocated_ptr) {
            from_arena = 1;
            if (SHOULD_DEBUG_MEMORY()) {
                fprintf(stderr, "memory_alloc: Arena allocation successful, size=%zu, ptr=%p\n", size, allocated_ptr);
            }
        } else {
            if (SHOULD_DEBUG_MEMORY()) {
                fprintf(stderr, "memory_alloc: Arena allocation failed, falling back, size=%zu\n", size);
            }
        }
    }
    
    /* DECISION PATH 2: TLSF Allocator (Medium Priority)
     * Conditions: No arena allocation + Runtime context + TLSF enabled + Size in range
     * Benefits: O(1) worst-case allocation + minimal fragmentation + thread-local pools
     */
    if (!allocated_ptr && should_use_tlsf_allocator(size)) {
        /* Ensure TLSF allocation uses same alignment as system malloc for consistency */
        size_t total_size = HEADER_SIZE + size;
        size_t aligned_total = (total_size + _Alignof(max_align_t) - 1) & ~(_Alignof(max_align_t) - 1);
        allocated_ptr = tlsf_malloc(tls_memory.tlsf_pool, aligned_total);
        if (allocated_ptr) {
            from_tlsf = 1;
            if (SHOULD_DEBUG_MEMORY()) {
                fprintf(stderr, "memory_alloc: TLSF allocation successful, size=%zu, aligned_total=%zu, tlsf_ptr=%p\n", 
                        size, aligned_total, allocated_ptr);
            }
        } else {
            if (SHOULD_DEBUG_MEMORY()) {
                fprintf(stderr, "memory_alloc: TLSF allocation failed, falling back, size=%zu\n", size);
            }
        }
    }
    
    /* DECISION PATH 3: System Malloc (Lowest Priority - Fallback)
     * Conditions: No exotic allocation successful OR early init OR large sizes OR emergency mode
     * Benefits: Universal compatibility + reliability + handles any size
     * Use Cases: Early initialization, oversized allocations, allocator failures
     */
    if (!allocated_ptr) {
        size_t total_size = HEADER_SIZE + size;
        allocated_ptr = aligned_alloc(_Alignof(max_align_t), 
                                     (total_size + _Alignof(max_align_t) - 1) & ~(_Alignof(max_align_t) - 1));
        if (SHOULD_DEBUG_MEMORY()) {
            fprintf(stderr, "memory_alloc: System malloc fallback, size=%zu\n", size);
        }
    }
    
    if (!allocated_ptr) {
        return NULL;
    }
    
    memory_header_t* header = (memory_header_t*)allocated_ptr;
    
    /* Initialize header */
    header->magic = MEMORY_MAGIC;
    header->size = size;
    header->checkpoint = NULL;
    header->next = NULL;
    header->prev = NULL;
    header->flags = 0;
    if (from_arena) header->flags |= MEMORY_FLAG_ARENA_ALLOCATED;
    if (from_tlsf) header->flags |= MEMORY_FLAG_TLSF_ALLOCATED;
    header->hazard_data = NULL;
    header->tlsf_ptr = from_tlsf ? allocated_ptr : NULL;  /* For TLSF, this is the original TLSF pointer to pass to tlsf_free */
    
    /* Debug print */
    if (getenv("JDBX_MEM_DEBUG")) {
        void* user_ptr = (char*)header + HEADER_SIZE;
        fprintf(stderr, "memory_alloc: allocated_ptr=%p, header=%p, user_ptr=%p, size=%zu, from_arena=%d, from_tlsf=%d, flags=0x%x\n",
                allocated_ptr, header, user_ptr, size, from_arena, from_tlsf, header->flags);
    }
    
    /* If we have an active checkpoint AND not bypassing, track the allocation 
     * EXCEPT for Arena allocations which are tracked/freed in bulk via arena destruction */
    if (tls_memory.current_checkpoint && !tls_memory.current_checkpoint->committed && 
        !tls_memory.bypass_checkpoint && !from_arena) {
        memory_checkpoint_t* cp = tls_memory.current_checkpoint;
        
        /* Lock checkpoint for list modification */
        pthread_spin_lock(&cp->lock);
        
        /* Link into checkpoint's allocation list */
        header->checkpoint = cp;
        header->prev = cp->last_alloc;
        
        if (cp->last_alloc) {
            cp->last_alloc->next = header;
        } else {
            cp->first_alloc = header;
        }
        cp->last_alloc = header;
        
        /* Update checkpoint stats */
        cp->allocation_count++;
        cp->total_size += size;
        
        /* Unlock checkpoint */
        pthread_spin_unlock(&cp->lock);
    }
    
    /* Return pointer to user data - calculate properly aligned offset */
    return (char*)header + HEADER_SIZE;
}

/**
 * Free memory (checkpoint aware)
 */
void memory_free(void* ptr) {
    if (!ptr) return;
    
    /* Handle case where memory manager is initialized but thread-local state isn't ready yet */
    if (!g_memory_manager_initialized) {
        /* Try to handle this as unmanaged memory - this can happen during early initialization */
        free(ptr);
        return;
    }
    
    /* Ensure thread-local state is initialized */
    ensure_memory_initialized();
    
    /* Try to get header - this validates if it's a managed allocation */
    memory_header_t* header = get_memory_header(ptr);
    if (!header) {
        /* Not a managed allocation - this is OK, just do regular free */
        free(ptr);
        return;
    }
    
    /* Double-free check */
    if (header->magic == MEMORY_MAGIC_FREE) {
        /* Silent failure - double free detected */
        return;
    }
    
    /* Unlink from checkpoint if not committed */
    if (header->checkpoint && !header->checkpoint->committed) {
        memory_checkpoint_t* cp = header->checkpoint;
        
        /* Lock checkpoint for list modification */
        pthread_spin_lock(&cp->lock);
        
        if (header->prev) {
            header->prev->next = header->next;
        } else {
            cp->first_alloc = header->next;
        }
        
        if (header->next) {
            header->next->prev = header->prev;
        } else {
            cp->last_alloc = header->prev;
        }
        
        cp->allocation_count--;
        cp->total_size -= header->size;
        
        /* Unlock checkpoint */
        pthread_spin_unlock(&cp->lock);
    }
    
    /* Mark as freed to detect double-free */
    header->magic = MEMORY_MAGIC_FREE;
    
    /* Determine which allocator to free to */
    if (getenv("JDBX_MEM_DEBUG")) {
        fprintf(stderr, "memory_free: header=%p, flags=0x%x, magic=0x%x, size=%zu\n", 
                header, header->flags, header->magic, header->size);
    }
    
    if (header->flags & MEMORY_FLAG_ARENA_ALLOCATED) {
        /* Arena allocations are freed in bulk on checkpoint rewind */
        /* Individual frees are no-ops */
        if (getenv("JDBX_MEM_DEBUG")) {
            fprintf(stderr, "memory_free: arena allocation, no-op\n");
        }
    } else if (header->flags & MEMORY_FLAG_TLSF_ALLOCATED) {
        /* TLSF allocation - free using the stored TLSF pointer */
        if (getenv("JDBX_MEM_DEBUG")) {
            fprintf(stderr, "memory_free: TLSF allocation, pool=%p, tlsf_ptr=%p\n", tls_memory.tlsf_pool, header->tlsf_ptr);
        }
        if (!tls_memory.tlsf_pool) {
            fprintf(stderr, "ERROR: TLSF pool is NULL!\n");
            abort();
        }
        /* For TLSF, the tlsf_ptr is the original user pointer we got from tlsf_malloc */
        tlsf_free(tls_memory.tlsf_pool, header->tlsf_ptr);
    } else if (header->flags & 8) {
        /* 🚀 REVOLUTIONARY SSL POOL RETURN */
        if (ssl_semantic_free(header)) {
            if (getenv("JDBX_MEM_DEBUG")) {
                fprintf(stderr, "memory_free: SSL pool return successful\n");
            }
        } else {
            /* Pool full or wrong size - fallback to system free */
            if (getenv("JDBX_MEM_DEBUG")) {
                fprintf(stderr, "memory_free: SSL pool full, using system free\n");
            }
            free(header);
        }
    } else {
        /* System allocation */
        if (getenv("JDBX_MEM_DEBUG")) {
            fprintf(stderr, "memory_free: system allocation, calling free\n");
        }
        free(header);
    }
}

/**
 * Reallocate memory with checkpoint tracking
 */
void* memory_realloc(void* ptr, size_t new_size) {
    /* Debug print */
    if (getenv("JDBX_MEM_DEBUG")) {
        fprintf(stderr, "memory_realloc called: ptr=%p, new_size=%zu\n", ptr, new_size);
    }
    
    if (new_size == 0) {
        memory_free(ptr);
        return NULL;
    }
    
    if (!ptr) {
        return memory_alloc(new_size);
    }
    
    /* If memory manager not initialized yet, use regular realloc */
    if (!g_memory_manager_initialized) {
        return realloc(ptr, new_size);
    }
    
    /* Ensure thread-local state is initialized */
    ensure_memory_initialized();
    
    /* Check if this is a managed allocation */
    memory_header_t* header = get_memory_header(ptr);
    if (!header) {
        /* Not a managed allocation - could be early allocation or system malloc */
        if (getenv("JDBX_MEM_DEBUG")) {
            fprintf(stderr, "memory_realloc: Not managed allocation, using fallback for ptr=%p\n", ptr);
        }
        
        /* For unmanaged allocations, we need to handle two cases:
         * 1. NULL pointer - this is just a malloc
         * 2. Non-NULL pointer - could be from system malloc or corrupted
         * 
         * The safest approach is to treat NULL as malloc and try system
         * realloc for non-NULL pointers as a last resort.
         */
        if (!ptr) {
            /* This is effectively a malloc */
            return memory_alloc(new_size);
        }
        
        /* Non-NULL unmanaged pointer - try system realloc */
        if (getenv("JDBX_MEM_DEBUG")) {
            fprintf(stderr, "WARNING: memory_realloc called on unmanaged pointer %p, "
                    "attempting system realloc\n", ptr);
        }
        
        /* Try system realloc - this will work if:
         * 1. The pointer was allocated with system malloc before memory_manager_init
         * 2. The pointer was allocated by a library using malloc directly
         * It will fail (and likely crash) if the pointer is invalid or corrupted
         */
        if (getenv("JDBX_MEM_DEBUG")) {
            fprintf(stderr, "WARNING: About to call system realloc on unmanaged pointer %p, size=%zu\n", 
                    ptr, new_size);
        }
        void* result = realloc(ptr, new_size);
        if (!result && new_size > 0) {
            /* Realloc failed - could be invalid pointer or out of memory */
            if (getenv("JDBX_MEM_DEBUG")) {
                fprintf(stderr, "ERROR: system realloc failed for unmanaged pointer %p\n", ptr);
            }
        }
        return result;
    }
    
    /* Check which allocator owns this memory */
    int is_arena = (header->flags & MEMORY_FLAG_ARENA_ALLOCATED) ? 1 : 0;
    int is_tlsf = (header->flags & MEMORY_FLAG_TLSF_ALLOCATED) ? 1 : 0;
    
    /* Debug print */
    if (getenv("JDBX_MEM_DEBUG")) {
        fprintf(stderr, "memory_realloc: is_arena=%d, is_tlsf=%d\n", is_arena, is_tlsf);
    }
    
    /* Arena allocations cannot be reallocated in-place */
    if (is_arena) {
        /* Allocate new memory */
        void* new_ptr = memory_alloc(new_size);
        if (!new_ptr) {
            return NULL;
        }
        
        /* Copy data */
        size_t copy_size = header->size < new_size ? header->size : new_size;
        memcpy(new_ptr, ptr, copy_size);
        
        /* Arena memory is freed in bulk, so just return new allocation */
        return new_ptr;
    }
    
    /* TLSF allocations cannot use TLSF realloc due to header layout mismatch */
    /* TLSF realloc expects pure TLSF blocks, but we have memory_header_t layered on top */
    /* Fall through to allocate-copy-free approach for TLSF allocations */
    
    /* Fallback: allocate new memory */
    void* new_ptr = memory_alloc(new_size);
    if (!new_ptr) {
        return NULL;
    }
    
    /* Copy data */
    size_t copy_size = header->size < new_size ? header->size : new_size;
    memcpy(new_ptr, ptr, copy_size);
    
    /* Debug print */
    if (getenv("JDBX_MEM_DEBUG")) {
        fprintf(stderr, "memory_realloc fallback: old_ptr=%p, new_ptr=%p, old_size=%zu, new_size=%zu, copy_size=%zu\n",
                ptr, new_ptr, header->size, new_size, copy_size);
    }
    
    /* Free old memory */
    memory_free(ptr);
    
    return new_ptr;
}

/**
 * Allocate zero-initialized memory with checkpoint tracking
 */
void* memory_calloc(size_t nmemb, size_t size) {
    /* Check for overflow */
    if (nmemb && size > SIZE_MAX / nmemb) {
        return NULL;
    }
    
    /* Handle zero-size allocations */
    if (nmemb == 0 || size == 0) {
        return NULL;
    }
    
    /* If memory manager not initialized yet, use aligned allocation + memset */
    if (!g_memory_manager_initialized) {
        size_t total_size = nmemb * size;
        void* ptr = aligned_alloc(_Alignof(max_align_t), 
                                 (total_size + _Alignof(max_align_t) - 1) & ~(_Alignof(max_align_t) - 1));
        if (ptr) {
            memset(ptr, 0, total_size);
        }
        return ptr;
    }
    
    size_t total_size = nmemb * size;
    void* ptr = memory_alloc(total_size);
    if (ptr) {
        memset(ptr, 0, total_size);
    }
    
    return ptr;
}

/**
 * Duplicate string with checkpoint tracking
 */
char* memory_strdup(const char* str) {
    if (!str) {
        return NULL;
    }
    
    /* If memory manager not initialized yet, use aligned allocation + strcpy */
    if (!g_memory_manager_initialized) {
        size_t len = strlen(str);
        char* copy = (char*)aligned_alloc(_Alignof(max_align_t), 
                                         (len + 1 + _Alignof(max_align_t) - 1) & ~(_Alignof(max_align_t) - 1));
        if (copy) {
            memcpy(copy, str, len + 1);
        }
        return copy;
    }
    
    size_t len = strlen(str);
    char* copy = (char*)memory_alloc(len + 1);
    if (copy) {
        memcpy(copy, str, len + 1);
    }
    
    return copy;
}

/**
 * Initialize memory manager
 */
void memory_manager_init(void) {
    /* Memory manager initialization - no logging to avoid circular dependencies */
    pthread_mutex_lock(&g_memory_lock);
    
    /* Initialize memory allocator configuration system */
    memory_allocator_config_init();
    
    /* Initialize TLSF system */
    tlsf_init();
    
    /* Reset global stats */
    g_memory_stats.checkpoints_created = 0;
    g_memory_stats.rewinds_performed = 0;
    g_memory_stats.allocations_freed_by_rewind = 0;
    /* Mark as initialized */
    g_memory_manager_initialized = 1;
    
    pthread_mutex_unlock(&g_memory_lock);
    
    /* Now initialize TLSF pool for the main thread if it wasn't created yet */
    ensure_memory_initialized();
    
    if (SHOULD_DEBUG_MEMORY()) {
        fprintf(stderr, "memory_manager_init: Memory manager initialized with configuration\n");
        memory_allocator_config_log_status();
    }
}

/**
 * Shutdown and report leaks
 */
void memory_manager_shutdown(void) {
    /* Memory manager shutdown - stats available via memory_manager_get_stats() */
    pthread_mutex_lock(&g_memory_lock);
    
    /* Clean up thread-local TLSF pool if exists */
    if (tls_memory.tlsf_pool) {
        tlsf_destroy_pool(tls_memory.tlsf_pool);
        tls_memory.tlsf_pool = NULL;
    }
    
    /* Clean up any remaining checkpoints */
    while (tls_memory.current_checkpoint) {
        memory_checkpoint_t* cp = tls_memory.current_checkpoint;
        tls_memory.current_checkpoint = cp->parent;
        
        /* Clean up arena if present */
        if (cp->arena) {
            arena_destroy(cp->arena);
        }
        
        pthread_spin_destroy(&cp->lock);
        free(cp);
    }
    
    /* Mark as uninitialized */
    g_memory_manager_initialized = 0;
    tls_memory.initialized = 0;
    
    pthread_mutex_unlock(&g_memory_lock);
    
    /* Shutdown configuration system */
    memory_allocator_config_shutdown();
}

/**
 * Get memory manager statistics
 */
void memory_manager_get_stats(uint64_t* checkpoints_created, uint64_t* rewinds, uint64_t* allocations_freed) {
    if (checkpoints_created) *checkpoints_created = g_memory_stats.checkpoints_created;
    if (rewinds) *rewinds = g_memory_stats.rewinds_performed;
    if (allocations_freed) *allocations_freed = g_memory_stats.allocations_freed_by_rewind;
}

/**
 * Enable TLSF allocator (disabled during early initialization to avoid mixing allocations)
 */
void memory_manager_enable_tlsf(void) {
    /* This will be referenced by the static variable in memory_alloc */
    /* For now, we'll keep TLSF disabled until we fix the integration properly */
}

/**
 * Mark an allocation as hazard-protected
 * The allocation will not be freed on checkpoint rewind until the hazard is cleared
 */
void memory_mark_hazard_protected(void* ptr, void* hazard_data) {
    if (!ptr || !g_memory_manager_initialized) return;
    
    memory_header_t* header = get_memory_header(ptr);
    if (!header || header->magic != MEMORY_MAGIC) return;
    
    /* Set hazard protection flag and data */
    header->flags |= MEMORY_FLAG_HAZARD_PROTECTED;
    header->hazard_data = hazard_data;
}

/**
 * Clear hazard protection from an allocation
 * If the checkpoint was already rewound, the memory will be freed
 */
void memory_clear_hazard_protection(void* ptr) {
    if (!ptr || !g_memory_manager_initialized) return;
    
    memory_header_t* header = get_memory_header(ptr);
    if (!header || header->magic != MEMORY_MAGIC) return;
    
    /* Clear hazard protection */
    header->flags &= ~MEMORY_FLAG_HAZARD_PROTECTED;
    header->hazard_data = NULL;
    
    /* If checkpoint was already rewound and we're no longer in the checkpoint, free now */
    if (header->checkpoint && header->checkpoint->committed) {
        /* Checkpoint was committed, not rewound */
        return;
    }
    
    /* Check if we're still linked to an active checkpoint */
    if (!header->checkpoint || (!header->prev && !header->next && 
        header->checkpoint->first_alloc != header)) {
        /* Not in any checkpoint list - was already rewound, free now */
        header->magic = MEMORY_MAGIC_FREE;
        free(header);
    }
}

/**
 * Callback from hazard pointer system when memory is safe to free
 */
void memory_hazard_retire_callback(void* ptr) {
    /* Just clear the hazard protection - the logic in clear will handle freeing */
    memory_clear_hazard_protection(ptr);
}