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

/* Memory allocation header for tracking */
typedef struct memory_header {
    struct memory_header* next;      /* Next allocation in checkpoint */
    struct memory_header* prev;      /* Previous allocation in checkpoint */
    memory_checkpoint_t* checkpoint; /* Owning checkpoint */
    size_t size;                    /* Allocation size */
    uint32_t magic;                 /* Magic number for corruption detection */
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
};

/* Thread-local memory state */
typedef struct {
    memory_checkpoint_t* current_checkpoint;  /* Active checkpoint */
    int initialized;                          /* Whether initialized */
    int bypass_checkpoint;                    /* Skip checkpoint for this allocation */
} memory_state_t;

/* Thread-local storage for memory state */
static __thread memory_state_t tls_memory = {NULL, 0, 0};

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
    
    /* Additional validation: check if header is in valid memory range */
    /* This prevents reading arbitrary memory for magic validation */
    if ((uintptr_t)header & (_Alignof(max_align_t) - 1)) {
        return NULL;  /* Not properly aligned for our headers */
    }
    
    uint32_t magic = header->magic;
    
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
        while (header) {
            memory_header_t* next = header->next;
            header->magic = MEMORY_MAGIC_FREE;  /* Mark as freed for detection */
            free(header);
            freed_count++;
            header = next;
        }
        
        /* Clear the list pointers */
        cp->first_alloc = NULL;
        cp->last_alloc = NULL;
        
        /* Unlock before destroying */
        pthread_spin_unlock(&cp->lock);
        
        /* Move to parent and free the checkpoint */
        memory_checkpoint_t* parent = cp->parent;
        pthread_spin_destroy(&cp->lock);  /* Cleanup spinlock */
        free(cp);
        cp = parent;
    }
    
    /* Update current checkpoint */
    tls_memory.current_checkpoint = checkpoint;
    
    /* Free the target checkpoint's allocations too */
    pthread_spin_lock(&checkpoint->lock);
    
    memory_header_t* header = checkpoint->first_alloc;
    while (header) {
        memory_header_t* next = header->next;
        header->magic = 0;
        free(header);
        freed_count++;
        header = next;
    }
    
    /* Reset checkpoint */
    checkpoint->first_alloc = NULL;
    checkpoint->last_alloc = NULL;
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
        header->checkpoint = NULL;
        header = header->next;
    }
    
    /* Mark as committed */
    checkpoint->committed = 1;
    
    /* Unlock before potential destruction */
    pthread_spin_unlock(&checkpoint->lock);
    
    /* If this is the current checkpoint, clear it */
    if (tls_memory.current_checkpoint == checkpoint) {
        tls_memory.current_checkpoint = checkpoint->parent;
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
    
    /* Always allocate with header for consistency */
    /* Ensure total allocation is aligned to max_align_t */
    size_t total_size = HEADER_SIZE + size;
    memory_header_t* header = (memory_header_t*)aligned_alloc(_Alignof(max_align_t), 
                                                              (total_size + _Alignof(max_align_t) - 1) & ~(_Alignof(max_align_t) - 1));
    if (!header) {
        return NULL;
    }
    
    /* Initialize header */
    header->magic = MEMORY_MAGIC;
    header->size = size;
    header->checkpoint = NULL;
    header->next = NULL;
    header->prev = NULL;
    
    /* If we have an active checkpoint AND not bypassing, track the allocation */
    if (tls_memory.current_checkpoint && !tls_memory.current_checkpoint->committed && !tls_memory.bypass_checkpoint) {
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
    free(header);
}

/**
 * Reallocate memory with checkpoint tracking
 */
void* memory_realloc(void* ptr, size_t new_size) {
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
    
    /* Managed allocation - allocate new memory */
    void* new_ptr = memory_alloc(new_size);
    if (!new_ptr) {
        return NULL;
    }
    
    /* Copy data */
    size_t copy_size = header->size < new_size ? header->size : new_size;
    memcpy(new_ptr, ptr, copy_size);
    
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
    
    /* Clean shutdown - no logging from memory manager */
    
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