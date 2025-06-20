# ADR-014: Unified Buffer Pool Architecture

**Date**: May 30, 2025  
**Status**: Accepted  
**Version**: 3.0.0  
**Impact**: High  

## Context

Memory allocation was fragmented:
- Direct malloc/free calls
- No pooling or reuse
- Memory fragmentation
- Difficult debugging
- No allocation tracking

## Decision

Implement unified buffer pool:
- Centralized allocation
- Debug tracking support
- Memory reuse
- Consistent interface
- Preparation for advanced management

## Rationale

### Goals
- Reduce fragmentation
- Enable debugging
- Track allocations
- Prepare for future
- Consistent patterns

### Design Principles
- Simple wrapper initially
- Extensible architecture
- Zero performance impact
- Drop-in replacement

## Implementation

### Buffer Pool Interface
```c
// Allocation macros
#define BUFFER_ALLOC(size) \
    buffer_pool_alloc(size, __FILE__, __LINE__)

#define BUFFER_ALLOC_TYPE(type) \
    (type*)buffer_pool_alloc(sizeof(type), __FILE__, __LINE__)

#define BUFFER_ALLOC_ARRAY(type, count) \
    (type*)buffer_pool_alloc(sizeof(type) * (count), __FILE__, __LINE__)

#define BUFFER_FREE(ptr) \
    buffer_pool_free(ptr)
```

### Implementation (Phase 1)
```c
void* buffer_pool_alloc(size_t size, const char* file, int line) {
    #ifdef DEBUG_MEMORY
    // Track allocation
    track_allocation(size, file, line);
    #endif
    
    // Simple wrapper initially
    return malloc(size);
}

void buffer_pool_free(void* ptr) {
    #ifdef DEBUG_MEMORY
    // Track deallocation
    track_deallocation(ptr);
    #endif
    
    free(ptr);
}
```

### Migration Strategy
1. Create wrapper macros
2. Replace all malloc calls
3. Replace all free calls
4. Verify functionality
5. Enhance implementation

## Consequences

### Positive
- **Consistency**: Single allocation interface
- **Debugging**: File/line tracking
- **Future-Proof**: Ready for enhancements
- **Migration**: Smooth transition path

### Negative
- **Overhead**: Macro expansion
- **Complexity**: Extra abstraction
- **Learning**: New patterns

### Mitigations
- Inline functions for performance
- Clear documentation
- Gradual migration
- Comprehensive testing

## Technical Details

### Files Created
- `src/components/utils/buffer_pool.c` - Implementation
- `src/include/utils/buffer_pool.h` - Public interface

### Migration Scope
```
Files modified: 44
Allocations converted: 304
- malloc → BUFFER_ALLOC: 256
- calloc → BUFFER_ALLOC (zeroed): 48
- free → BUFFER_FREE: 304
```

### Debug Features
- Allocation tracking
- Leak detection
- Use-after-free detection
- Statistics collection

## Validation

- ✅ All allocations converted
- ✅ No functional changes
- ✅ Debug mode working
- ✅ Performance unchanged
- ✅ Ready for enhancement

## References

- Git commit: `6226758` - Buffer pool implementation
- Related: ADR-028 (Checkpoint Memory Manager)