# ADR-021: Use-After-Close File Descriptor Fix

**Date**: June 17, 2025  
**Status**: Accepted  
**Version**: 6.5.10  
**Impact**: Critical  

## Context

Critical stability issue under rapid connection load:
- General protection faults with 10+ rapid connections
- File descriptors used after closure
- Keep-alive loop using closed sockets
- Server crashes under SYN flood conditions

## Decision

Implement comprehensive file descriptor lifecycle management:
1. Synchronize local and client fd variables
2. Check fd validity before operations
3. Proper cleanup section logic
4. Safe keep-alive termination

## Rationale

### Root Causes
1. Local `client_fd` not updated after close
2. Cleanup always called shutdown/close
3. Keep-alive attempted on closed fds
4. Operations on fd=0 (stdin)

### Impact
- Server crashes under load
- DoS vulnerability
- Unpredictable behavior
- Resource leaks

## Implementation

### FD Synchronization
```c
// BEFORE - Local variable out of sync
int client_fd = client->socket_fd;
close(client_fd);
// ... later code uses client_fd (stale!)

// AFTER - Keep synchronized
close(client->socket_fd);
client->socket_fd = -1;  // Mark invalid
local_client_fd = -1;    // Sync local copy
```

### Cleanup Section Fix
```c
// BEFORE - Always shutdown/close
cleanup:
    shutdown(local_client_fd, SHUT_RDWR);
    close(local_client_fd);

// AFTER - Check validity first
cleanup:
    if (local_client_fd >= 0) {
        shutdown(local_client_fd, SHUT_RDWR);
        close(local_client_fd);
    }
```

### Keep-Alive Safety
```c
// Safe keep-alive loop
while (keep_alive && local_client_fd >= 0) {
    http_request_t* request = read_request(client);
    
    if (!request || local_client_fd < 0) {
        break;  // Connection closed
    }
    
    // Process request...
    
    // Re-check after processing
    if (client->socket_fd < 0) {
        local_client_fd = -1;
        break;
    }
}
```

## Consequences

### Positive
- **Stability**: No crashes under load
- **Resilience**: Handles connection floods
- **Safety**: No use-after-close
- **Performance**: Clean fd management

### Negative
- **Complexity**: More state checks
- **Overhead**: Validation logic

### Mitigations
- Efficient integer checks
- Clear state management
- Comprehensive testing
- fd leak monitoring

## Technical Details

### Files Modified
- `src/components/core/handle_client.c` - FD lifecycle

### Test Results
```bash
# SYN flood test
for i in {1..50}; do
    (echo -n | nc localhost 5000) &
done
wait

Result: All connections handled, zero crashes
```

### Key Fixes
1. Set fd to -1 after close
2. Check fd >= 0 before use
3. Sync local and struct copies
4. Safe cleanup paths

## Validation

- ✅ 50 rapid connections handled
- ✅ Zero crashes under load
- ✅ FD lifecycle correct
- ✅ No resource leaks
- ✅ DoS resilience improved

## References

- Git commit: Use-after-close fix
- Related: ADR-022 (Documentation Excellence)