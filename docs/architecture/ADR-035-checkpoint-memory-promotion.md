# ADR-035: Checkpoint Memory Promotion Fixes

**Status**: Accepted  
**Date**: June 19, 2025  
**Decision**: Systematically promote all allocations that survive checkpoint boundaries

## Context

JDBX uses a checkpoint-based memory management system where allocations within a checkpoint are automatically freed when the checkpoint is rewound. However, certain allocations need to survive beyond checkpoint boundaries, particularly:

1. Global structures (JWT cache, thread pool, API context)
2. Persistent data structures (indexes, query tracker, adaptive indexer)
3. Data stored in global caches or pools
4. SSL/TLS structures that outlive request handling

## Problem Details

### Root Cause

When allocations that need to persist are not promoted, they get freed when checkpoints are rewound, leading to:
- Use-after-free crashes
- Segmentation faults
- Memory corruption ("unaligned tcache chunk detected")
- JWT cache corruption

### Critical Areas Affected

1. **JWT Cache**: Cache entries and JSON claims were freed after authentication
2. **Thread Pool**: Pool structure and thread arrays
3. **API Context**: Context and route arrays
4. **Indexes**: Index structures and entries
5. **Lock-Free Queue**: Queue nodes and sentinel
6. **Global Trackers**: Query tracker and adaptive indexer

## Decision

Systematically identify and promote all allocations that cross checkpoint boundaries:

```c
// Example: JWT cache entry promotion
jwt_cache_entry_t* new_entry = BUFFER_ALLOC(sizeof(jwt_cache_entry_t));
memory_promote(new_entry);  /* Cache entry survives checkpoints */

// Example: JSON promotion helper
void json_promote(json_value_t* value) {
    if (!value) return;
    memory_promote(value);
    // Recursively promote nested structures...
}
```

## Implementation

### Files Modified

1. **JWT Cache** (`jwt_cache.c`):
   - Promoted cache structure, buckets, entries
   - Promoted JWT payload strings
   - Added recursive JSON promotion for claims

2. **Thread Pool** (`thread_pool.c`):
   - Promoted pool structure and thread array

3. **API Context** (`api.c`):
   - Promoted context, JWT secret, and routes array

4. **Indexes** (`index.c`):
   - Promoted index structures, buckets, and entries
   - Promoted key strings in index entries

5. **Lock-Free Queue** (`lockfree_queue.c`):
   - Promoted queue, sentinel, and pool nodes

6. **Global Trackers**:
   - Query tracker and patterns (`query_tracker.c`)
   - Adaptive indexer (`adaptive_indexer.c`)

7. **JSON Utilities** (`json.c`):
   - Added `json_promote()` for recursive promotion

## Consequences

### Positive
- ✅ Eliminated use-after-free crashes
- ✅ Fixed JWT cache corruption
- ✅ Stable server operation under load
- ✅ Proper memory lifecycle management

### Negative
- ❌ Slightly more complex allocation patterns
- ❌ Need to remember promotion for persistent structures

### Neutral
- Memory usage unchanged (promoted memory still freed eventually)
- Performance impact negligible

## Lessons Learned

1. **Identify Lifetime Early**: When allocating, immediately consider if it crosses checkpoints
2. **Promote at Allocation**: Promote immediately after allocation for clarity
3. **Document Promotion**: Comment why each promotion is needed
4. **Test Under Load**: Memory issues often only appear under concurrent load
5. **Recursive Structures**: Complex structures like JSON need recursive promotion

## Testing Results

### Before Fixes
- Server crashed after 4-5 operations
- JWT cache corruption on second authentication
- Core dumps with use-after-free errors

### After Fixes
- Server stable for extended operation
- Multiple authentication cycles work correctly
- No memory corruption under normal load

## Future Considerations

1. Consider automatic promotion detection based on allocation patterns
2. Add debug mode that tracks checkpoint crossings
3. Create promotion guidelines for new code
4. Add static analysis to detect missing promotions