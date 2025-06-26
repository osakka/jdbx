#ifndef JDBX_MEMORY_TYPES_H
#define JDBX_MEMORY_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <pthread.h>

/**
 * @file memory_types.h
 * @brief Shared memory management type definitions
 * 
 * Common types and constants used across the memory management system.
 * This avoids circular dependencies between memory manager components.
 */

/* Memory flags */
#define MEMORY_FLAG_HAZARD_PROTECTED 0x00000001  /* Protected by hazard pointers */
#define MEMORY_FLAG_ARENA_ALLOCATED  0x00000002  /* Allocated from arena */
#define MEMORY_FLAG_TLSF_ALLOCATED   0x00000004  /* Allocated from TLSF pool */

/* Magic numbers */
#define MEMORY_MAGIC 0xDEADBEEF
#define MEMORY_MAGIC_FREE 0xFEEDF00D

/* Forward declarations */
typedef struct arena_t arena_t;
typedef struct memory_checkpoint memory_checkpoint_t;

/* Memory allocation header for tracking */
typedef struct memory_header {
    struct memory_header* next;      /* Next allocation in checkpoint */
    struct memory_header* prev;      /* Previous allocation in checkpoint */
    memory_checkpoint_t* checkpoint; /* Owning checkpoint */
    size_t size;                    /* Allocation size */
    uint32_t magic;                 /* Magic number for corruption detection */
    uint32_t flags;                 /* Memory flags (hazard protected, etc) */
    void* hazard_data;              /* Hazard pointer data if protected */
    void* tlsf_ptr;                 /* Original TLSF pointer for freeing (if TLSF allocated) */
    /* User data follows immediately after this header with proper alignment */
} memory_header_t;

/* Calculate header size with proper alignment for max_align_t */
#define HEADER_SIZE ((sizeof(memory_header_t) + _Alignof(max_align_t) - 1) & ~(_Alignof(max_align_t) - 1))

/* Memory checkpoint structure */
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

#endif /* JDBX_MEMORY_TYPES_H */