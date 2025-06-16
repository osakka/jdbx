#ifndef MEMORY_MONITORING_H
#define MEMORY_MONITORING_H

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "utils/logger.h"
#include "utils/buffer_pool.h"

/* Memory debugging - always available, controlled dynamically via TRACE_MEMORY */

/* Memory allocation tracking */
typedef struct memory_track {
    void* ptr;
    size_t size;
    const char* file;
    int line;
    const char* function;
    const char* allocator; /* "malloc", "buffer_pool", "strdup" */
    struct memory_track* next;
} memory_track_t;

/* Initialize memory debugging */
void memory_debug_init(void);

/* Cleanup memory debugging */
void memory_debug_cleanup(void);

/* Track memory allocation - uses TRACE_MEMORY */
void memory_debug_track_alloc(void* ptr, size_t size, const char* file, int line, 
                              const char* function, const char* allocator);

/* Track memory deallocation - uses TRACE_MEMORY */
void memory_debug_track_free(void* ptr, const char* file, int line, 
                             const char* function, const char* deallocator);

/* Check for allocation/deallocation mismatches */
void memory_debug_check_mismatches(void);

/* Print memory usage summary */
void memory_debug_print_summary(void);

/* Thread-safe memory debugging enabled check */
int memory_debug_enabled(void);

/* Smart wrapper macros - only track when TRACE_MEMORY is enabled */
#define DEBUG_MALLOC(size) \
    ({ void* __ptr =BUFFER_ALLOC(size); \
       if (logger_trace_enabled(TRACE_MEMORY)) { \
           memory_debug_track_alloc(__ptr, size, __FILE__, __LINE__, __func__, "malloc"); \
       } \
       __ptr; })

#define DEBUG_FREE(ptr) \
    do { \
        if (logger_trace_enabled(TRACE_MEMORY)) { \
            memory_debug_track_free(ptr, __FILE__, __LINE__, __func__, "free"); \
        } \
        BUFFER_FREE(ptr); \
    } while(0)

#define DEBUG_BUFFER_POOL_ALLOC(size) \
    ({ void* __ptr = buffer_pool_alloc(size); \
       if (logger_trace_enabled(TRACE_MEMORY)) { \
           memory_debug_track_alloc(__ptr, size, __FILE__, __LINE__, __func__, "buffer_pool"); \
       } \
       __ptr; })

#define DEBUG_BUFFER_POOL_FREE(ptr) \
    do { \
        if (logger_trace_enabled(TRACE_MEMORY)) { \
            memory_debug_track_free(ptr, __FILE__, __LINE__, __func__, "buffer_pool_free"); \
        } \
        buffer_pool_free(ptr); \
    } while(0)

#define DEBUG_STRDUP(str) \
    ({ char* __ptr = BUFFER_STRDUP(str); \
       if (logger_trace_enabled(TRACE_MEMORY)) { \
           memory_debug_track_alloc(__ptr, strlen(str) + 1, __FILE__, __LINE__, __func__, "strdup"); \
       } \
       __ptr; })

/* Always provide memory debugging capability - controlled at runtime */

#endif /* MEMORY_MONITORING_H */
