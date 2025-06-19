# ADR-028: Memory Checkpoint Error Response Promotion

**Status**: Implemented  
**Date**: 2025-06-19  
**Context**: Critical server crash on authentication error responses

## Context

The JDBX server was experiencing critical crashes (general protection faults) when returning error responses from API handlers, particularly noticeable during failed authentication attempts. The crash pattern was:

1. Successful login → Works fine
2. Failed login (wrong password) → Returns proper error response
3. Any subsequent request → Server crashes with memory corruption

## Problem

The root cause was a fundamental design flaw in how memory checkpoints interacted with HTTP error responses:

1. API handlers create memory checkpoints at the start of request processing
2. Handlers allocate memory (JSON objects, HTTP responses) within the checkpoint scope
3. On error (HTTP status >= 400), the checkpoint is rewound to free all allocations
4. However, the error response itself was allocated within the checkpoint scope
5. After checkpoint rewind, the response pointer pointed to freed memory
6. Attempting to use this freed response caused crashes

This created a use-after-free vulnerability affecting all error responses across the entire API.

## Decision

Implement response promotion before checkpoint rewind for error responses. When an API handler returns an error response (status >= 400), the response and all its contents must be promoted to survive the checkpoint rewind.

## Implementation

```c
/* CRITICAL: Promote response before rewinding checkpoint */
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

## Additional Changes

1. **Removed manual JSON cleanup**: All `json_free()` calls in error paths were commented out since the checkpoint system handles cleanup automatically
2. **Unified response creation**: Eliminated duplicate response creation functions to ensure single source of truth
3. **Enhanced memory manager**: Added early abort if memory operations occur before initialization

## Consequences

### Positive
- Server no longer crashes on error responses
- Memory management is consistent across all API handlers
- Error handling is more robust and predictable
- Checkpoint system works as designed

### Negative
- Slight performance overhead from promoting error responses
- Must ensure all error responses are properly promoted

### Neutral
- Reinforces the checkpoint-based memory management architecture
- Requires developers to understand checkpoint/promotion semantics

## Lessons Learned

1. **Mixed memory management is dangerous**: Combining manual memory management with checkpoint-based systems creates subtle bugs
2. **Error paths need special attention**: Error handling code paths often have different memory lifecycle requirements
3. **Promotion is critical**: Objects that survive checkpoint boundaries must be explicitly promoted
4. **Testing must include error scenarios**: The bug only manifested on error responses, highlighting the need for comprehensive error testing

## Testing

Verified fix with:
- Multiple consecutive failed login attempts
- Malformed JSON requests
- Various error scenarios across different endpoints
- Confirmed server stability after errors

## References

- Related to checkpoint memory management system (v6.3.0)
- Fixes critical stability issue discovered during authentication testing
- Complements ADR-027 SSL/TLS compatibility fixes