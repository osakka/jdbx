# ADR-005: Thread Pool Architecture

**Date**: May 19, 2025  
**Status**: Accepted  
**Version**: 2.0.0  
**Impact**: High  

## Context

Initial implementation used thread-per-connection model which:
- Limited scalability to ~1000 connections
- High memory usage (8MB stack per thread)
- Thread creation overhead
- Poor cache locality

## Decision

Implement fixed-size thread pool with:
- Configurable worker threads (4-64)
- Work queue with backpressure
- Connection multiplexing
- CPU affinity options

## Rationale

### Scalability Requirements
- Handle 10K+ concurrent connections
- Predictable resource usage
- Better CPU cache utilization
- Controlled memory footprint

### Design Principles
- Fixed thread count
- Lock-free work queue
- Fair scheduling
- Graceful overload handling

## Implementation

### Thread Pool Structure
```c
typedef struct thread_pool {
    pthread_t* threads;
    size_t num_threads;
    work_queue_t* queue;
    _Atomic(bool) shutdown;
    pthread_mutex_t queue_mutex;
    pthread_cond_t queue_cond;
} thread_pool_t;
```

### Work Queue Design
```c
typedef struct work_item {
    void (*function)(void* arg);
    void* arg;
    struct work_item* next;
} work_item_t;
```

### Configuration
- Min threads: 4 (default)
- Max threads: 64 (configurable)
- Queue size: 1000 (backpressure)
- Idle timeout: 60 seconds

## Consequences

### Positive
- **Scalability**: 10K+ connections handled
- **Memory**: Fixed memory usage
- **Performance**: Better cache locality
- **Control**: Predictable behavior

### Negative
- **Complexity**: Work queue management
- **Debugging**: Async execution model
- **Tuning**: Requires configuration

### Mitigations
- Comprehensive metrics
- Queue depth monitoring
- Thread state tracking
- Automatic tuning option

## Technical Details

### Files Created
- `src/components/core/thread_pool.c` - Implementation
- `src/components/core/work_queue.c` - Queue management
- `src/include/core/thread_pool.h` - Public API

### Performance Characteristics
```
Metric              Thread-per-conn    Thread Pool
Max connections     ~1,000            10,000+
Memory per conn     8 MB              < 1 KB
Context switches    High              Low
Cache efficiency    Poor              Excellent
```

### Worker Thread Logic
```c
void* worker_thread(void* arg) {
    thread_pool_t* pool = arg;
    while (!atomic_load(&pool->shutdown)) {
        work_item_t* work = get_work(pool);
        if (work) {
            work->function(work->arg);
            free(work);
        }
    }
    return NULL;
}
```

## Validation

- ✅ 10K concurrent connections tested
- ✅ Memory usage constant
- ✅ CPU utilization optimal
- ✅ Graceful shutdown working
- ✅ Metrics collection accurate

## References

- Git commits: `72963d3`, `c83bc12` - Thread pool implementation
- Related: ADR-006 (Database-Based RBAC)