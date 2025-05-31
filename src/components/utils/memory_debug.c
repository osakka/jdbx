#include "utils/memory_debug.h"
#include "utils/buffer_pool.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#ifdef MEMORY_DEBUG_ENABLED

/* Global memory tracking list */
static memory_track_t* g_memory_list = NULL;
static pthread_mutex_t g_memory_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_memory_debug_initialized = 0;

/* Statistics */
static struct {
    size_t total_allocations;
    size_t total_deallocations;
    size_t current_allocations;
    size_t peak_allocations;
    size_t total_bytes_allocated;
    size_t current_bytes_allocated;
    size_t peak_bytes_allocated;
    size_t malloc_allocs;
    size_t buffer_pool_allocs;
    size_t strdup_allocs;
    size_t mismatched_frees;
} g_memory_stats = {0};

/* Thread-safe memory debugging enabled check */
int memory_debug_enabled(void) {
    return logger_trace_enabled(TRACE_MEMORY);
}

/* Initialize memory debugging */
void memory_debug_init(void) {
    pthread_mutex_lock(&g_memory_mutex);
    if (!g_memory_debug_initialized) {
        g_memory_list = NULL;
        memset(&g_memory_stats, 0, sizeof(g_memory_stats));
        g_memory_debug_initialized = 1;
        TRACE_MEMORY("Memory debugging initialized");
    }
    pthread_mutex_unlock(&g_memory_mutex);
}

/* Cleanup memory debugging */
void memory_debug_cleanup(void) {
    pthread_mutex_lock(&g_memory_mutex);
    if (g_memory_debug_initialized) {
        memory_debug_check_mismatches();
        memory_debug_print_summary();
        
        /* Free tracking list */
        memory_track_t* current = g_memory_list;
        while (current) {
            memory_track_t* next = current->next;
            free(current);
            current = next;
        }
        g_memory_list = NULL;
        g_memory_debug_initialized = 0;
        TRACE_MEMORY("Memory debugging cleanup completed");
    }
    pthread_mutex_unlock(&g_memory_mutex);
}

/* Track memory allocation */
void memory_debug_track_alloc(void* ptr, size_t size, const char* file, int line, 
                              const char* function, const char* allocator) {
    if (!ptr || !g_memory_debug_initialized) return;
    
    pthread_mutex_lock(&g_memory_mutex);
    
    /* Create tracking entry */
    memory_track_t* track = malloc(sizeof(memory_track_t));
    if (!track) {
        LOG_ERROR("Failed to allocate memory tracking entry");
        pthread_mutex_unlock(&g_memory_mutex);
        return;
    }
    
    track->ptr = ptr;
    track->size = size;
    track->file = file;
    track->line = line;
    track->function = function;
    track->allocator = allocator;
    track->next = g_memory_list;
    g_memory_list = track;
    
    /* Update statistics */
    g_memory_stats.total_allocations++;
    g_memory_stats.current_allocations++;
    g_memory_stats.total_bytes_allocated += size;
    g_memory_stats.current_bytes_allocated += size;
    
    if (g_memory_stats.current_allocations > g_memory_stats.peak_allocations) {
        g_memory_stats.peak_allocations = g_memory_stats.current_allocations;
    }
    if (g_memory_stats.current_bytes_allocated > g_memory_stats.peak_bytes_allocated) {
        g_memory_stats.peak_bytes_allocated = g_memory_stats.current_bytes_allocated;
    }
    
    /* Track allocator type */
    if (strcmp(allocator, "malloc") == 0) {
        g_memory_stats.malloc_allocs++;
    } else if (strcmp(allocator, "buffer_pool") == 0) {
        g_memory_stats.buffer_pool_allocs++;
    } else if (strcmp(allocator, "strdup") == 0) {
        g_memory_stats.strdup_allocs++;
    }
    
    TRACE_MEMORY("ALLOC: %p (%zu bytes) via %s at %s:%d:%s", 
                 ptr, size, allocator, file, line, function);
    
    pthread_mutex_unlock(&g_memory_mutex);
}

/* Track memory deallocation */
void memory_debug_track_free(void* ptr, const char* file, int line, 
                             const char* function, const char* deallocator) {
    if (!ptr || !g_memory_debug_initialized) return;
    
    pthread_mutex_lock(&g_memory_mutex);
    
    /* Find allocation in tracking list */
    memory_track_t* current = g_memory_list;
    memory_track_t* prev = NULL;
    
    while (current) {
        if (current->ptr == ptr) {
            /* Found the allocation */
            TRACE_MEMORY("FREE: %p (%zu bytes) via %s at %s:%d:%s (allocated via %s at %s:%d:%s)", 
                         ptr, current->size, deallocator, file, line, function,
                         current->allocator, current->file, current->line, current->function);
            
            /* Check for allocator/deallocator mismatch */
            int mismatch = 0;
            if ((strcmp(current->allocator, "malloc") == 0 || strcmp(current->allocator, "strdup") == 0) &&
                strcmp(deallocator, "free") != 0) {
                mismatch = 1;
            } else if (strcmp(current->allocator, "buffer_pool") == 0 &&
                       strcmp(deallocator, "buffer_pool_free") != 0) {
                mismatch = 1;
            }
            
            if (mismatch) {
                LOG_ERROR("MEMORY MISMATCH: Allocated with %s at %s:%d:%s, freed with %s at %s:%d:%s",
                          current->allocator, current->file, current->line, current->function,
                          deallocator, file, line, function);
                g_memory_stats.mismatched_frees++;
            }
            
            /* Update statistics */
            g_memory_stats.total_deallocations++;
            g_memory_stats.current_allocations--;
            g_memory_stats.current_bytes_allocated -= current->size;
            
            /* Remove from list */
            if (prev) {
                prev->next = current->next;
            } else {
                g_memory_list = current->next;
            }
            free(current);
            
            pthread_mutex_unlock(&g_memory_mutex);
            return;
        }
        prev = current;
        current = current->next;
    }
    
    /* Pointer not found - potential double-free or untracked allocation */
    LOG_WARNING("Attempting to free untracked pointer %p via %s at %s:%d:%s", 
                ptr, deallocator, file, line, function);
    
    pthread_mutex_unlock(&g_memory_mutex);
}

/* Check for allocation/deallocation mismatches */
void memory_debug_check_mismatches(void) {
    if (!g_memory_debug_initialized) return;
    
    pthread_mutex_lock(&g_memory_mutex);
    
    if (g_memory_stats.mismatched_frees > 0) {
        LOG_ERROR("Memory debug: Found %zu allocation/deallocation mismatches!", 
                  g_memory_stats.mismatched_frees);
    }
    
    /* Check for memory leaks */
    if (g_memory_stats.current_allocations > 0) {
        LOG_WARNING("Memory debug: %zu allocations still active (potential memory leaks)", 
                    g_memory_stats.current_allocations);
        
        /* List first few leaked allocations */
        memory_track_t* current = g_memory_list;
        int count = 0;
        while (current && count < 10) {
            LOG_WARNING("Leaked allocation: %p (%zu bytes) via %s at %s:%d:%s",
                        current->ptr, current->size, current->allocator,
                        current->file, current->line, current->function);
            current = current->next;
            count++;
        }
        if (g_memory_stats.current_allocations > 10) {
            LOG_WARNING("... and %zu more leaked allocations", 
                        g_memory_stats.current_allocations - 10);
        }
    }
    
    pthread_mutex_unlock(&g_memory_mutex);
}

/* Print memory usage summary */
void memory_debug_print_summary(void) {
    if (!g_memory_debug_initialized) return;
    
    pthread_mutex_lock(&g_memory_mutex);
    
    LOG_INFO("=== Memory Debug Summary ===");
    LOG_INFO("Total allocations: %zu", g_memory_stats.total_allocations);
    LOG_INFO("Total deallocations: %zu", g_memory_stats.total_deallocations);
    LOG_INFO("Current allocations: %zu", g_memory_stats.current_allocations);
    LOG_INFO("Peak allocations: %zu", g_memory_stats.peak_allocations);
    LOG_INFO("Total bytes allocated: %zu", g_memory_stats.total_bytes_allocated);
    LOG_INFO("Current bytes allocated: %zu", g_memory_stats.current_bytes_allocated);
    LOG_INFO("Peak bytes allocated: %zu", g_memory_stats.peak_bytes_allocated);
    LOG_INFO("Malloc allocations: %zu", g_memory_stats.malloc_allocs);
    LOG_INFO("Buffer pool allocations: %zu", g_memory_stats.buffer_pool_allocs);
    LOG_INFO("Strdup allocations: %zu", g_memory_stats.strdup_allocs);
    LOG_INFO("Mismatched frees: %zu", g_memory_stats.mismatched_frees);
    LOG_INFO("=============================");
    
    pthread_mutex_unlock(&g_memory_mutex);
}

#endif /* MEMORY_DEBUG_ENABLED */