/**
 * @file buffer_pool.c
 * @brief Single Source of Truth Buffer Pool Implementation
 * 
 * Simple, robust memory management using consistent malloc/free with debugging.
 * No parallel implementations, no bootstrap modes - just one way that always works.
 */

#include "utils/buffer_pool.h"
#include "utils/logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Global statistics for monitoring */
static struct {
    volatile uint64_t total_allocations;
    volatile uint64_t total_frees;
} g_stats = {0, 0};

/**
 * Allocate memory with debugging information
 */
void* buffer_pool_alloc_safe(size_t size, const char* file, int line, const char* func) {
    if (size == 0) {
        return NULL;
    }
    
    void* ptr = malloc(size);
    if (ptr) {
        __sync_fetch_and_add(&g_stats.total_allocations, 1);
    }
    
    return ptr;
}

/**
 * Free memory with debugging information
 */
void buffer_pool_free_safe(void* ptr, const char* file, int line, const char* func) {
    if (ptr) {
        free(ptr);
        __sync_fetch_and_add(&g_stats.total_frees, 1);
    }
}

/**
 * Reallocate memory with debugging information
 */
void* buffer_pool_realloc_safe(void* ptr, size_t new_size, const char* file, int line, const char* func) {
    if (new_size == 0) {
        buffer_pool_free_safe(ptr, file, line, func);
        return NULL;
    }
    
    if (!ptr) {
        return buffer_pool_alloc_safe(new_size, file, line, func);
    }
    
    return realloc(ptr, new_size);
}

/**
 * Duplicate string with debugging information
 */
char* buffer_pool_strdup_safe(const char* str, const char* file, int line, const char* func) {
    if (!str) {
        return NULL;
    }
    
    size_t len = strlen(str);
    char* copy = (char*)buffer_pool_alloc_safe(len + 1, file, line, func);
    if (copy) {
        memcpy(copy, str, len + 1);
    }
    
    return copy;
}

/**
 * Get buffer pool statistics
 */
void buffer_pool_get_stats(uint64_t* total_allocs, uint64_t* pool_hits, uint64_t* pool_misses) {
    if (total_allocs) *total_allocs = g_stats.total_allocations;
    if (pool_hits) *pool_hits = 0;  /* Not implemented in simple version */
    if (pool_misses) *pool_misses = 0;  /* Not implemented in simple version */
}

/**
 * Reset statistics
 */
void buffer_pool_reset_stats(void) {
    g_stats.total_allocations = 0;
    g_stats.total_frees = 0;
}

/**
 * Legacy function implementations for backward compatibility
 */
void* buffer_pool_alloc(size_t size) {
    return buffer_pool_alloc_safe(size, "legacy", 0, "buffer_pool_alloc");
}

void buffer_pool_free(void* ptr) {
    buffer_pool_free_safe(ptr, "legacy", 0, "buffer_pool_free");
}

char* buffer_pool_strdup(const char* str) {
    return buffer_pool_strdup_safe(str, "legacy", 0, "buffer_pool_strdup");
}

void* buffer_pool_realloc(void* ptr, size_t new_size) {
    return buffer_pool_realloc_safe(ptr, new_size, "legacy", 0, "buffer_pool_realloc");
}

