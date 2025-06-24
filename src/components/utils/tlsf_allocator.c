#include "utils/tlsf_allocator.h"
#include "utils/logger.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

/* TLSF Two-Level Segregated Fit Implementation
 * 
 * This is a highly optimized O(1) memory allocator with minimal fragmentation.
 * Based on the algorithm described in:
 * "TLSF: A New Dynamic Memory Allocator for Real-Time Systems"
 * by M. Masmano, I. Ripoll, A. Crespo, and J. Real
 */

/* Block header structure - 16 bytes */
typedef struct block_header_t {
    /* Size includes header, lower bits are flags */
    size_t size;
    /* Previous physical block for coalescing */
    struct block_header_t* prev_phys_block;
} block_header_t;

/* Free block structure - extends block header */
typedef struct free_block_t {
    block_header_t header;
    /* Free list pointers */
    struct free_block_t* next_free;
    struct free_block_t* prev_free;
    /* Checkpoint ID for tracking */
    uint32_t checkpoint_id;
} free_block_t;

/* TLSF pool structure */
struct tlsf_pool_t {
    /* Bitmap for first level */
    uint32_t fl_bitmap;
    /* Bitmap for second level */
    uint32_t sl_bitmap[TLSF_FL_INDEX_MAX];
    /* Free lists organized by size class */
    free_block_t* blocks[TLSF_FL_INDEX_MAX][TLSF_SL_INDEX_COUNT];
    /* Pool memory region */
    void* pool_memory;
    size_t pool_size;
    /* Current checkpoint ID */
    uint32_t current_checkpoint;
    /* Statistics */
    size_t used_size;
    uint32_t used_blocks;
    uint32_t free_blocks;
    /* Thread safety */
    pthread_spinlock_t lock;
};

/* Thread-local storage for per-thread pools */
static __thread tlsf_pool_t* thread_pool = NULL;

/* Utility functions */
static inline size_t align_up(size_t size, size_t align) {
    return (size + (align - 1)) & ~(align - 1);
}

static inline int fls(uint32_t word) {
    return word ? (32 - __builtin_clz(word)) : 0;
}

static inline int tlsf_ffs(uint32_t word) {
    return __builtin_ffs(word) - 1;
}

/* Size to fl/sl mapping */
static void mapping_insert(size_t size, int* fl, int* sl) {
    if (size < 256) {
        *fl = 0;
        *sl = (int)(size / 8);
    } else {
        *fl = fls((uint32_t)size) - 1;
        *sl = (int)((size >> (*fl - 5)) & 0x1f);
        *fl -= 7;
    }
}

static size_t mapping_search(int fl, int sl) {
    if (fl == 0) {
        return sl * 8;
    } else {
        return (1 << (fl + 7)) | (sl << (fl + 2));
    }
}

/* Block operations */
static inline size_t block_get_size(block_header_t* block) {
    return block->size & ~0x7;
}

static inline void block_set_size(block_header_t* block, size_t size) {
    block->size = size | (block->size & 0x7);
}

static inline int block_is_free(block_header_t* block) {
    return block->size & TLSF_BLOCK_FREE;
}

static inline void block_set_free(block_header_t* block) {
    block->size |= TLSF_BLOCK_FREE;
}

static inline void block_set_used(block_header_t* block) {
    block->size &= ~TLSF_BLOCK_FREE;
}

static inline int block_is_prev_free(block_header_t* block) {
    return block->size & TLSF_BLOCK_PREV_FREE;
}

static inline void block_set_prev_free(block_header_t* block) {
    block->size |= TLSF_BLOCK_PREV_FREE;
}

static inline void block_set_prev_used(block_header_t* block) {
    block->size &= ~TLSF_BLOCK_PREV_FREE;
}

static inline block_header_t* block_next(block_header_t* block) {
    return (block_header_t*)((char*)block + block_get_size(block));
}

/* Remove block from free list */
static void remove_free_block(tlsf_pool_t* pool, free_block_t* block, int fl, int sl) {
    free_block_t* prev = block->prev_free;
    free_block_t* next = block->next_free;
    
    if (next) {
        next->prev_free = prev;
    }
    if (prev) {
        prev->next_free = next;
    }
    
    /* If this was the head of the list, update it */
    if (pool->blocks[fl][sl] == block) {
        pool->blocks[fl][sl] = next;
        if (!next) {
            /* Clear second level bit */
            pool->sl_bitmap[fl] &= ~(1 << sl);
            if (!pool->sl_bitmap[fl]) {
                /* Clear first level bit */
                pool->fl_bitmap &= ~(1 << fl);
            }
        }
    }
    
    pool->free_blocks--;
}

/* Insert block into free list */
static void insert_free_block(tlsf_pool_t* pool, free_block_t* block, int fl, int sl) {
    free_block_t* head = pool->blocks[fl][sl];
    
    block->next_free = head;
    block->prev_free = NULL;
    
    if (head) {
        head->prev_free = block;
    }
    
    pool->blocks[fl][sl] = block;
    
    /* Set bitmaps */
    pool->fl_bitmap |= (1 << fl);
    pool->sl_bitmap[fl] |= (1 << sl);
    
    pool->free_blocks++;
}

/* Find suitable block using bitmaps */
static free_block_t* find_suitable_block(tlsf_pool_t* pool, int* fl, int* sl) {
    uint32_t sl_map = pool->sl_bitmap[*fl] & (~0u << *sl);
    
    if (!sl_map) {
        /* No suitable blocks in this fl, search higher */
        uint32_t fl_map = pool->fl_bitmap & (~0u << (*fl + 1));
        if (!fl_map) {
            return NULL; /* No suitable blocks */
        }
        *fl = tlsf_ffs(fl_map);
        sl_map = pool->sl_bitmap[*fl];
    }
    
    *sl = tlsf_ffs(sl_map);
    return pool->blocks[*fl][*sl];
}

/* Split block if too large */
static void split_block(tlsf_pool_t* pool, block_header_t* block, size_t size) {
    size_t block_size = block_get_size(block);
    size_t remaining = block_size - size;
    
    if (remaining >= TLSF_MIN_BLOCK_SIZE + TLSF_BLOCK_HEADER_SIZE) {
        /* Create new block from remainder */
        block_header_t* remaining_block = (block_header_t*)((char*)block + size);
        remaining_block->size = remaining | TLSF_BLOCK_FREE;
        remaining_block->prev_phys_block = block;
        
        /* Update next block's prev pointer */
        block_header_t* next = block_next(remaining_block);
        if ((char*)next < (char*)pool->pool_memory + pool->pool_size) {
            next->prev_phys_block = remaining_block;
            block_set_prev_free(next);
        }
        
        /* Update current block */
        block_set_size(block, size);
        
        /* Insert remainder into free list */
        int fl, sl;
        mapping_insert(remaining - TLSF_BLOCK_HEADER_SIZE, &fl, &sl);
        insert_free_block(pool, (free_block_t*)remaining_block, fl, sl);
    }
}

/* Merge with adjacent free blocks */
static block_header_t* merge_prev(tlsf_pool_t* pool, block_header_t* block) {
    if (block_is_prev_free(block)) {
        block_header_t* prev = block->prev_phys_block;
        size_t prev_size = block_get_size(prev);
        
        /* Remove prev from free list */
        int fl, sl;
        mapping_insert(prev_size - TLSF_BLOCK_HEADER_SIZE, &fl, &sl);
        remove_free_block(pool, (free_block_t*)prev, fl, sl);
        
        /* Merge */
        size_t new_size = prev_size + block_get_size(block);
        block_set_size(prev, new_size);
        
        /* Update next block's prev pointer */
        block_header_t* next = block_next(prev);
        if ((char*)next < (char*)pool->pool_memory + pool->pool_size) {
            next->prev_phys_block = prev;
        }
        
        return prev;
    }
    return block;
}

static void merge_next(tlsf_pool_t* pool, block_header_t* block) {
    block_header_t* next = block_next(block);
    if ((char*)next < (char*)pool->pool_memory + pool->pool_size && block_is_free(next)) {
        size_t next_size = block_get_size(next);
        
        /* Remove next from free list */
        int fl, sl;
        mapping_insert(next_size - TLSF_BLOCK_HEADER_SIZE, &fl, &sl);
        remove_free_block(pool, (free_block_t*)next, fl, sl);
        
        /* Merge */
        size_t new_size = block_get_size(block) + next_size;
        block_set_size(block, new_size);
        
        /* Update following block's prev pointer */
        block_header_t* following = block_next(block);
        if ((char*)following < (char*)pool->pool_memory + pool->pool_size) {
            following->prev_phys_block = block;
        }
    }
}

/* Initialize TLSF system */
int tlsf_init(void) {
    /* Verify assumptions */
    assert(sizeof(block_header_t) == TLSF_BLOCK_HEADER_SIZE);
    return 1;
}

/* Create a new TLSF pool */
tlsf_pool_t* tlsf_create_pool(size_t pool_size) {
    /* Allocate pool structure and memory in one chunk */
    size_t total_size = sizeof(tlsf_pool_t) + pool_size;
    void* memory = aligned_alloc(64, total_size);
    if (!memory) {
        return NULL;
    }
    
    tlsf_pool_t* pool = (tlsf_pool_t*)memory;
    memset(pool, 0, sizeof(tlsf_pool_t));
    
    /* Initialize pool */
    pool->pool_memory = (char*)memory + sizeof(tlsf_pool_t);
    pool->pool_size = pool_size;
    pthread_spin_init(&pool->lock, PTHREAD_PROCESS_PRIVATE);
    
    /* Create initial free block spanning entire pool */
    block_header_t* initial_block = (block_header_t*)pool->pool_memory;
    initial_block->size = pool_size | TLSF_BLOCK_FREE;
    initial_block->prev_phys_block = NULL;
    
    /* Insert into free list */
    int fl, sl;
    mapping_insert(pool_size - TLSF_BLOCK_HEADER_SIZE, &fl, &sl);
    insert_free_block(pool, (free_block_t*)initial_block, fl, sl);
    
    LOG_DEBUG("Created TLSF pool: size=%zu, address=%p", pool_size, pool);
    return pool;
}

/* Destroy a TLSF pool */
void tlsf_destroy_pool(tlsf_pool_t* pool) {
    if (!pool) return;
    
    pthread_spin_destroy(&pool->lock);
    free(pool);
}

/* Allocate memory from pool */
void* tlsf_malloc(tlsf_pool_t* pool, size_t size) {
    if (!pool || size == 0) return NULL;
    
    /* Adjust size for header and alignment */
    size = align_up(size + TLSF_BLOCK_HEADER_SIZE, 8);
    if (size < TLSF_MIN_BLOCK_SIZE) {
        size = TLSF_MIN_BLOCK_SIZE;
    }
    
    pthread_spin_lock(&pool->lock);
    
    /* Find suitable block */
    int fl, sl;
    mapping_insert(size - TLSF_BLOCK_HEADER_SIZE, &fl, &sl);
    free_block_t* block = find_suitable_block(pool, &fl, &sl);
    
    if (!block) {
        pthread_spin_unlock(&pool->lock);
        return NULL; /* No suitable block found */
    }
    
    /* Remove from free list */
    remove_free_block(pool, block, fl, sl);
    
    /* Split if necessary */
    split_block(pool, &block->header, size);
    
    /* Mark as used */
    block_set_used(&block->header);
    
    /* Update stats */
    pool->used_size += block_get_size(&block->header);
    pool->used_blocks++;
    
    /* Store checkpoint ID */
    if (pool->current_checkpoint) {
        block->checkpoint_id = pool->current_checkpoint;
        block->header.size |= TLSF_BLOCK_CHECKPOINT;
    }
    
    pthread_spin_unlock(&pool->lock);
    
    /* Return pointer after header */
    return (char*)block + TLSF_BLOCK_HEADER_SIZE;
}

/* Free memory back to pool */
void tlsf_free(tlsf_pool_t* pool, void* ptr) {
    if (!pool || !ptr) return;
    
    /* Get block header */
    block_header_t* block = (block_header_t*)((char*)ptr - TLSF_BLOCK_HEADER_SIZE);
    
    pthread_spin_lock(&pool->lock);
    
    /* Update stats */
    pool->used_size -= block_get_size(block);
    pool->used_blocks--;
    
    /* Mark as free */
    block_set_free(block);
    
    /* Merge with neighbors */
    block = merge_prev(pool, block);
    merge_next(pool, block);
    
    /* Update next block's prev_free flag */
    block_header_t* next = block_next(block);
    if ((char*)next < (char*)pool->pool_memory + pool->pool_size) {
        block_set_prev_free(next);
    }
    
    /* Insert into free list */
    int fl, sl;
    mapping_insert(block_get_size(block) - TLSF_BLOCK_HEADER_SIZE, &fl, &sl);
    insert_free_block(pool, (free_block_t*)block, fl, sl);
    
    pthread_spin_unlock(&pool->lock);
}

/* Allocate zeroed memory */
void* tlsf_calloc(tlsf_pool_t* pool, size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void* ptr = tlsf_malloc(pool, total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

/* Reallocate memory */
void* tlsf_realloc(tlsf_pool_t* pool, void* ptr, size_t size) {
    if (!ptr) return tlsf_malloc(pool, size);
    if (size == 0) {
        tlsf_free(pool, ptr);
        return NULL;
    }
    
    /* Get current size */
    block_header_t* block = (block_header_t*)((char*)ptr - TLSF_BLOCK_HEADER_SIZE);
    size_t current_size = block_get_size(block) - TLSF_BLOCK_HEADER_SIZE;
    
    if (size <= current_size) {
        return ptr; /* Current block is large enough */
    }
    
    /* Allocate new block and copy */
    void* new_ptr = tlsf_malloc(pool, size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, current_size);
        tlsf_free(pool, ptr);
    }
    return new_ptr;
}

/* Get actual allocated size */
size_t tlsf_block_size(void* ptr) {
    if (!ptr) return 0;
    block_header_t* block = (block_header_t*)((char*)ptr - TLSF_BLOCK_HEADER_SIZE);
    return block_get_size(block) - TLSF_BLOCK_HEADER_SIZE;
}

/* Get pool statistics */
void tlsf_get_stats(tlsf_pool_t* pool, tlsf_stats_t* stats) {
    if (!pool || !stats) return;
    
    pthread_spin_lock(&pool->lock);
    
    stats->pool_size = pool->pool_size;
    stats->used_size = pool->used_size;
    stats->free_size = pool->pool_size - pool->used_size;
    stats->overhead_size = pool->used_blocks * TLSF_BLOCK_HEADER_SIZE;
    stats->used_blocks = pool->used_blocks;
    stats->free_blocks = pool->free_blocks;
    
    /* Calculate fragmentation */
    if (stats->free_size > 0) {
        /* Find largest free block */
        size_t largest_free = 0;
        for (int fl = TLSF_FL_INDEX_MAX - 1; fl >= 0; fl--) {
            if (pool->fl_bitmap & (1 << fl)) {
                int sl = fls(pool->sl_bitmap[fl]) - 1;
                if (sl >= 0) {
                    largest_free = mapping_search(fl, sl);
                    break;
                }
            }
        }
        stats->fragmentation_percent = 100 - (100 * largest_free / stats->free_size);
    } else {
        stats->fragmentation_percent = 0;
    }
    
    pthread_spin_unlock(&pool->lock);
}

/* Get thread-local pool */
tlsf_pool_t* tlsf_get_thread_pool(void) {
    if (!thread_pool) {
        /* Create default 64MB pool per thread */
        thread_pool = tlsf_create_pool(64 * 1024 * 1024);
    }
    return thread_pool;
}

/* Set checkpoint ID */
void tlsf_set_checkpoint(tlsf_pool_t* pool, uint32_t checkpoint_id) {
    if (!pool) return;
    pthread_spin_lock(&pool->lock);
    pool->current_checkpoint = checkpoint_id;
    pthread_spin_unlock(&pool->lock);
}

/* Free all blocks with given checkpoint ID */
void tlsf_rewind_checkpoint(tlsf_pool_t* pool, uint32_t checkpoint_id) {
    if (!pool) return;
    
    pthread_spin_lock(&pool->lock);
    
    /* Walk all used blocks and free matching checkpoint */
    block_header_t* block = (block_header_t*)pool->pool_memory;
    
    while ((char*)block < (char*)pool->pool_memory + pool->pool_size) {
        size_t block_size = block_get_size(block);
        
        if (!block_is_free(block) && (block->size & TLSF_BLOCK_CHECKPOINT)) {
            free_block_t* free_block = (free_block_t*)block;
            if (free_block->checkpoint_id == checkpoint_id) {
                /* Free this block - we need to be careful here */
                pthread_spin_unlock(&pool->lock);
                tlsf_free(pool, (char*)block + TLSF_BLOCK_HEADER_SIZE);
                pthread_spin_lock(&pool->lock);
                /* Restart scan as memory layout may have changed */
                block = (block_header_t*)pool->pool_memory;
                continue;
            }
        }
        
        block = (block_header_t*)((char*)block + block_size);
    }
    
    pthread_spin_unlock(&pool->lock);
}