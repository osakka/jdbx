/**
 * @file memory_manager.c
 * @brief JDBX Memory Manager - Checkpoint-Based Memory Management
 * 
 * Revolutionary memory management that eliminates manual cleanup code.
 * Uses checkpoint/rewind architecture for automatic memory cleanup on error paths.
 * Single source of truth for advanced memory management in JDBX.
 */

#define _GNU_SOURCE
#include "utils/memory_manager.h"
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
#include <pthread.h>

/* Memory managers should not depend on logging */

/* Memory allocation flags */
#define MEMORY_FLAG_HAZARD_PROTECTED 0x00000001  /* Protected by hazard pointers */
#define MEMORY_FLAG_ARENA_ALLOCATED  0x00000002  /* Allocated from arena */

/* Memory allocation header for tracking */
typedef struct memory_header {
    struct memory_header* next;      /* Next allocation in checkpoint */
    struct memory_header* prev;      /* Previous allocation in checkpoint */
    memory_checkpoint_t* checkpoint; /* Owning checkpoint */
    size_t size;                    /* Allocation size */
    uint32_t magic;                 /* Magic number for corruption detection */
    uint32_t flags;                 /* Memory flags (hazard protected, etc) */
    void* hazard_data;              /* Hazard pointer data if protected */
    /* User data follows immediately after this header with proper alignment */
} memory_header_t;

#define MEMORY_MAGIC 0xDEADBEEF
#define MEMORY_MAGIC_FREE 0xFEEDF00D
/* Calculate header size with proper alignment for max_align_t */
#define HEADER_SIZE ((sizeof(memory_header_t) + _Alignof(max_align_t) - 1) & ~(_Alignof(max_align_t) - 1))

/* Checkpoint structure */
struct memory_checkpoint {
    pthread_spinlock_t lock;          /* Spinlock for thread-safe list operations */
    struct memory_checkpoint* parent;  /* Parent checkpoint for nesting */
    memory_header_t* first_alloc;     /* First allocation in this checkpoint */
    memory_header_t* last_alloc;      /* Last allocation for O(1) append */
    time_t created_at;                /* Creation timestamp */
    uint64_t allocation_count;        /* Number of allocations */
    uint64_t total_size;              /* Total allocated size */
    int committed;                    /* Whether checkpoint is committed */
    arena_t* arena;                   /* Arena for checkpoint-scoped allocations */
    uint32_t arena_checkpoint_id;     /* Arena checkpoint ID for bulk free */
};

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
        
        /* Initialize TLSF pool for this thread (32MB) */
        if (!tls_memory.tlsf_pool) {
            tls_memory.tlsf_pool = tlsf_create_pool(32 * 1024 * 1024);
        }
        /* Don't log from here - can cause issues */
        /* But we can use direct printf for debugging */
        /* printf("Memory manager: Initialized TLS for thread %p\n", (void*)pthread_self()); */
    }
}

/**
 * Get memory header from user pointer
 */
static memory_header_t* get_memory_header(void* ptr) {
    if (!ptr) return NULL;
    
    /* Calculate the header pointer - user data starts at HEADER_SIZE offset */
    memory_header_t* header = (memory_header_t*)((char*)ptr - HEADER_SIZE);
    
    /* Sanity check for obviously bad pointers */
    if ((uintptr_t)ptr < HEADER_SIZE) {
        return NULL;
    }
    
    /* Safely check if this could be a valid header by checking alignment */
    /* Headers are allocated by aligned_alloc, so they should be aligned */
    if ((uintptr_t)header % _Alignof(max_align_t) != 0) {
        return NULL;
    }
    
    /* Check if the magic field location is readable before accessing it */
    /* This is a heuristic - we're checking if the pointer makes sense */
    if ((uintptr_t)header < 0x1000) {  /* Likely not a valid heap address */
        return NULL;
    }
    
    /* Validate magic number with memory barrier for thread safety */
    __sync_synchronize();  /* Memory fence */
    
    uint32_t magic = header->magic;
    
    /* Debug print for troubleshooting */
    if (getenv("JDBX_MEM_DEBUG")) {
        fprintf(stderr, "get_memory_header: ptr=%p, header=%p, magic=0x%x (expected 0x%x)\n", 
                ptr, header, magic, MEMORY_MAGIC);
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
    
    /* Create arena for checkpoint allocations (4MB) */
    checkpoint->arena = arena_create(4 * 1024 * 1024);
    checkpoint->arena_checkpoint_id = 0;
    if (checkpoint->arena) {
        checkpoint->arena_checkpoint_id = arena_checkpoint(checkpoint->arena);
    }
    
    /* Make it current */
    tls_memory.current_checkpoint = checkpoint;
    
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
                    
                    /* Check if TLSF allocated - DISABLED FOR NOW */
                    if (0 && tls_memory.tlsf_pool && tlsf_block_size(header) > 0) {
                        tlsf_free(tls_memory.tlsf_pool, header);
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
        
        /* Reset arena if present */
        if (cp->arena && cp->arena_checkpoint_id) {
            arena_reset_to_checkpoint(cp->arena, cp->arena_checkpoint_id);
        }
        
        /* Move to parent and free the checkpoint */
        memory_checkpoint_t* parent = cp->parent;
        pthread_spin_destroy(&cp->lock);  /* Cleanup spinlock */
        
        /* Arena is already reset, now destroy it */
        if (cp->arena) {
            arena_destroy(cp->arena);
            cp->arena = NULL;
        }
        
        free(cp);
        cp = parent;
    }
    
    /* Update current checkpoint */
    tls_memory.current_checkpoint = checkpoint;
    
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
                
                /* Check if TLSF allocated - DISABLED FOR NOW */
                if (0 && tls_memory.tlsf_pool && tlsf_block_size(header) > 0) {
                    tlsf_free(tls_memory.tlsf_pool, header);
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
        tls_memory.current_checkpoint = checkpoint->parent;
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

/**
 * Allocate memory with checkpoint tracking
 */
void* memory_alloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    
    /* SINGLE SOURCE OF TRUTH: Memory manager must be initialized before ANY allocation */
    if (!g_memory_manager_initialized) {
        fprintf(stderr, "FATAL: memory_alloc called before memory_manager_init()\n");
        fprintf(stderr, "This violates single source of truth - all allocations must go through initialized manager\n");
        abort();
    }
    
    ensure_memory_initialized();
    
    /* Decide which allocator to use */
    void* allocated_ptr = NULL;
    int from_arena = 0;
    
    /* Use arena for checkpoint allocations under 64KB - DISABLED FOR NOW */
    if (0 && tls_memory.current_checkpoint && !tls_memory.current_checkpoint->committed && 
        !tls_memory.bypass_checkpoint && size < 65536 && tls_memory.current_checkpoint->arena) {
        allocated_ptr = arena_alloc(tls_memory.current_checkpoint->arena, HEADER_SIZE + size);
        from_arena = 1;
    }
    
    /* Use TLSF for non-arena allocations - DISABLED FOR NOW */
    if (0 && !allocated_ptr && tls_memory.tlsf_pool) {
        allocated_ptr = tlsf_malloc(tls_memory.tlsf_pool, HEADER_SIZE + size);
    }
    
    /* Fallback to system malloc */
    if (!allocated_ptr) {
        size_t total_size = HEADER_SIZE + size;
        allocated_ptr = aligned_alloc(_Alignof(max_align_t), 
                                     (total_size + _Alignof(max_align_t) - 1) & ~(_Alignof(max_align_t) - 1));
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
    header->flags = from_arena ? MEMORY_FLAG_ARENA_ALLOCATED : 0;
    header->hazard_data = NULL;
    
    /* Debug print */
    if (getenv("JDBX_MEM_DEBUG")) {
        void* user_ptr = (char*)header + HEADER_SIZE;
        fprintf(stderr, "memory_alloc: allocated_ptr=%p, header=%p, user_ptr=%p, size=%zu, from_arena=%d\n",
                allocated_ptr, header, user_ptr, size, from_arena);
    }
    
    /* If we have an active checkpoint AND not bypassing, track the allocation */
    if (tls_memory.current_checkpoint && !tls_memory.current_checkpoint->committed && 
        !tls_memory.bypass_checkpoint) {
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
    
    /* SINGLE SOURCE OF TRUTH: Memory manager must be initialized before ANY free */
    if (!g_memory_manager_initialized) {
        fprintf(stderr, "FATAL: memory_free called before memory_manager_init()\n");
        fprintf(stderr, "This violates single source of truth - all frees must go through initialized manager\n");
        abort();
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
    if (header->flags & MEMORY_FLAG_ARENA_ALLOCATED) {
        /* Arena allocations are freed in bulk on checkpoint rewind */
        /* Individual frees are no-ops */
    } else if (0 && tls_memory.tlsf_pool && tlsf_block_size(header) > 0) {
        /* TLSF allocation - DISABLED FOR NOW */
        tlsf_free(tls_memory.tlsf_pool, header);
    } else {
        /* System allocation */
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
        /* Not a managed allocation - use regular realloc */
        return realloc(ptr, new_size);
    }
    
    /* Check which allocator owns this memory */
    int is_arena = (header->flags & MEMORY_FLAG_ARENA_ALLOCATED) ? 1 : 0;
    int is_tlsf = 0;
    
    if (0 && !is_arena && tls_memory.tlsf_pool) {
        /* TLSF realloc checking - DISABLED FOR NOW */
        size_t block_size = tlsf_block_size(header);
        if (getenv("JDBX_MEM_DEBUG")) {
            fprintf(stderr, "tlsf_block_size returned: %zu for header=%p\n", block_size, header);
        }
        if (block_size > 0) {
            is_tlsf = 1;
        }
    }
    
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
    
    /* For now, use fallback for TLSF allocations too - proper integration needs more work */
    if (0 && is_tlsf) {
        /* DISABLED: TLSF realloc integration needs to handle our header properly */
    }
    
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
    /* Memory manager initialization - no logging */
    pthread_mutex_lock(&g_memory_lock);
    
    /* Initialize TLSF system */
    tlsf_init();
    
    /* Reset global stats */
    g_memory_stats.checkpoints_created = 0;
    g_memory_stats.rewinds_performed = 0;
    g_memory_stats.allocations_freed_by_rewind = 0;
    /* Mark as initialized */
    g_memory_manager_initialized = 1;
    pthread_mutex_unlock(&g_memory_lock);
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