/**
 * @file memory_manager.h
 * @brief JDBX Memory Manager - Revolutionary Checkpoint-Based Memory Management
 * 
 * Provides automatic memory cleanup through checkpoint/rewind architecture.
 * Single source of truth for advanced memory management in JDBX.
 * 
 * Key Features:
 * - Checkpoint creation for memory transaction boundaries
 * - Automatic cleanup on rewind (eliminates memory leaks!)
 * - Zero-copy promotion for surviving allocations
 * - Thread-local checkpoint stacks
 * - Seamless integration with existing codebase
 */

#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include "utils/memory_types.h"

/* Forward declarations */
typedef struct memory_checkpoint memory_checkpoint_t;

/**
 * Create a memory checkpoint
 * All allocations after this point can be automatically freed with rewind
 * 
 * @return New checkpoint or NULL on failure
 */
memory_checkpoint_t* memory_checkpoint_create(void);

/**
 * Rewind to a checkpoint, freeing all allocations since then
 * This eliminates manual cleanup code and prevents memory leaks
 * 
 * @param checkpoint The checkpoint to rewind to
 */
void memory_checkpoint_rewind(memory_checkpoint_t* checkpoint);

/**
 * Commit a checkpoint, making all allocations permanent
 * 
 * @param checkpoint The checkpoint to commit
 */
void memory_checkpoint_commit(memory_checkpoint_t* checkpoint);

/**
 * Promote an allocation to survive checkpoint rewind
 * Useful for return values that need to outlive the checkpoint
 * 
 * @param ptr The allocation to promote
 * @return The promoted pointer (same address, different tracking)
 */
void* memory_promote(void* ptr);

/**
 * Get current active checkpoint for this thread
 * 
 * @return Current checkpoint or NULL
 */
memory_checkpoint_t* memory_checkpoint_current(void);

/**
 * Allocate memory with checkpoint tracking
 * If a checkpoint is active, allocation will be tracked for automatic cleanup
 * 
 * @param size Size to allocate
 * @return Allocated memory or NULL
 */
void* memory_alloc(size_t size);

/**
 * Allocate zero-initialized memory with checkpoint tracking
 * 
 * @param nmemb Number of members
 * @param size Size of each member
 * @return Allocated memory or NULL
 */
void* memory_calloc(size_t nmemb, size_t size);

/**
 * Free memory (checkpoint aware)
 * 
 * @param ptr Memory to free
 */
void memory_free(void* ptr);

/**
 * Reallocate memory with checkpoint tracking
 * 
 * @param ptr Original pointer
 * @param new_size New size needed
 * @return Reallocated memory or NULL
 */
void* memory_realloc(void* ptr, size_t new_size);

/**
 * Duplicate string with checkpoint tracking
 * 
 * @param str String to duplicate
 * @return Duplicated string or NULL
 */
char* memory_strdup(const char* str);

/**
 * Initialize memory manager (called once at startup)
 */
void memory_manager_init(void);

/**
 * Shutdown memory manager and report any leaks
 */
void memory_manager_shutdown(void);

/**
 * Temporarily bypass checkpoint for allocations
 * Used by logger and other infrastructure to avoid recursion
 * 
 * @param bypass 1 to bypass checkpoints, 0 to re-enable
 */
void memory_bypass_checkpoint(int bypass);

/**
 * Get memory manager statistics
 * 
 * @param checkpoints_created Output: Total checkpoints created
 * @param rewinds Output: Total rewinds performed
 * @param allocations_freed Output: Total allocations freed by rewinds
 */
void memory_manager_get_stats(uint64_t* checkpoints_created, uint64_t* rewinds, uint64_t* allocations_freed);

/**
 * Mark an allocation as hazard-protected
 * The allocation will not be freed on checkpoint rewind until the hazard is cleared
 * 
 * @param ptr The allocation to protect
 * @param hazard_data Hazard pointer system data
 */
void memory_mark_hazard_protected(void* ptr, void* hazard_data);

/**
 * Clear hazard protection from an allocation
 * If the checkpoint was already rewound, the memory will be freed
 * 
 * @param ptr The allocation to unprotect
 */
void memory_clear_hazard_protection(void* ptr);

/**
 * Callback from hazard pointer system when memory is safe to free
 * 
 * @param ptr The memory that can now be safely freed
 */
void memory_hazard_retire_callback(void* ptr);

/* Internal functions for advanced memory management */
memory_header_t* get_memory_header(void* ptr);
int should_use_tlsf_allocator(size_t size);

/* Convenience macros for easy integration */
#define MEMORY_CHECKPOINT() memory_checkpoint_create()
#define MEMORY_REWIND(cp) memory_checkpoint_rewind(cp)
#define MEMORY_COMMIT(cp) memory_checkpoint_commit(cp)

#endif /* MEMORY_MANAGER_H */