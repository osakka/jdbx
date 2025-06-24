#ifndef JDBX_ARENA_ALLOCATOR_H
#define JDBX_ARENA_ALLOCATOR_H

#include <stddef.h>
#include <stdint.h>

/* Arena Allocator - Bump Pointer with Checkpoint Integration
 * 
 * Ultra-fast allocation for temporary objects:
 * - Single pointer increment allocation (1 instruction)
 * - Bulk deallocation on checkpoint rewind
 * - Zero fragmentation within arena
 * - Automatic promotion to TLSF for survivors
 */

typedef struct arena_t arena_t;

/* Arena configuration */
#define ARENA_DEFAULT_SIZE      (4 * 1024 * 1024)  /* 4MB default */
#define ARENA_ALIGNMENT         16                   /* 16-byte alignment */
#define ARENA_MIN_REMAINING     1024                /* Min space before new arena */

/* Create a new arena with specified size */
arena_t* arena_create(size_t size);

/* Destroy an arena and all its memory */
void arena_destroy(arena_t* arena);

/* Allocate memory from arena - ultra fast */
void* arena_alloc(arena_t* arena, size_t size);

/* Allocate aligned memory from arena */
void* arena_alloc_aligned(arena_t* arena, size_t size, size_t alignment);

/* Reset arena to checkpoint (bulk free) */
void arena_reset_to_checkpoint(arena_t* arena, uint32_t checkpoint_id);

/* Mark current position as checkpoint */
uint32_t arena_checkpoint(arena_t* arena);

/* Get arena statistics */
typedef struct {
    size_t total_size;
    size_t used_size;
    size_t free_size;
    uint32_t allocation_count;
    uint32_t checkpoint_count;
} arena_stats_t;

void arena_get_stats(arena_t* arena, arena_stats_t* stats);

/* Thread-local arena access */
arena_t* arena_get_thread_arena(void);

/* Promote object from arena to TLSF (survives checkpoint) */
void* arena_promote(arena_t* arena, void* ptr, size_t size);

/* Check if pointer is in arena */
int arena_contains(arena_t* arena, void* ptr);

#endif /* JDBX_ARENA_ALLOCATOR_H */