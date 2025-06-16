/**
 * @file buffer_pool_enterprise.c
 * @brief Enterprise-grade buffer pool implementation with thread safety and memory protection
 * 
 * SINGLE SOURCE OF TRUTH: This is the ONLY buffer pool implementation in JDBX.
 * No alternatives, no fallbacks, no parallel implementations.
 * 
 * Features:
 * - Thread-safe with minimal locking
 * - Memory corruption detection
 * - Leak detection and reporting
 * - Size class optimization
 * - Debug tracking with file/line info
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include <assert.h>
#include <errno.h>

/* Magic numbers for corruption detection */
#define MAGIC_ALLOCATED 0xDEADBEEF
#define MAGIC_FREED     0xFEEDFACE
#define CANARY_VALUE    0xCAFEBABEDEADC0DEULL

/* Size classes */
#define NUM_SIZE_CLASSES 11
#define MAX_SMALL_SIZE   4096
#define SLAB_SIZE        (64 * 1024)  /* 64KB slabs */

/* Thread-local cache settings */
#define THREAD_CACHE_SIZE 32

/* Statistics */
typedef struct {
    uint64_t allocations;
    uint64_t deallocations;
    uint64_t bytes_allocated;
    uint64_t bytes_freed;
    uint64_t active_allocations;
    uint64_t peak_allocations;
    uint64_t corruptions_detected;
} pool_stats_t;

/* Allocation header */
typedef struct alloc_header {
    uint32_t magic;
    uint32_t size;
    uint32_t flags;
    uint32_t thread_id;
    const char* file;
    int line;
    struct alloc_header* next;
    struct alloc_header* prev;
    uint64_t canary;
} alloc_header_t;

/* Size class descriptor */
typedef struct size_class {
    size_t size;
    size_t slab_capacity;
    pthread_mutex_t lock;
    void* free_list;
    void* slabs;
    size_t num_slabs;
} size_class_t;

/* Thread cache */
typedef struct thread_cache {
    void* free_lists[NUM_SIZE_CLASSES];
    size_t counts[NUM_SIZE_CLASSES];
    uint32_t thread_id;
} thread_cache_t;

/* Global buffer pool */
typedef struct buffer_pool {
    size_class_t size_classes[NUM_SIZE_CLASSES];
    pthread_mutex_t global_lock;
    alloc_header_t* all_allocations;  /* Debug mode: track all allocations */
    pool_stats_t stats;
    int initialized;
    int debug_mode;
} buffer_pool_t;

/* Global instance - SINGLE SOURCE OF TRUTH */
static buffer_pool_t g_pool = {0};

/* Thread-local storage for caching */
static __thread thread_cache_t* tls_cache = NULL;

/* Size class boundaries */
static const size_t size_classes[] = {
    8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, SIZE_MAX
};

/* Forward declarations */
static int get_size_class(size_t size);
static void* alloc_from_size_class(int class_idx, size_t size);
static void free_to_size_class(void* ptr, int class_idx);
static void validate_allocation(alloc_header_t* header);
static void track_allocation(alloc_header_t* header);
static void untrack_allocation(alloc_header_t* header);

/* Initialize buffer pool */
int buffer_pool_init(size_t initial_size, int debug_mode) {
    if (g_pool.initialized) {
        return 0;  /* Already initialized */
    }
    
    pthread_mutex_init(&g_pool.global_lock, NULL);
    g_pool.debug_mode = debug_mode;
    
    /* Initialize size classes */
    for (int i = 0; i < NUM_SIZE_CLASSES - 1; i++) {
        g_pool.size_classes[i].size = size_classes[i];
        g_pool.size_classes[i].slab_capacity = SLAB_SIZE / (size_classes[i] + sizeof(alloc_header_t));
        pthread_mutex_init(&g_pool.size_classes[i].lock, NULL);
    }
    
    g_pool.initialized = 1;
    return 0;
}

/* Shutdown buffer pool */
void buffer_pool_shutdown(void) {
    if (!g_pool.initialized) return;
    
    /* Report statistics */
    fprintf(stderr, "\n=== Buffer Pool Statistics ===\n");
    fprintf(stderr, "Total allocations: %lu\n", g_pool.stats.allocations);
    fprintf(stderr, "Total deallocations: %lu\n", g_pool.stats.deallocations);
    fprintf(stderr, "Bytes allocated: %lu\n", g_pool.stats.bytes_allocated);
    fprintf(stderr, "Bytes freed: %lu\n", g_pool.stats.bytes_freed);
    fprintf(stderr, "Active allocations: %lu\n", g_pool.stats.active_allocations);
    fprintf(stderr, "Peak allocations: %lu\n", g_pool.stats.peak_allocations);
    fprintf(stderr, "Corruptions detected: %lu\n", g_pool.stats.corruptions_detected);
    
    /* Check for leaks */
    if (g_pool.stats.active_allocations > 0) {
        fprintf(stderr, "\nWARNING: %lu memory leaks detected!\n", g_pool.stats.active_allocations);
        
        if (g_pool.debug_mode && g_pool.all_allocations) {
            fprintf(stderr, "\nLeak details:\n");
            alloc_header_t* header = g_pool.all_allocations;
            int count = 0;
            while (header && count < 20) {  /* Show first 20 leaks */
                fprintf(stderr, "  Leak %d: %u bytes at %s:%d\n", 
                    ++count, header->size, header->file ?: "unknown", header->line);
                header = header->next;
            }
            if (g_pool.stats.active_allocations > 20) {
                fprintf(stderr, "  ... and %lu more leaks\n", g_pool.stats.active_allocations - 20);
            }
        }
    }
    
    /* Clean up */
    for (int i = 0; i < NUM_SIZE_CLASSES - 1; i++) {
        pthread_mutex_destroy(&g_pool.size_classes[i].lock);
    }
    pthread_mutex_destroy(&g_pool.global_lock);
    
    g_pool.initialized = 0;
}

/* Allocate memory */
void* buffer_pool_alloc_safe(size_t size, const char* file, int line, const char* func) {
    (void)func;  /* Unused but kept for API compatibility */
    
    if (!g_pool.initialized) {
        buffer_pool_init(0, 1);  /* Auto-initialize in debug mode */
    }
    
    if (size == 0) return NULL;
    
    /* Add space for header */
    size_t total_size = size + sizeof(alloc_header_t);
    int class_idx = get_size_class(total_size);
    
    void* ptr;
    if (class_idx < NUM_SIZE_CLASSES - 1) {
        /* Small allocation from size class */
        ptr = alloc_from_size_class(class_idx, total_size);
    } else {
        /* Large allocation - direct mmap */
        ptr = mmap(NULL, total_size, PROT_READ | PROT_WRITE, 
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (ptr == MAP_FAILED) return NULL;
    }
    
    if (!ptr) return NULL;
    
    /* Initialize header */
    alloc_header_t* header = (alloc_header_t*)ptr;
    header->magic = MAGIC_ALLOCATED;
    header->size = size;
    header->flags = 0;
    header->thread_id = (uint32_t)pthread_self();
    header->file = file;
    header->line = line;
    header->next = NULL;
    header->prev = NULL;
    header->canary = CANARY_VALUE;
    
    /* Update statistics */
    __sync_fetch_and_add(&g_pool.stats.allocations, 1);
    __sync_fetch_and_add(&g_pool.stats.bytes_allocated, size);
    __sync_fetch_and_add(&g_pool.stats.active_allocations, 1);
    
    uint64_t current = g_pool.stats.active_allocations;
    uint64_t peak = g_pool.stats.peak_allocations;
    while (current > peak) {
        if (__sync_bool_compare_and_swap(&g_pool.stats.peak_allocations, peak, current)) {
            break;
        }
        peak = g_pool.stats.peak_allocations;
    }
    
    /* Track allocation in debug mode */
    if (g_pool.debug_mode) {
        track_allocation(header);
    }
    
    /* Return user pointer (after header) */
    return (char*)ptr + sizeof(alloc_header_t);
}

/* Free memory */
void buffer_pool_free_safe(void* ptr, const char* file, int line, const char* func) {
    (void)file; (void)line; (void)func;  /* Unused but kept for API compatibility */
    
    if (!ptr) return;
    
    /* Get header */
    alloc_header_t* header = (alloc_header_t*)((char*)ptr - sizeof(alloc_header_t));
    
    /* Validate allocation */
    validate_allocation(header);
    
    /* Check for double-free */
    if (header->magic == MAGIC_FREED) {
        fprintf(stderr, "ERROR: Double-free detected at %p\n", ptr);
        __sync_fetch_and_add(&g_pool.stats.corruptions_detected, 1);
        abort();
    }
    
    /* Update statistics */
    __sync_fetch_and_add(&g_pool.stats.deallocations, 1);
    __sync_fetch_and_add(&g_pool.stats.bytes_freed, header->size);
    __sync_sub_and_fetch(&g_pool.stats.active_allocations, 1);
    
    /* Untrack allocation in debug mode */
    if (g_pool.debug_mode) {
        untrack_allocation(header);
    }
    
    /* Mark as freed */
    header->magic = MAGIC_FREED;
    
    /* Poison memory to detect use-after-free */
    memset(ptr, 0xDE, header->size);
    
    /* Return to pool or unmap */
    size_t total_size = header->size + sizeof(alloc_header_t);
    int class_idx = get_size_class(total_size);
    
    if (class_idx < NUM_SIZE_CLASSES - 1) {
        free_to_size_class(header, class_idx);
    } else {
        munmap(header, total_size);
    }
}

/* String duplication */
char* buffer_pool_strdup(const char* str) {
    if (!str) return NULL;
    
    size_t len = strlen(str) + 1;
    char* copy = buffer_pool_alloc_safe(len, __FILE__, __LINE__, __func__);
    if (copy) {
        memcpy(copy, str, len);
    }
    return copy;
}

/* Reallocation */
void* buffer_pool_realloc(void* ptr, size_t new_size) {
    if (!ptr) return buffer_pool_alloc_safe(new_size, __FILE__, __LINE__, __func__);
    if (new_size == 0) {
        buffer_pool_free_safe(ptr, __FILE__, __LINE__, __func__);
        return NULL;
    }
    
    /* Get old size */
    alloc_header_t* header = (alloc_header_t*)((char*)ptr - sizeof(alloc_header_t));
    validate_allocation(header);
    
    size_t old_size = header->size;
    if (new_size <= old_size) {
        return ptr;  /* Already big enough */
    }
    
    /* Allocate new block */
    void* new_ptr = buffer_pool_alloc_safe(new_size, header->file, header->line, __func__);
    if (!new_ptr) return NULL;
    
    /* Copy data */
    memcpy(new_ptr, ptr, old_size);
    
    /* Free old block */
    buffer_pool_free_safe(ptr, __FILE__, __LINE__, __func__);
    
    return new_ptr;
}

/* Get size class index */
static int get_size_class(size_t size) {
    for (int i = 0; i < NUM_SIZE_CLASSES; i++) {
        if (size <= size_classes[i]) {
            return i;
        }
    }
    return NUM_SIZE_CLASSES - 1;
}

/* Allocate from size class */
static void* alloc_from_size_class(int class_idx, size_t size) {
    (void)size;  /* Size already includes header */
    
    size_class_t* sc = &g_pool.size_classes[class_idx];
    
    pthread_mutex_lock(&sc->lock);
    
    /* Check free list */
    if (sc->free_list) {
        void* ptr = sc->free_list;
        sc->free_list = *(void**)ptr;
        pthread_mutex_unlock(&sc->lock);
        return ptr;
    }
    
    /* Allocate new slab if needed */
    void* slab = mmap(NULL, SLAB_SIZE, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (slab == MAP_FAILED) {
        pthread_mutex_unlock(&sc->lock);
        return NULL;
    }
    
    /* Initialize slab free list */
    size_t obj_size = sc->size + sizeof(alloc_header_t);
    size_t count = SLAB_SIZE / obj_size;
    
    char* ptr = slab;
    for (size_t i = 0; i < count - 1; i++) {
        *(void**)ptr = ptr + obj_size;
        ptr += obj_size;
    }
    *(void**)ptr = NULL;
    
    /* Take first object, rest go to free list */
    void* result = slab;
    sc->free_list = (char*)slab + obj_size;
    sc->num_slabs++;
    
    pthread_mutex_unlock(&sc->lock);
    return result;
}

/* Return to size class */
static void free_to_size_class(void* ptr, int class_idx) {
    size_class_t* sc = &g_pool.size_classes[class_idx];
    
    pthread_mutex_lock(&sc->lock);
    *(void**)ptr = sc->free_list;
    sc->free_list = ptr;
    pthread_mutex_unlock(&sc->lock);
}

/* Validate allocation */
static void validate_allocation(alloc_header_t* header) {
    if (header->magic != MAGIC_ALLOCATED) {
        fprintf(stderr, "ERROR: Invalid magic number: %x (expected %x)\n", 
                header->magic, MAGIC_ALLOCATED);
        __sync_fetch_and_add(&g_pool.stats.corruptions_detected, 1);
        abort();
    }
    
    if (header->canary != CANARY_VALUE) {
        fprintf(stderr, "ERROR: Buffer overflow detected - canary corrupted\n");
        __sync_fetch_and_add(&g_pool.stats.corruptions_detected, 1);
        abort();
    }
}

/* Track allocation for leak detection */
static void track_allocation(alloc_header_t* header) {
    pthread_mutex_lock(&g_pool.global_lock);
    
    header->prev = NULL;
    header->next = g_pool.all_allocations;
    if (g_pool.all_allocations) {
        g_pool.all_allocations->prev = header;
    }
    g_pool.all_allocations = header;
    
    pthread_mutex_unlock(&g_pool.global_lock);
}

/* Untrack allocation */
static void untrack_allocation(alloc_header_t* header) {
    pthread_mutex_lock(&g_pool.global_lock);
    
    if (header->prev) {
        header->prev->next = header->next;
    } else {
        g_pool.all_allocations = header->next;
    }
    
    if (header->next) {
        header->next->prev = header->prev;
    }
    
    pthread_mutex_unlock(&g_pool.global_lock);
}

/* Compatibility wrappers for exact API match */
void* buffer_pool_alloc(size_t size) {
    return buffer_pool_alloc_safe(size, "unknown", 0, "unknown");
}

void buffer_pool_free(void* ptr) {
    buffer_pool_free_safe(ptr, "unknown", 0, "unknown");
}

/* Additional compatibility functions */
char* buffer_pool_strdup_safe(const char* str, const char* file, int line, const char* func) {
    if (!str) return NULL;
    
    size_t len = strlen(str) + 1;
    char* copy = buffer_pool_alloc_safe(len, file, line, func);
    if (copy) {
        memcpy(copy, str, len);
    }
    return copy;
}

void* buffer_pool_realloc_safe(void* ptr, size_t new_size, const char* file, int line, const char* func) {
    if (!ptr) return buffer_pool_alloc_safe(new_size, file, line, func);
    if (new_size == 0) {
        buffer_pool_free_safe(ptr, file, line, func);
        return NULL;
    }
    
    /* Get old size */
    alloc_header_t* header = (alloc_header_t*)((char*)ptr - sizeof(alloc_header_t));
    validate_allocation(header);
    
    size_t old_size = header->size;
    if (new_size <= old_size) {
        return ptr;  /* Already big enough */
    }
    
    /* Allocate new block */
    void* new_ptr = buffer_pool_alloc_safe(new_size, file, line, func);
    if (!new_ptr) return NULL;
    
    /* Copy data */
    memcpy(new_ptr, ptr, old_size);
    
    /* Free old block */
    buffer_pool_free_safe(ptr, file, line, func);
    
    return new_ptr;
}

/* Initialize function for compatibility */
void buffer_pool_init_metrics(void) {
    /* Auto-initialized on first use */
}

/* Get statistics */
void buffer_pool_get_stats(uint64_t* total_allocs, uint64_t* total_frees, uint64_t* bytes_in_use) {
    if (total_allocs) *total_allocs = g_pool.stats.allocations;
    if (total_frees) *total_frees = g_pool.stats.deallocations;
    if (bytes_in_use) *bytes_in_use = g_pool.stats.bytes_allocated - g_pool.stats.bytes_freed;
}