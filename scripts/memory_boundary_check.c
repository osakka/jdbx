/*
 * JDBX Memory Boundary Detection System
 * Comprehensive memory corruption detection for exotic allocators
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <signal.h>
#include <execinfo.h>
#include <unistd.h>

/* Memory boundary detection constants */
#define BOUNDARY_MAGIC_START  0xDEADBEEFCAFEBABE
#define BOUNDARY_MAGIC_END    0xFEEDFACEDEADC0DE
#define BOUNDARY_SIZE         16
#define MAX_ALLOCATIONS       10000
#define BACKTRACE_SIZE        16

/* Allocation tracking structure */
typedef struct allocation_info {
    void* user_ptr;
    void* actual_ptr;
    size_t size;
    uint64_t magic_start;
    uint64_t magic_end;
    void* backtrace[BACKTRACE_SIZE];
    int backtrace_size;
    struct allocation_info* next;
} allocation_info_t;

/* Global allocation tracking */
static allocation_info_t* allocations = NULL;
static int allocation_count = 0;
static int corruption_detected = 0;

/* Signal handler for segfaults */
void segfault_handler(int sig) {
    void *array[BACKTRACE_SIZE];
    size_t size;
    
    fprintf(stderr, "\n🚨 SEGMENTATION FAULT DETECTED\n");
    fprintf(stderr, "Signal: %d\n", sig);
    
    /* Get backtrace */
    size = backtrace(array, BACKTRACE_SIZE);
    fprintf(stderr, "Backtrace:\n");
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    
    /* Check for memory corruption */
    check_all_boundaries();
    
    exit(1);
}

/* Initialize boundary detection */
void init_boundary_detection() {
    signal(SIGSEGV, segfault_handler);
    signal(SIGABRT, segfault_handler);
    fprintf(stderr, "🔒 Memory boundary detection initialized\n");
}

/* Find allocation info by user pointer */
allocation_info_t* find_allocation(void* ptr) {
    allocation_info_t* current = allocations;
    while (current) {
        if (current->user_ptr == ptr) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

/* Check boundary integrity for single allocation */
int check_allocation_boundaries(allocation_info_t* alloc) {
    if (!alloc) return 0;
    
    /* Check start boundary */
    uint64_t* start_magic = (uint64_t*)((char*)alloc->actual_ptr);
    if (*start_magic != BOUNDARY_MAGIC_START) {
        fprintf(stderr, "🚨 CORRUPTION: Start boundary violated at %p\n", alloc->user_ptr);
        fprintf(stderr, "   Expected: 0x%lx, Found: 0x%lx\n", 
                BOUNDARY_MAGIC_START, *start_magic);
        return 1;
    }
    
    /* Check end boundary */
    uint64_t* end_magic = (uint64_t*)((char*)alloc->user_ptr + alloc->size);
    if (*end_magic != BOUNDARY_MAGIC_END) {
        fprintf(stderr, "🚨 CORRUPTION: End boundary violated at %p\n", alloc->user_ptr);
        fprintf(stderr, "   Expected: 0x%lx, Found: 0x%lx\n", 
                BOUNDARY_MAGIC_END, *end_magic);
        return 1;
    }
    
    return 0;
}

/* Check all allocation boundaries */
void check_all_boundaries() {
    allocation_info_t* current = allocations;
    int corruptions = 0;
    
    while (current) {
        if (check_allocation_boundaries(current)) {
            corruptions++;
            
            /* Print allocation details */
            fprintf(stderr, "   Allocation: ptr=%p, size=%zu\n", 
                    current->user_ptr, current->size);
            fprintf(stderr, "   Backtrace when allocated:\n");
            backtrace_symbols_fd(current->backtrace, current->backtrace_size, STDERR_FILENO);
        }
        current = current->next;
    }
    
    if (corruptions > 0) {
        corruption_detected = 1;
        fprintf(stderr, "🚨 Total corruptions detected: %d\n", corruptions);
    }
}

/* Protected malloc with boundary detection */
void* protected_malloc(size_t size) {
    if (allocation_count >= MAX_ALLOCATIONS) {
        fprintf(stderr, "⚠️ Maximum allocations reached\n");
        return malloc(size);  /* Fallback to regular malloc */
    }
    
    /* Allocate with boundaries: [START_MAGIC][USER_DATA][END_MAGIC] */
    size_t total_size = BOUNDARY_SIZE + size + BOUNDARY_SIZE;
    void* actual_ptr = malloc(total_size);
    if (!actual_ptr) return NULL;
    
    /* Set up boundaries */
    uint64_t* start_magic = (uint64_t*)actual_ptr;
    *start_magic = BOUNDARY_MAGIC_START;
    
    void* user_ptr = (char*)actual_ptr + BOUNDARY_SIZE;
    
    uint64_t* end_magic = (uint64_t*)((char*)user_ptr + size);
    *end_magic = BOUNDARY_MAGIC_END;
    
    /* Track allocation */
    allocation_info_t* info = malloc(sizeof(allocation_info_t));
    if (info) {
        info->user_ptr = user_ptr;
        info->actual_ptr = actual_ptr;
        info->size = size;
        info->magic_start = BOUNDARY_MAGIC_START;
        info->magic_end = BOUNDARY_MAGIC_END;
        
        /* Capture backtrace */
        info->backtrace_size = backtrace(info->backtrace, BACKTRACE_SIZE);
        
        /* Add to list */
        info->next = allocations;
        allocations = info;
        allocation_count++;
    }
    
    return user_ptr;
}

/* Protected free with boundary checking */
void protected_free(void* ptr) {
    if (!ptr) return;
    
    allocation_info_t* alloc = find_allocation(ptr);
    if (!alloc) {
        /* Not a protected allocation, use regular free */
        free(ptr);
        return;
    }
    
    /* Check boundaries before freeing */
    if (check_allocation_boundaries(alloc)) {
        fprintf(stderr, "🚨 CORRUPTION detected during free of %p\n", ptr);
    }
    
    /* Remove from tracking list */
    allocation_info_t** current = &allocations;
    while (*current) {
        if (*current == alloc) {
            *current = alloc->next;
            break;
        }
        current = &(*current)->next;
    }
    
    /* Free actual memory */
    free(alloc->actual_ptr);
    free(alloc);
    allocation_count--;
}

/* Protected realloc with boundary detection */
void* protected_realloc(void* ptr, size_t new_size) {
    if (!ptr) return protected_malloc(new_size);
    
    allocation_info_t* alloc = find_allocation(ptr);
    if (!alloc) {
        /* Not a protected allocation, use regular realloc */
        return realloc(ptr, new_size);
    }
    
    /* Check boundaries before realloc */
    check_allocation_boundaries(alloc);
    
    /* Allocate new protected memory */
    void* new_ptr = protected_malloc(new_size);
    if (!new_ptr) return NULL;
    
    /* Copy data */
    size_t copy_size = (new_size < alloc->size) ? new_size : alloc->size;
    memcpy(new_ptr, ptr, copy_size);
    
    /* Free old memory */
    protected_free(ptr);
    
    return new_ptr;
}

/* Generate comprehensive memory report */
void generate_memory_report() {
    FILE* report = fopen("/opt/jdbx/build/diagnostics/memory_boundary_report.txt", "w");
    if (!report) {
        fprintf(stderr, "Failed to create memory report\n");
        return;
    }
    
    fprintf(report, "JDBX Memory Boundary Detection Report\n");
    fprintf(report, "=====================================\n");
    fprintf(report, "Active allocations: %d\n", allocation_count);
    fprintf(report, "Corruptions detected: %s\n", corruption_detected ? "YES" : "NO");
    fprintf(report, "\n");
    
    if (allocation_count > 0) {
        fprintf(report, "Active Allocations:\n");
        allocation_info_t* current = allocations;
        int index = 0;
        
        while (current) {
            fprintf(report, "%d. ptr=%p, size=%zu\n", ++index, current->user_ptr, current->size);
            
            /* Check this allocation */
            if (check_allocation_boundaries(current)) {
                fprintf(report, "   ❌ CORRUPTED\n");
            } else {
                fprintf(report, "   ✅ INTACT\n");
            }
            
            current = current->next;
        }
    }
    
    fclose(report);
    fprintf(stderr, "📊 Memory report generated: memory_boundary_report.txt\n");
}

/* Test boundary detection system */
int main() {
    init_boundary_detection();
    
    fprintf(stderr, "🧪 Testing memory boundary detection system\n");
    
    /* Test 1: Normal allocation */
    void* ptr1 = protected_malloc(100);
    fprintf(stderr, "✅ Normal allocation: %p\n", ptr1);
    
    /* Test 2: Write within bounds */
    strcpy((char*)ptr1, "Hello, World!");
    check_all_boundaries();
    
    /* Test 3: Deliberate overflow (should be detected) */
    fprintf(stderr, "🧪 Testing deliberate buffer overflow...\n");
    strcpy((char*)ptr1 + 90, "This string is too long and will overflow the buffer boundary!");
    check_all_boundaries();
    
    /* Test 4: Free with corruption check */
    protected_free(ptr1);
    
    generate_memory_report();
    
    fprintf(stderr, "✅ Boundary detection test complete\n");
    return corruption_detected ? 1 : 0;
}