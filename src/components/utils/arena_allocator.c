#include "utils/arena_allocator.h"
#include "utils/tlsf_allocator.h"
#include "utils/logger.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Arena Allocator Implementation
 * 
 * Fastest possible allocation for temporary objects.
 * Perfect for checkpoint-based memory management.
 */

/* Checkpoint marker in arena */
typedef struct arena_checkpoint_t {
    uint32_t checkpoint_id;
    size_t position;
    struct arena_checkpoint_t* next;
} arena_checkpoint_t;

/* Arena structure */
struct arena_t {
    /* Memory region */
    char* memory;
    size_t size;
    
    /* Current allocation position */
    char* current;
    
    /* Checkpoint tracking */
    arena_checkpoint_t* checkpoints;
    uint32_t next_checkpoint_id;
    
    /* Statistics */
    uint32_t allocation_count;
    
    /* For chaining multiple arenas if needed */
    struct arena_t* next;
};

/* Thread-local arena */
static __thread arena_t* thread_arena = NULL;

/* Utility: align pointer */
static inline char* align_ptr(char* ptr, size_t alignment) {
    uintptr_t addr = (uintptr_t)ptr;
    uintptr_t aligned = (addr + alignment - 1) & ~(alignment - 1);
    return (char*)aligned;
}

/* Create new arena */
arena_t* arena_create(size_t size) {
    if (size < ARENA_DEFAULT_SIZE) {
        size = ARENA_DEFAULT_SIZE;
    }
    
    /* Allocate arena structure and memory together */
    size_t total_size = sizeof(arena_t) + size;
    arena_t* arena = (arena_t*)aligned_alloc(64, total_size);
    if (!arena) {
        LOG_ERROR("Failed to allocate arena of size %zu", total_size);
        return NULL;
    }
    
    /* Initialize arena */
    memset(arena, 0, sizeof(arena_t));
    arena->memory = (char*)arena + sizeof(arena_t);
    arena->size = size;
    arena->current = arena->memory;
    arena->next_checkpoint_id = 1;
    
    LOG_DEBUG("Created arena: size=%zu, address=%p", size, arena);
    return arena;
}

/* Destroy arena */
void arena_destroy(arena_t* arena) {
    if (!arena) return;
    
    /* Free any chained arenas */
    arena_t* next = arena->next;
    while (next) {
        arena_t* tmp = next->next;
        free(next);
        next = tmp;
    }
    
    /* Free checkpoint list */
    arena_checkpoint_t* cp = arena->checkpoints;
    while (cp) {
        arena_checkpoint_t* tmp = cp->next;
        free(cp);
        cp = tmp;
    }
    
    free(arena);
}

/* Allocate from arena - ULTRA FAST PATH */
void* arena_alloc(arena_t* arena, size_t size) {
    if (!arena || size == 0) return NULL;
    
    /* Align current pointer */
    char* aligned = align_ptr(arena->current, ARENA_ALIGNMENT);
    
    /* Check if we have enough space */
    if (aligned + size > arena->memory + arena->size) {
        /* Try to allocate from overflow arena */
        if (!arena->next) {
            /* Create overflow arena */
            size_t new_size = arena->size;
            if (size > new_size - ARENA_MIN_REMAINING) {
                new_size = size + ARENA_MIN_REMAINING;
            }
            arena->next = arena_create(new_size);
            if (!arena->next) {
                LOG_ERROR("Failed to create overflow arena");
                return NULL;
            }
        }
        return arena_alloc(arena->next, size);
    }
    
    /* Fast path - just bump the pointer */
    void* result = aligned;
    arena->current = aligned + size;
    arena->allocation_count++;
    
    return result;
}

/* Allocate aligned memory */
void* arena_alloc_aligned(arena_t* arena, size_t size, size_t alignment) {
    if (!arena || size == 0) return NULL;
    
    /* Align current pointer to requested alignment */
    char* aligned = align_ptr(arena->current, alignment);
    
    /* Check space */
    if (aligned + size > arena->memory + arena->size) {
        if (!arena->next) {
            size_t new_size = arena->size;
            if (size > new_size - ARENA_MIN_REMAINING) {
                new_size = size + ARENA_MIN_REMAINING;
            }
            arena->next = arena_create(new_size);
            if (!arena->next) {
                return NULL;
            }
        }
        return arena_alloc_aligned(arena->next, size, alignment);
    }
    
    void* result = aligned;
    arena->current = aligned + size;
    arena->allocation_count++;
    
    return result;
}

/* Create checkpoint */
uint32_t arena_checkpoint(arena_t* arena) {
    if (!arena) return 0;
    
    /* Allocate checkpoint marker */
    arena_checkpoint_t* cp = (arena_checkpoint_t*)malloc(sizeof(arena_checkpoint_t));
    if (!cp) return 0;
    
    /* Initialize checkpoint */
    cp->checkpoint_id = arena->next_checkpoint_id++;
    cp->position = (size_t)(arena->current - arena->memory);
    cp->next = arena->checkpoints;
    arena->checkpoints = cp;
    
    LOG_DEBUG("Arena checkpoint %u at position %zu", cp->checkpoint_id, cp->position);
    return cp->checkpoint_id;
}

/* Reset to checkpoint - BULK FREE */
void arena_reset_to_checkpoint(arena_t* arena, uint32_t checkpoint_id) {
    if (!arena || checkpoint_id == 0) return;
    
    /* Find checkpoint */
    arena_checkpoint_t* cp = arena->checkpoints;
    
    while (cp && cp->checkpoint_id != checkpoint_id) {
        cp = cp->next;
    }
    
    if (!cp) {
        LOG_WARNING("Checkpoint %u not found in arena", checkpoint_id);
        return;
    }
    
    /* Reset arena position */
    arena->current = arena->memory + cp->position;
    
    /* Remove newer checkpoints */
    arena_checkpoint_t* newer = arena->checkpoints;
    while (newer != cp) {
        arena_checkpoint_t* tmp = newer->next;
        free(newer);
        newer = tmp;
    }
    arena->checkpoints = cp;
    
    /* Reset overflow arenas */
    if (arena->next) {
        arena_destroy(arena->next);
        arena->next = NULL;
    }
    
    LOG_DEBUG("Arena reset to checkpoint %u, position %zu", checkpoint_id, cp->position);
}

/* Get statistics */
void arena_get_stats(arena_t* arena, arena_stats_t* stats) {
    if (!arena || !stats) return;
    
    memset(stats, 0, sizeof(arena_stats_t));
    
    /* Calculate stats for this arena */
    stats->total_size = arena->size;
    stats->used_size = (size_t)(arena->current - arena->memory);
    stats->free_size = stats->total_size - stats->used_size;
    stats->allocation_count = arena->allocation_count;
    
    /* Count checkpoints */
    arena_checkpoint_t* cp = arena->checkpoints;
    while (cp) {
        stats->checkpoint_count++;
        cp = cp->next;
    }
    
    /* Add stats from overflow arenas */
    if (arena->next) {
        arena_stats_t overflow_stats;
        arena_get_stats(arena->next, &overflow_stats);
        stats->total_size += overflow_stats.total_size;
        stats->used_size += overflow_stats.used_size;
        stats->free_size += overflow_stats.free_size;
        stats->allocation_count += overflow_stats.allocation_count;
    }
}

/* Get thread-local arena */
arena_t* arena_get_thread_arena(void) {
    if (!thread_arena) {
        thread_arena = arena_create(ARENA_DEFAULT_SIZE);
    }
    return thread_arena;
}

/* Check if pointer is in arena */
int arena_contains(arena_t* arena, void* ptr) {
    if (!arena || !ptr) return 0;
    
    char* p = (char*)ptr;
    
    /* Check this arena */
    if (p >= arena->memory && p < arena->memory + arena->size) {
        return 1;
    }
    
    /* Check overflow arenas */
    return arena->next ? arena_contains(arena->next, ptr) : 0;
}

/* Promote object from arena to TLSF */
void* arena_promote(arena_t* arena, void* ptr, size_t size) {
    if (!arena || !ptr || !arena_contains(arena, ptr)) {
        return NULL;
    }
    
    /* Allocate in TLSF */
    tlsf_pool_t* tlsf = tlsf_get_thread_pool();
    void* new_ptr = tlsf_malloc(tlsf, size);
    if (!new_ptr) {
        LOG_ERROR("Failed to promote arena object to TLSF");
        return NULL;
    }
    
    /* Copy data */
    memcpy(new_ptr, ptr, size);
    
    LOG_DEBUG("Promoted object from arena to TLSF: old=%p, new=%p, size=%zu", ptr, new_ptr, size);
    return new_ptr;
}