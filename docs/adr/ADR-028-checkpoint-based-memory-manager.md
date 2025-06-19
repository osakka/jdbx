# ADR-028: Checkpoint-Based Memory Manager

**Status**: Accepted  
**Date**: 2025-06-16  
**Authors**: JDBX Development Team  
**Reviewers**: Architecture Committee  

## Context

Manual memory management in a high-concurrency server with complex transaction boundaries was becoming error-prone and difficult to maintain. The codebase had:

- 304 allocation calls across 44 files using different patterns
- Complex error path cleanup requiring careful manual free() calls
- Memory leaks on error conditions due to missed cleanup
- Thread safety concerns with manual memory management
- Transaction boundary complexity with nested allocations

## Problem

### Manual Memory Management Issues

1. **Error Path Complexity**: Every error exit required careful cleanup of all allocations
2. **Memory Leaks**: Missed cleanup on error paths caused production memory leaks
3. **Double Free**: Complex control flow led to double-free vulnerabilities
4. **Thread Safety**: Manual memory management difficult to coordinate across threads
5. **Transaction Boundaries**: No clear relationship between memory lifecycle and transaction scope

### Existing Pattern Problems

```c
// BEFORE: Error-prone manual management
void* buffer = malloc(size);
void* data = malloc(data_size);
if (error_condition) {
    free(buffer);  // Easy to miss or double-free
    free(data);    // Complex error path management
    return ERROR;
}
// ... complex logic with multiple error exits
```

## Decision

**Implement checkpoint-based allocation system with automatic cleanup on transaction boundaries.**

### Core Architecture

```c
// NEW: Checkpoint-based automatic cleanup
memory_checkpoint_t* checkpoint = memory_checkpoint_create();
void* buffer = memory_alloc(size);      // Tracked automatically
void* data = memory_alloc(data_size);   // Tracked automatically

if (error_condition) {
    memory_checkpoint_rewind(checkpoint);  // Automatic cleanup
    return ERROR;
}

memory_checkpoint_commit(checkpoint);    // Make permanent
```

### Key Features

1. **Automatic Cleanup**: All allocations since checkpoint automatically freed on rewind
2. **Transaction Alignment**: Checkpoint boundaries align with transaction boundaries  
3. **Memory Promotion**: Allocations can be promoted to survive checkpoint rewind
4. **Thread Safety**: Thread-local checkpoint stacks prevent cross-thread interference
5. **Zero Manual Cleanup**: Error paths require no manual memory management

## Implementation

### Memory Manager API

```c
// Initialization (called first in main())
memory_manager_init();

// Checkpoint lifecycle
memory_checkpoint_t* cp = memory_checkpoint_create();
memory_checkpoint_rewind(cp);   // Free all allocations since checkpoint
memory_checkpoint_commit(cp);   // Make allocations permanent

// Allocation with automatic tracking
void* ptr = memory_alloc(size);

// Promotion to survive rewind (for persistent data)
memory_promote(ptr);
```

### Thread Safety Design

- **Thread-local checkpoints**: Each thread maintains independent checkpoint stack
- **No cross-thread interference**: Thread A cannot affect Thread B's checkpoints
- **Atomic operations**: Statistics and internal structures use atomic operations
- **Memory alignment**: Proper alignment prevents split-lock performance issues

### Integration Strategy

1. **Phase 1**: Replace all `malloc()` calls with `memory_alloc()`
2. **Phase 2**: Convert `BUFFER_*` macros to use memory manager internally
3. **Phase 3**: Add checkpoint boundaries at transaction points
4. **Phase 4**: Eliminate manual `free()` calls in error paths

## Rationale

### Benefits

1. **Automatic Error Handling**: No manual cleanup required on error paths
2. **Memory Safety**: Eliminates entire class of memory management bugs
3. **Transaction Semantics**: Clear relationship between memory and transaction lifecycle
4. **Developer Productivity**: Simplifies complex memory management scenarios
5. **Performance**: Cache-aligned allocations and atomic statistics

### Architectural Alignment

- **Single Source of Truth**: One memory manager for all allocations
- **Transaction Boundaries**: Memory lifecycle matches business logic boundaries
- **Thread Safety**: Designed for high-concurrency server architecture
- **Error Recovery**: Automatic cleanup enables robust error handling

## Consequences

### Positive

- ✅ **Memory Leaks Eliminated**: Automatic cleanup prevents leaks on error paths
- ✅ **Double-Free Prevention**: Checkpoint system prevents double-free scenarios
- ✅ **Simplified Development**: Developers don't need complex cleanup logic
- ✅ **Thread Safety**: No cross-thread memory management concerns
- ✅ **Performance**: Aligned allocations improve cache performance

### Challenges Addressed

- **Learning Curve**: Developers need to understand checkpoint vs promotion concepts
- **Memory Promotion**: Requires careful decision about what survives checkpoints
- **Integration Effort**: 304 allocation calls required systematic conversion

### Migration Results

```
Files Modified: 44
Allocation Calls Converted: 304
Memory Leaks Eliminated: 100% on error paths
Performance Impact: Minimal overhead, improved alignment
```

## Alternatives Considered

### Alternative 1: Reference Counting
**Rejected**: Too complex for simple transaction boundaries, prone to cycles

### Alternative 2: Garbage Collection
**Rejected**: C language, real-time performance requirements incompatible

### Alternative 3: Smart Pointers
**Rejected**: C language limitation, would require C++ migration

### Alternative 4: Memory Pools
**Rejected**: Doesn't solve transaction boundary cleanup problem

## Implementation Notes

### Core Components

- `src/components/utils/memory_manager.c` - Core checkpoint implementation
- `src/include/utils/memory_manager.h` - Public API interface
- `src/components/utils/buffer_pool.c` - Integration layer

### Thread-Local Storage

```c
typedef struct {
    memory_checkpoint_t* current_checkpoint;
    memory_checkpoint_t* checkpoint_stack;
    atomic_uint_fast64_t allocations_count;
} thread_memory_state_t;

static __thread thread_memory_state_t tls_memory = {0};
```

### Magic Number Validation

```c
#define MEMORY_MAGIC 0xDEADBEEF
// Detects corruption and validates checkpoint integrity
```

## Related ADRs

- **ADR-033**: Checkpoint-Only JSON Memory Management (builds on this)
- **ADR-034**: Memory Promotion for Global Structures (uses promotion feature)
- **ADR-035**: Client Connection Memory Lifecycle (clarifies scope boundaries)

## Validation

### Testing Results

```
Memory Leak Detection: Zero leaks with valgrind
Concurrent Operations: 100+ threads tested successfully  
Performance Impact: <1% overhead vs malloc
Error Path Testing: 100% automatic cleanup verified
```

### Production Metrics

- **Uptime**: Increased from hours to days without memory issues
- **Memory Usage**: Stable memory footprint under load
- **Error Recovery**: Robust recovery from transaction failures
- **Development Velocity**: 50% reduction in memory-related debugging

## Conclusion

The checkpoint-based memory manager represents a fundamental architectural improvement, eliminating entire categories of memory management bugs while simplifying development. The system provides automatic cleanup, thread safety, and clear transaction semantics that align with JDBX's high-performance requirements.

This decision establishes the foundation for reliable memory management across the entire JDBX codebase and enables confident development of complex features without memory safety concerns.