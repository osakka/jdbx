# ADR-032: Client Connection Memory Promotion for Checkpoint Architecture

**Status**: Accepted  
**Date**: June 19, 2025  
**Decision**: Promote client connection structures to survive checkpoint operations

## Context

JDBX's checkpoint memory system was causing SSL connection crashes when handling concurrent requests. The server would crash with segmentation faults in OpenSSL's `SSL_free()` function, particularly during the `OPENSSL_sk_pop_free()` operation.

## Problem Details

### Root Cause Analysis

The crash pattern revealed a critical architectural conflict:

1. Server accepts connection and allocates `client_conn_t` structure
2. Structure passed to thread pool for processing
3. Thread worker creates checkpoint for request handling
4. SSL connection created and attached to client structure
5. Checkpoint rewind/commit frees the client structure
6. Later SSL cleanup tries to access freed client memory
7. Segmentation fault in `SSL_free()` → `OPENSSL_sk_pop_free()`

### Evidence from Core Dumps
```
#0  0x00007c7d020af22c in OPENSSL_sk_pop_free () from libcrypto.so.3
#1  0x00007c7d020e50a5 in X509_VERIFY_PARAM_free () from libcrypto.so.3
#2  0x00007c7d023463dc in SSL_free () from libssl.so.3
#3  0x00005fc779a30b1a in ssl_connection_free (conn=0x7c7ce00166b0)
```

The crash consistently occurred when trying to free SSL connections, indicating the parent client structure had been freed by checkpoint operations.

## Decision

Promote ALL client connection structures immediately after allocation to ensure they survive checkpoint operations throughout the request lifecycle.

### Implementation

Added `memory_promote()` calls in three server accept loops:

1. **server_thread_safe.c**:
```c
client_conn_t* client = (client_conn_t*)BUFFER_ALLOC(sizeof(client_conn_t));
if (!client) {
    LOG_ERROR("Cannot allocate memory for client connection");
    close(client_fd);
    continue;
}

/* CRITICAL: Client connections must survive checkpoint rewinds */
memory_promote(client);
```

2. **server.c** - Same pattern in the standard accept loop
3. **server_init_sequence.c** - Same pattern in the init sequence accept loop

## Consequences

### Positive
- ✅ SSL connections no longer crash during cleanup
- ✅ Server handles concurrent SSL operations reliably
- ✅ Memory safety maintained across checkpoint boundaries
- ✅ No changes to existing checkpoint architecture
- ✅ Simple, surgical fix with clear rationale

### Negative
- ❌ Client structures not automatically cleaned by checkpoints
- ❌ Manual cleanup required in error paths
- ❌ Slightly higher memory usage (promoted allocations)

### Neutral
- Consistent with other promoted structures (SSL contexts, skiplists)
- Follows established pattern for cross-checkpoint resources
- Documents critical lifetime requirements

## Alternatives Considered

1. **Disable Checkpoints for Accept Loop**: Would lose benefits of automatic cleanup
2. **Create Clients Outside Checkpoints**: Complex thread synchronization required
3. **Pool Pre-allocated Clients**: Over-engineering for the problem
4. **Reference Counting**: Adds complexity without addressing root cause

## Testing Results

### Before Fix
- Server crashed after 4-5 concurrent SSL connections
- Consistent segfaults in SSL cleanup paths
- Memory corruption visible in core dumps

### After Fix
- 50 document creates: 100% success
- 20 concurrent queries: No crashes
- Mixed SSL/API operations: Stable
- Continuous operation verified

## Architectural Alignment

This fix maintains JDBX's checkpoint memory architecture while properly handling resources that cross checkpoint boundaries. The pattern is:

1. **Checkpoint-local**: Request/response data, temporary buffers
2. **Promoted**: Client connections, SSL contexts, database structures
3. **External**: OpenSSL internal allocations (untracked)

## Implementation Notes

- Added `#include "utils/memory_manager.h"` to affected files
- No changes to client structure or SSL handling logic
- Promotion occurs immediately after allocation
- Cleanup paths unchanged (already handle manual free)

## References

- ADR-029: Skiplist Memory Promotion Architecture
- ADR-030: SSL Memory Promotion Architecture  
- ADR-031: HTTP Response Memory Promotion
- Core dump analysis: `/opt/jdbx/core` (79MB)