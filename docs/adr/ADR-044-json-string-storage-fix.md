# ADR-044: JSON String Storage Fix for Threading Safety

## Status
Accepted

## Context
After implementing the unified threading model (ADR-043), we discovered that while the storage query functions were updated to expect JSON strings, the storage insert function was still storing JSON object pointers. This mismatch caused all documents to be skipped as "invalid or corrupted" during queries, preventing authentication from working.

## Decision
We fixed the inconsistency by updating `storage_insert_document` to store JSON strings instead of JSON object pointers, matching what the query functions expect.

## Consequences

### Positive
- Eliminates race conditions during concurrent JSON operations
- Each thread gets its own parsed JSON object from the stored string
- No shared memory access between threads
- Consistent storage format across all operations
- Authentication and document queries work correctly

### Negative
- Slight performance overhead from JSON serialization/deserialization
- Increased memory usage (storing strings instead of pointers)
- Existing databases need to be recreated (no migration path)

## Implementation
The fix was simple but critical:

```c
// Before (storing JSON object pointers - WRONG)
json_value_t** doc_ptr = (json_value_t**)BUFFER_ALLOC(sizeof(json_value_t*));
*doc_ptr = json_deep_copy(doc_copy);
skiplist_insert(coll->documents, uuid, strlen(uuid) + 1, doc_ptr, sizeof(json_value_t*));

// After (storing JSON strings - CORRECT)
char* json_str = json_stringify(doc_copy);
skiplist_insert(coll->documents, uuid, strlen(uuid) + 1, json_str, strlen(json_str) + 1);
```

## Related Issues
- Rate limiter showing negative token values (needs investigation)
- Documents created before the fix are incompatible (require database recreation)

## Related ADRs
- ADR-043: Unified Threading Model
- ADR-042: JWT OpenSSL Security