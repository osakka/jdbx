# ADR-029: Use-After-Close File Descriptor Fix

**Status**: Accepted  
**Date**: 2025-06-18  
**Author**: JDBX Development Team

## Context

During rapid connection testing (SYN flood pattern), the JDBX server was experiencing general protection faults and crashes. Investigation revealed multiple use-after-close bugs in the client connection handling code where file descriptors were being used after they had been closed.

### The Problem

The server was crashing with general protection faults when handling:
- Rapid new connections (50+ concurrent connections)
- Incomplete HTTP requests that required early connection closure
- Keep-alive connection scenarios with closed sockets

Root cause analysis identified three critical use-after-close scenarios:

1. **Incomplete Request Handling**: When closing a connection due to incomplete request body, the local `client_fd` variable wasn't updated, leading to operations on fd=0 (stdin)
2. **Double Close in Cleanup**: The cleanup section always called `shutdown()` and `close()` even if the socket was already closed
3. **Keep-Alive on Closed Socket**: The keep-alive loop attempted to use `client_fd` after it was closed

## Decision

Implement comprehensive file descriptor lifecycle management to prevent use-after-close bugs:

1. **Update Local Variables**: Always set `client_fd = 0` after closing to prevent reuse
2. **Guard Socket Operations**: Check `client_fd > 0` before any socket operations
3. **Consistent State Management**: Ensure both `client->client_fd` and local `client_fd` are synchronized

## Implementation

### 1. Incomplete Request Handling Fix
```c
// After closing due to incomplete request
close(client_fd);
client->client_fd = 0;
client_fd = 0;  // CRITICAL: Update local variable to prevent use-after-close
goto cleanup;
```

### 2. Safe Cleanup Operations
```c
// Only close if socket is still open
if (client_fd > 0) {
    shutdown(client_fd, SHUT_WR);
    close(client_fd);
}
```

### 3. Keep-Alive Safety Check
```c
// Check socket is valid before keep-alive processing
if (keep_alive_enabled && client_fd > 0) {
    // Process next keep-alive request
}
```

## Consequences

### Positive
- **Crash Prevention**: Eliminates general protection faults from use-after-close bugs
- **Robust Connection Handling**: Server stable under 50+ rapid concurrent connections
- **Clear Lifecycle**: File descriptor state is always consistent and predictable
- **No Performance Impact**: Simple integer checks have negligible overhead

### Negative
- None identified - this is a pure bug fix with no downsides

## Validation

The fix was validated through:
1. **SYN Flood Test**: 50 rapid new connections handled without crashes
2. **Discovery Test**: All edge cases handled correctly
3. **Stress Testing**: Server remains stable under sustained load
4. **Zero Regressions**: All existing functionality preserved

## Lessons Learned

1. **Always Update All References**: When closing a file descriptor, update ALL variables that reference it
2. **Defensive Programming**: Always check file descriptor validity before use
3. **Consistent State**: Maintain consistency between structure members and local variables
4. **Systematic Testing**: Use tools like SYN flood tests to discover edge cases

## Related ADRs

- ADR-027: HTTP Protocol Compliance for Incomplete Requests
- ADR-026: SSL Read Reliability and Large Document Handling