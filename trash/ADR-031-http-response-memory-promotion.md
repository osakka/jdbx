# ADR-031: HTTP Response Memory Promotion for Checkpoint Architecture

**Status**: Accepted  
**Date**: June 19, 2025  
**Decision**: Promote ALL HTTP responses before checkpoint operations

## Context

JDBX's checkpoint memory system was causing memory corruption when serving static files concurrently with API operations. The server would crash with segmentation faults, and debugging revealed that JSON object pointers were being overwritten with HTML content (e.g., `0x696e67223e0a2020` = `ing">\n  `).

## Problem Details

### Root Cause Analysis

The API dispatch code was only promoting error responses (status >= 400) before checkpoint rewind. Success responses, including large static file responses, were not promoted before checkpoint commit. This created a race condition:

1. Thread A serves a large HTML file (e.g., index.html)
2. Response created with checkpoint memory containing HTML content
3. Checkpoint commit frees unused allocations
4. Thread B allocates memory for JSON operations
5. Memory reused, but Thread A still transmitting HTML
6. HTML content overwrites JSON object pointers
7. Next JSON access causes segfault

### Crash Manifestation
```
Program terminated with signal SIGSEGV, Segmentation fault.
#0  __strcmp_avx2 () 
#1  json_object_get (object=0x7393fc03e820, key="type")
$1 = 0x20200a3e22676e69 <error: Cannot access memory at address 0x20200a3e22676e69>
```

The pointer `0x20200a3e22676e69` decoded to ASCII revealed `ing">\n  ` - clearly HTML content.

## Decision

Promote ALL HTTP responses before ANY checkpoint operation (rewind or commit).

### Implementation

```c
/* CRITICAL: ALL responses must be promoted before checkpoint operations
 * as they survive beyond the checkpoint boundary. This includes:
 * - Error responses (promoted before rewind)
 * - Success responses (promoted before commit)
 * - Static file responses with large HTML/JS/CSS content
 * Without promotion, response memory can be freed and reused, causing
 * corruption when the response data is accessed later. */
if (request_checkpoint && result) {
    /* Promote the response structure and its contents */
    memory_promote(result);
    if (result->body) memory_promote(result->body);
    if (result->content_type) memory_promote(result->content_type);
    if (result->headers) {
        memory_promote(result->headers);
        for (size_t j = 0; j < result->num_headers; j++) {
            if (result->headers[j]) {
                memory_promote(result->headers[j]);
            }
        }
    }
}
```

## Consequences

### Positive
- ✅ Static file serving stable under concurrent load
- ✅ No memory corruption between HTTP and JSON operations
- ✅ Web UI browsing no longer crashes server
- ✅ Checkpoint benefits preserved for request processing
- ✅ Simple fix with comprehensive impact

### Negative
- ❌ ALL responses now require manual cleanup
- ❌ Slightly higher memory usage for promoted responses
- ❌ Response memory not automatically freed by checkpoints

### Neutral
- Consistent promotion pattern for all long-lived objects
- Clear lifecycle for HTTP response memory
- Aligns with SSL and skiplist promotion patterns

## Alternatives Considered

1. **Disable Checkpoints for HTTP Path**: Would lose automatic cleanup benefits
2. **Copy Response After Handler**: Would double memory usage
3. **Delayed Response Promotion**: Too complex to track response lifecycle
4. **Separate Memory Pool**: Violates single source of truth

## Testing Results

### Before Fix
- Server crashed with 50 rapid API calls
- Memory corruption visible in core dumps
- HTML content overwrote JSON pointers

### After Fix
- 50 document creations: 100% success
- 60 mixed static + API operations: No crashes
- Zero memory corruption
- No core dumps

## Memory Corruption Evidence

The corrupted pointer `0x696e67223e0a2020` decoded to `ing">\n  ` which matches HTML patterns found in served files:
- `/opt/jdbx/share/htdocs/index.html:696: <div class="stat-card warning">`
- `/opt/jdbx/share/htdocs/index.html:827: <div class="loading">`

This confirmed that static file content was overwriting JSON memory.

## Implementation Notes

- File: `src/components/core/api.c`
- Function: `api_dispatch_request()`
- Change: Moved response promotion before checkpoint operations
- Lines affected: ~20 (restructured promotion logic)

## Performance Impact

- Minimal - promotion is a simple linked list operation
- No impact on response generation or transmission
- Memory freed explicitly in `free_http_response()`

## References

- ADR-029: Skiplist Memory Promotion Architecture
- ADR-030: SSL Memory Promotion Architecture
- HTTP Response Handling: `src/components/core/http_response.c`
- Static File Serving: `src/components/core/static_files.c`