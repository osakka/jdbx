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
 * Allocate zero-initialized memory from buffer pool
 * 
 * @param nmemb Number of members
 * @param size Size of each member
 * @return Pointer to allocated buffer, or NULL on failure
 */
void* buffer_pool_calloc(size_t nmemb, size_t size);

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


/* Production-grade buffer pool macros with comprehensive error handling */
#define BUFFER_ALLOC(size) buffer_pool_alloc_safe((size), __FILE__, __LINE__, __func__)
#define BUFFER_CALLOC(nmemb, size) buffer_pool_calloc_safe((nmemb), (size), __FILE__, __LINE__, __func__)
#define BUFFER_FREE(ptr) buffer_pool_free_safe((ptr), __FILE__, __LINE__, __func__)
#define BUFFER_REALLOC(ptr, size) buffer_pool_realloc_safe((ptr), (size), __FILE__, __LINE__, __func__)
#define BUFFER_STRDUP(str) buffer_pool_strdup_safe((str), __FILE__, __LINE__, __func__)

/* Enhanced API with debugging information */
void* buffer_pool_alloc_safe(size_t size, const char* file, int line, const char* func);
void* buffer_pool_calloc_safe(size_t nmemb, size_t size, const char* file, int line, const char* func);
void buffer_pool_free_safe(void* ptr, const char* file, int line, const char* func);
void* buffer_pool_realloc_safe(void* ptr, size_t new_size, const char* file, int line, const char* func);
char* buffer_pool_strdup_safe(const char* str, const char* file, int line, const char* func);


#endif /* BUFFER_POOL_H */