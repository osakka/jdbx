# JDBX Threading Guidelines

## Overview

JDBX implements a **Unified Threading Model** that guarantees 100% reliability under concurrent load. This document provides guidelines for maintaining thread safety across the codebase.

## Core Principles

### 1. Single Source of Truth
- **One threading pattern per use case** - no parallel implementations
- **Consistent mutex usage** across all components
- **Unified atomic operations** for counters and statistics
- **Standard JSON string storage** for thread-safe document access

### 2. Thread Safety Guarantees

| Component | Thread Safety Mechanism | Use Case |
|-----------|-------------------------|----------|
| Document Storage | JSON string serialization | Eliminates shared JSON object access |
| Rate Limiting | pthread_mutex_t | Token bucket operations |
| JavaScript Engine | pthread_mutex_t | Script execution serialization |
| Reference Counting | C11 atomic operations | Lock-free counters |
| SSL Operations | pthread_mutex_t | SSL object creation |
| File Cache | Static mutex | Global cache protection |

### 3. Memory Safety
- All thread-shared data managed by JDBX memory manager
- JSON objects stored as strings to prevent concurrent access
- Automatic cleanup via checkpoint system
- Hazard pointer protection for lock-free structures

## Implementation Patterns

### JSON Document Storage (CRITICAL)

**DO**: Store JSON as serialized strings in skiplist
```c
// CORRECT: Thread-safe JSON string storage
char* doc_str = json_stringify(document);
size_t str_len = strlen(doc_str) + 1;
char* stored_str = (char*)BUFFER_ALLOC(str_len);
memcpy(stored_str, doc_str, str_len);
skiplist_insert(coll->documents, uuid, strlen(uuid) + 1, stored_str, str_len);
```

**DON'T**: Store JSON object pointers (causes race conditions)
```c
// WRONG: Race condition - multiple threads access same object
json_value_t** doc_ptr = (json_value_t**)BUFFER_ALLOC(sizeof(json_value_t*));
*doc_ptr = document;  // DANGER: Shared object access
skiplist_insert(coll->documents, uuid, strlen(uuid) + 1, doc_ptr, sizeof(json_value_t*));
```

### Mutex Protection

**DO**: Add mutex to structure and protect all operations
```c
// Structure definition
typedef struct {
    pthread_mutex_t mutex;  // Thread safety mutex
    database_t* db;
    // ... other fields
} component_t;

// Initialization
if (pthread_mutex_init(&component->mutex, NULL) != 0) {
    // Handle error
}

// Operation protection
pthread_mutex_lock(&component->mutex);
// Critical section
pthread_mutex_unlock(&component->mutex);

// Cleanup
pthread_mutex_destroy(&component->mutex);
```

**DON'T**: Use global mutexes or unprotected access
```c
// WRONG: Global mutex creates bottleneck
static pthread_mutex_t global_mutex = PTHREAD_MUTEX_INITIALIZER;

// WRONG: Unprotected access
component->shared_data = new_value;  // Race condition
```

### Atomic Operations

**DO**: Use C11 atomics for simple counters
```c
#include <stdatomic.h>

typedef struct {
    atomic_size_t ref_count;  // Atomic counter
} ref_counted_t;

// Atomic operations
size_t new_count = atomic_fetch_add(&obj->ref_count, 1) + 1;
size_t old_count = atomic_fetch_sub(&obj->ref_count, 1) - 1;
```

**DON'T**: Use non-atomic operations for shared counters
```c
// WRONG: Race condition in increment
obj->ref_count++;  // Not thread-safe
```

### TOCTOU Prevention

**DO**: Hold lock during entire check-and-act sequence
```c
pthread_mutex_lock(&component->mutex);
if (condition_check()) {
    perform_action();  // Atomic with check
}
pthread_mutex_unlock(&component->mutex);
```

**DON'T**: Check outside lock, act inside lock
```c
// WRONG: Time-of-check-time-of-use vulnerability
if (condition_check()) {  // Check without lock
    pthread_mutex_lock(&component->mutex);
    perform_action();     // Race window existed
    pthread_mutex_unlock(&component->mutex);
}
```

## Component-Specific Guidelines

### Database Operations
- **Always** parse JSON strings from skiplist (never store pointers)
- **Use** pthread_rwlock_t for collection-level access control
- **Promote** long-lived allocations with memory_promote()
- **Clean up** parsed JSON with checkpoint comments

### Rate Limiting
- **Protect** all token bucket operations with mutex
- **Use** database-backed state for persistence
- **Implement** automatic cleanup for expired entries
- **Ensure** atomic check-and-consume operations

### JavaScript Engine
- **Serialize** script execution with mutex
- **Use** thread-local contexts if available
- **Protect** global engine state
- **Handle** errors gracefully without leaving locks held

### SSL/TLS Operations
- **Protect** SSL_new() calls with mutex
- **Use** proper SSL cleanup sequences
- **Handle** connection state transitions safely
- **Avoid** SSL operations after socket close

## Testing Requirements

### Concurrent Testing
Every threading change MUST be validated with concurrent tests:

```bash
# Run threading validation
python3 tests/concurrent/test_direct_storage.py

# Expected result: 100% success rate
🎯 HEALTH CHECK CONCURRENT TEST: 50 operations
✅✅✅✅✅✅✅✅✅✅ (10/50)
🏁 FINAL RESULTS: 50/50 successful (100.0%)
```

### Stress Testing
- **Load Testing**: 50+ concurrent operations
- **Duration Testing**: Sustained load for 5+ minutes  
- **Memory Testing**: Monitor for leaks and corruption
- **Error Testing**: Inject failures during concurrent operations

## Debugging Threading Issues

### Common Symptoms
- **Segmentation Faults**: Often indicates race conditions in JSON access
- **JSON Corruption**: "Corrupted JSON type" errors from concurrent access
- **Deadlocks**: Mutex acquisition order issues
- **Memory Leaks**: Unreleased locks or atomic cleanup issues

### Debugging Tools
```bash
# Memory safety
valgrind --tool=helgrind ./bin/jdbxd

# Thread safety
gdb --batch --ex run --ex bt --args ./bin/jdbxd

# Performance impact
perf record -g ./bin/jdbxd
```

### Log Analysis
- **Enable TRACE level**: For detailed threading logs
- **Monitor patterns**: Look for "CRITICAL THREAD SAFETY" messages
- **Check metrics**: Monitor active_connections and response_times
- **Validate cleanup**: Ensure proper resource deallocation

## Performance Considerations

### Lock Granularity
- **Fine-grained**: Component-level mutexes (preferred)
- **Avoid**: Global locks that create bottlenecks
- **Short sections**: Minimize time holding locks
- **No I/O**: Never perform I/O while holding locks

### Memory Access Patterns
- **Cache-friendly**: Align atomic variables to cache lines
- **Minimize sharing**: Reduce false sharing between threads
- **Local buffers**: Use thread-local storage where possible
- **Memory barriers**: Use appropriate synchronization primitives

## Migration Guide

### Converting Existing Code

1. **Identify shared state**: Find variables accessed by multiple threads
2. **Choose protection**: Mutex for complex operations, atomics for counters
3. **Update structures**: Add mutex to component structures
4. **Protect operations**: Wrap critical sections with locks
5. **Test thoroughly**: Validate with concurrent testing

### JSON Storage Migration

1. **Find JSON pointers**: Search for `json_value_t*` in skiplist operations
2. **Add stringify**: Convert to JSON strings before storage
3. **Update retrieval**: Parse JSON strings instead of dereferencing pointers
4. **Add cleanup**: Use checkpoint comments for parsed JSON
5. **Verify safety**: Test under concurrent load

## Compliance Checklist

- [ ] No JSON object pointers stored in skiplist
- [ ] All shared state protected by appropriate synchronization
- [ ] Atomic operations used for simple counters
- [ ] No TOCTOU vulnerabilities in check-then-act patterns
- [ ] Mutex initialization and cleanup properly handled
- [ ] Thread safety tested under concurrent load
- [ ] Memory safety verified with appropriate tools
- [ ] Performance impact measured and acceptable
- [ ] Documentation updated for any new patterns
- [ ] Single source of truth maintained (no parallel implementations)

---

**Remember**: Threading bugs are often non-deterministic and may only appear under specific load conditions. Always test thoroughly with realistic concurrent workloads.

## References

- [ADR-029: Unified Threading Model](../architecture/ADR-029-unified-threading-model.md)
- [JDBX Memory Manager](../architecture/ADR-026-checkpoint-memory-manager.md)
- [Concurrent Test Suite](../../tests/concurrent/)
- [POSIX Threads Reference](https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/pthread.h.html)
- [C11 Atomics Reference](https://en.cppreference.com/w/c/atomic)