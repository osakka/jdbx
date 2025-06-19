# ADR-029: Skiplist Memory Promotion for Checkpoint Architecture

**Status**: Accepted  
**Date**: June 19, 2025  
**Decision**: Promote all skiplist allocations to survive checkpoint rewinds

## Context

JDBX's checkpoint-based memory management system provides automatic cleanup of allocations when checkpoints are rewound. However, this created a fundamental architectural conflict with persistent data structures like skiplists used for document storage.

The problem manifested as use-after-free crashes when:
1. Skiplist nodes were allocated within checkpoint boundaries
2. Checkpoint rewind freed these nodes
3. Subsequent operations tried to access freed memory

## Problem Details

### Root Cause Analysis
```c
// BEFORE: Stack address stored in skiplist
json_value_t* stored_doc = json_deep_copy(doc_copy);
skiplist_insert(coll->documents, uuid, strlen(uuid) + 1, 
                &stored_doc,  // BUG: Stack address!
                sizeof(json_value_t*));
```

When the function returned, `stored_doc` went out of scope, but the skiplist still held the stack address. Worse, even if we used the heap address directly, checkpoint rewind would free it.

### Cascade Effects
1. **Insert Path**: Documents freed by checkpoint cleanup
2. **Update Path**: New values freed before old values replaced
3. **Node Structure**: Skiplist nodes themselves freed while still linked
4. **HTTP Corruption**: Response data overwrote freed skiplist memory

## Decision

Implement comprehensive memory promotion for all skiplist-related allocations to ensure they survive checkpoint rewinds.

### Implementation Strategy

1. **Heap-Allocated Pointer Storage**
```c
// Allocate persistent pointer storage
json_value_t** doc_ptr = (json_value_t**)BUFFER_ALLOC(sizeof(json_value_t*));
*doc_ptr = json_deep_copy(doc_copy);

// Promote both pointer storage AND document
memory_promote(doc_ptr);     // Pointer storage survives
memory_promote(*doc_ptr);    // Document survives
```

2. **Skiplist Node Promotion**
```c
static inline skiplist_node_t* skiplist_create_node(...) {
    skiplist_node_t* node = BUFFER_ALLOC(...);
    memory_promote(node);    // Node structure survives
    
    // Copy and promote key
    node->key = BUFFER_ALLOC(key_len);
    memcpy(node->key, key, key_len);
    memory_promote(node->key);  // Key survives
    
    // Copy and promote value
    node->value = BUFFER_ALLOC(value_len);
    memcpy(node->value, value, value_len);
    memory_promote(node->value);  // Value survives
}
```

3. **Update Path Promotion**
```c
// In skiplist update path
void* new_value = BUFFER_ALLOC(value_len);
memory_promote(new_value);  // Ensure new value survives
memcpy(new_value, value, value_len);
```

## Consequences

### Positive
- ✅ Persistent data structures work correctly with checkpoints
- ✅ No manual memory management needed
- ✅ Checkpoint benefits preserved for transient allocations
- ✅ Production stability under concurrent load
- ✅ Clear architectural separation of concerns

### Negative
- ❌ Promoted memory not automatically cleaned (requires explicit free)
- ❌ Developers must understand promotion semantics
- ❌ Potential memory growth if promotions not balanced with frees

### Neutral
- Memory promotion is explicit and intentional
- Clear distinction between transient and persistent allocations
- Aligns with enterprise memory management patterns

## Alternatives Considered

1. **Disable Checkpoints for Storage Path**: Would lose automatic cleanup benefits
2. **Manual Memory Management**: Would regress to error-prone patterns
3. **Reference Counting**: Too complex for current architecture
4. **Separate Memory Pools**: Would violate single source of truth

## Implementation Notes

### Files Modified
- `src/components/database/database.c` - Document storage functions
- `src/include/utils/skiplist.h` - Inline node creation with promotion
- `src/components/utils/skiplist.c` - Update path promotion

### Testing Validation
- 20/20 concurrent operations (100% success rate)
- Large document handling (10KB+) without issues
- No memory corruption under stress testing
- Zero segfaults or use-after-free errors

## References
- ADR-028: Checkpoint-Only JSON Memory Management
- Memory Manager Documentation: `src/include/utils/memory_manager.h`
- Skiplist Implementation: `src/components/utils/skiplist.c`