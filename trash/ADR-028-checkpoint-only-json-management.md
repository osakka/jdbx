# ADR-028: Checkpoint-Only JSON Memory Management

**Status**: Accepted  
**Date**: 2025-06-19  
**Author**: JDBX Development Team

## Context

JDBX previously used manual json_free() calls throughout the codebase for JSON object cleanup. However, json_free() implementation was incomplete by design - it only freed the top-level object structure without recursively freeing child objects (arrays, nested objects, strings). This led to:

1. Memory leaks when developers assumed json_free() would clean up entire object trees
2. Inconsistent memory management patterns across the codebase
3. Double-free vulnerabilities when mixing manual cleanup with checkpoint rewinds
4. Use-after-free bugs when checkpoint rewinds freed memory still referenced by other components

## Decision

We have decided to eliminate all manual json_free() calls and rely exclusively on the checkpoint-based memory management system for JSON object lifecycle management. All 549 manual json_free() calls have been systematically converted to checkpoint comments.

## Rationale

1. **Single Source of Truth**: The checkpoint system already tracks all memory allocations made through BUFFER_* macros, including those from json_deep_copy() and json_create_* functions.

2. **Automatic Cleanup**: Checkpoint rewinds automatically free all allocations made since the checkpoint was created, eliminating the need for manual cleanup in error paths.

3. **Prevents Double-Free**: By removing manual json_free() calls, we eliminate the risk of double-freeing memory that would be freed by checkpoint rewind.

4. **Simplifies Code**: Developers no longer need to track and manually free JSON objects - the checkpoint system handles it automatically.

5. **Thread Safety**: The checkpoint system uses thread-local storage, preventing cross-thread memory management issues.

## Implementation

### Conversion Process

All json_free() calls have been converted to checkpoint comments:
```c
// Before:
json_free(result);

// After:
/* CHECKPOINT: json_free(result); */
```

### Memory Promotion for Persistent Objects

For JSON objects that must survive checkpoint rewinds (e.g., documents stored in skiplists), we use memory promotion:
```c
json_value_t* stored_doc = json_deep_copy(doc_copy);
memory_promote(stored_doc);  // Survives checkpoint rewind
skiplist_insert(coll->documents, uuid, strlen(uuid) + 1, &stored_doc, sizeof(json_value_t*));
```

### Files Modified

- 54 files across the codebase
- 549 manual json_free() calls converted
- Notable files with high concentrations:
  - rbac_database.c (46 calls)
  - transaction_log.c (37 calls)
  - js_native_storage.c (36 calls)
  - json_schema_manager.c (30 calls)

## Consequences

### Positive

1. **Unified Memory Management**: All memory is now managed through a single system
2. **Automatic Error Cleanup**: No more manual cleanup in error paths
3. **Reduced Bugs**: Eliminates entire classes of memory management bugs
4. **Simplified Development**: Developers don't need to track JSON object ownership

### Negative

1. **Learning Curve**: Developers must understand checkpoint system
2. **Promotion Requirements**: Must explicitly promote long-lived objects
3. **Debugging Complexity**: Memory leaks may be harder to trace to specific allocations

### Neutral

1. **Performance**: Checkpoint system has minimal overhead compared to manual cleanup
2. **Memory Usage**: Similar patterns, just automated instead of manual

## Notes

- The incomplete json_free() implementation was intentional - it avoided the complexity of recursive cleanup
- This change aligns with JDBX's "one source of truth" principle for memory management
- Future JSON operations should never use manual json_free() calls