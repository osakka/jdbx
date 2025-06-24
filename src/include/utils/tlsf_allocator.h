#ifndef JDBX_TLSF_ALLOCATOR_H
#define JDBX_TLSF_ALLOCATOR_H

#include <stddef.h>
#include <stdint.h>

/* TLSF (Two-Level Segregated Fit) Memory Allocator
 * 
 * Features:
 * - O(1) allocation and deallocation
 * - Low fragmentation (< 3%)
 * - Predictable performance
 * - Thread-safe with per-thread pools
 * - Checkpoint integration
 */

/* Forward declarations */
typedef struct tlsf_pool_t tlsf_pool_t;

/* TLSF configuration constants */
#define TLSF_MIN_BLOCK_SIZE     16      /* Minimum allocation size */
#define TLSF_MAX_BLOCK_SIZE     (1<<30) /* Maximum allocation size (1GB) */
#define TLSF_FL_INDEX_MAX       30      /* First level index bits */
#define TLSF_SL_INDEX_COUNT     32      /* Second level divisions */
#define TLSF_BLOCK_HEADER_SIZE  16      /* Header overhead per block */

/* Block header flags */
#define TLSF_BLOCK_FREE         0x01
#define TLSF_BLOCK_PREV_FREE    0x02
#define TLSF_BLOCK_CHECKPOINT   0x04

/* Initialize TLSF system */
int tlsf_init(void);

/* Create a new TLSF pool with given size */
tlsf_pool_t* tlsf_create_pool(size_t pool_size);

/* Destroy a TLSF pool */
void tlsf_destroy_pool(tlsf_pool_t* pool);

/* Allocate memory from pool */
void* tlsf_malloc(tlsf_pool_t* pool, size_t size);

/* Allocate zeroed memory from pool */
void* tlsf_calloc(tlsf_pool_t* pool, size_t nmemb, size_t size);

/* Reallocate memory in pool */
void* tlsf_realloc(tlsf_pool_t* pool, void* ptr, size_t size);

/* Free memory back to pool */
void tlsf_free(tlsf_pool_t* pool, void* ptr);

/* Get actual allocated size for a pointer */
size_t tlsf_block_size(void* ptr);

/* Pool statistics */
typedef struct {
    size_t pool_size;
    size_t used_size;
    size_t free_size;
    size_t overhead_size;
    size_t fragmentation_percent;
    uint32_t free_blocks;
    uint32_t used_blocks;
} tlsf_stats_t;

/* Get pool statistics */
void tlsf_get_stats(tlsf_pool_t* pool, tlsf_stats_t* stats);

/* Thread-local pool access */
tlsf_pool_t* tlsf_get_thread_pool(void);

/* Set checkpoint ID for subsequent allocations */
void tlsf_set_checkpoint(tlsf_pool_t* pool, uint32_t checkpoint_id);

/* Free all blocks associated with a checkpoint */
void tlsf_rewind_checkpoint(tlsf_pool_t* pool, uint32_t checkpoint_id);

#endif /* JDBX_TLSF_ALLOCATOR_H */