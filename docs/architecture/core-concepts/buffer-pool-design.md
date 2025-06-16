# Enterprise Buffer Pool Design for JDBX

## Core Principles
1. **SINGLE SOURCE OF TRUTH**: One implementation, no alternatives, no fallbacks
2. **Thread Safety**: Lock-free design with atomic operations where possible
3. **Memory Safety**: Guard pages, canaries, and comprehensive validation
4. **Performance**: O(1) allocation/deallocation with size classes
5. **Debugging**: Built-in memory leak detection and corruption detection
6. **Zero Regressions**: Drop-in replacement for current malloc/free wrapper

## Architecture

### Memory Layout
```
+------------------+
|  Pool Metadata   |  Global pool info, statistics
+------------------+
|  Size Class 0    |  8-byte allocations
|  [Slab 0]        |  
|  [Slab 1]        |  
|  ...             |  
+------------------+
|  Size Class 1    |  16-byte allocations  
|  [Slab 0]        |
|  ...             |
+------------------+
|  ...             |
+------------------+
|  Size Class N    |  Large allocations (>4KB)
|  [Direct mmap]   |
+------------------+
```

### Size Classes
- Class 0: 8 bytes
- Class 1: 16 bytes
- Class 2: 32 bytes
- Class 3: 64 bytes
- Class 4: 128 bytes
- Class 5: 256 bytes
- Class 6: 512 bytes
- Class 7: 1024 bytes
- Class 8: 2048 bytes
- Class 9: 4096 bytes
- Class 10+: Direct mmap (large allocations)

### Per-Allocation Metadata
```c
typedef struct alloc_header {
    uint32_t magic;          // 0xDEADBEEF - corruption detection
    uint32_t size;           // Actual allocation size
    uint32_t flags;          // Debug flags
    uint32_t thread_id;      // Allocating thread
    const char* file;        // Source file (debug mode)
    int line;                // Source line (debug mode)
    struct alloc_header* next; // Free list pointer (when free)
    uint64_t canary;         // 0xCAFEBABEDEADC0DE
} alloc_header_t;
```

### Thread-Local Caching
```c
typedef struct thread_cache {
    void* free_lists[NUM_SIZE_CLASSES];  // Per-size-class free lists
    size_t bytes_allocated;
    size_t bytes_freed;
    uint32_t thread_id;
} thread_cache_t;
```

## API Design

### Core Functions
```c
// Initialize buffer pool (called once at startup)
int buffer_pool_init(size_t initial_size, int flags);

// Shutdown buffer pool (called at exit)
void buffer_pool_shutdown(void);

// Allocation functions
void* buffer_pool_alloc(size_t size);
void* buffer_pool_alloc_debug(size_t size, const char* file, int line);
void* buffer_pool_calloc(size_t nmemb, size_t size);
void* buffer_pool_realloc(void* ptr, size_t new_size);
char* buffer_pool_strdup(const char* str);

// Deallocation
void buffer_pool_free(void* ptr);

// Statistics and debugging
void buffer_pool_stats(buffer_pool_stats_t* stats);
void buffer_pool_check_leaks(void);
int buffer_pool_validate(void* ptr);
```

### Compatibility Macros (Drop-in Replacement)
```c
#define BUFFER_ALLOC(size) buffer_pool_alloc_debug(size, __FILE__, __LINE__)
#define BUFFER_FREE(ptr) buffer_pool_free(ptr)
#define BUFFER_STRDUP(str) buffer_pool_strdup(str)
#define BUFFER_REALLOC(ptr, size) buffer_pool_realloc(ptr, size)
```

## Memory Safety Features

### 1. Magic Numbers
- Header magic: 0xDEADBEEF
- Footer canary: 0xCAFEBABEDEADC0DE
- Free magic: 0xFEEDFACE

### 2. Double-Free Protection
- Mark freed blocks with special pattern
- Validate on every free operation

### 3. Buffer Overflow Detection
- Guard pages for large allocations
- Canary values for small allocations

### 4. Use-After-Free Detection
- Poison freed memory with 0xDE pattern
- Validate pointers before operations

### 5. Memory Leak Detection
- Track all live allocations
- Report leaks on shutdown
- Optional periodic leak scans

## Thread Safety Design

### 1. Lock-Free Free Lists
- Use CAS operations for free list management
- Per-thread caches to reduce contention

### 2. Atomic Statistics
- Use atomic counters for global stats
- Thread-local stats aggregated periodically

### 3. Safe Shutdown
- Graceful handling of in-flight operations
- Proper cleanup of thread-local storage

## Performance Optimizations

### 1. Slab Allocation
- Pre-allocate slabs for common sizes
- Reduce system calls

### 2. Thread-Local Caching
- Cache freed objects per thread
- Batch returns to global pool

### 3. Size Class Alignment
- Align to cache lines where beneficial
- Minimize false sharing

### 4. Fast Path
- Inline common allocation sizes
- Skip debug checks in release mode

## Migration Strategy

### Phase 1: Implementation
1. Implement new buffer pool in separate file
2. Comprehensive unit tests
3. Stress testing with valgrind

### Phase 2: Integration
1. Replace buffer_pool.c with new implementation
2. Keep exact same API
3. Update Makefile - single source

### Phase 3: Validation
1. Run full test suite
2. Memory leak testing
3. Performance benchmarking
4. Production load testing

## Success Metrics
- Zero memory leaks
- Zero segmentation faults
- <5% performance overhead vs malloc
- 100% API compatibility
- Single implementation (no alternatives)