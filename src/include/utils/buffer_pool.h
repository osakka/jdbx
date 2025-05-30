#ifndef BUFFER_POOL_H
#define BUFFER_POOL_H

#include <stddef.h>
#include <stdint.h>

/**
 * High-performance buffer pool for reducing malloc/free overhead
 * 
 * The buffer pool provides thread-local pools of pre-allocated buffers
 * in common size classes. This significantly reduces allocation overhead
 * in hot paths.
 * 
 * Features:
 * - Thread-local pools (no contention between threads)
 * - Multiple size classes (64B, 512B, 4KB, 16KB, 64KB, 256KB)
 * - Automatic buffer recycling
 * - Fallback to malloc for large sizes
 * - Statistics tracking
 */

/**
 * Allocate a buffer from the pool
 * 
 * @param size The size of buffer needed
 * @return Pointer to allocated buffer, or NULL on failure
 */
void* buffer_pool_alloc(size_t size);

/**
 * Free a buffer back to the pool
 * 
 * @param ptr Pointer to buffer to free
 */
void buffer_pool_free(void* ptr);

/**
 * Reallocate a buffer
 * 
 * @param ptr Pointer to existing buffer (may be NULL)
 * @param new_size New size needed
 * @return Pointer to reallocated buffer, or NULL on failure
 */
void* buffer_pool_realloc(void* ptr, size_t new_size);

/**
 * Duplicate a string using buffer pool
 * 
 * @param str String to duplicate
 * @return Duplicated string, or NULL on failure
 */
char* buffer_pool_strdup(const char* str);

/**
 * Get buffer pool statistics
 * 
 * @param total_allocs Output: Total allocations made
 * @param pool_hits Output: Number of allocations served from pool
 * @param pool_misses Output: Number of allocations that went to malloc
 */
void buffer_pool_get_stats(uint64_t* total_allocs, uint64_t* pool_hits, uint64_t* pool_misses);

/**
 * Reset buffer pool statistics
 */
void buffer_pool_reset_stats(void);

/**
 * Safe free function that handles both buffer pool and malloc allocations
 * 
 * @param ptr Pointer to free (can be from buffer pool or malloc)
 */
void buffer_pool_free_safe(void* ptr);

/* Convenience macros for common operations */
#define BUFFER_ALLOC(size) buffer_pool_alloc(size)
#define BUFFER_FREE(ptr) buffer_pool_free(ptr)
#define BUFFER_FREE_SAFE(ptr) buffer_pool_free_safe(ptr)
#define BUFFER_REALLOC(ptr, size) buffer_pool_realloc(ptr, size)
#define BUFFER_STRDUP(str) buffer_pool_strdup(str)

#endif /* BUFFER_POOL_H */