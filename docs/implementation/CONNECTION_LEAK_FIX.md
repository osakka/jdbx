# Connection Leak Fix Summary

## Problem Identified

The JSONdb server has potential connection leak issues in both the thread-safe and traditional client handlers where the active connections metric might not be properly decremented in all cases.

## Root Causes

1. **Keep-Alive Connection Handling**: The thread-safe handler recursively calls itself for keep-alive connections but the metric tracking doesn't properly account for this pattern.

2. **Error Path Cleanup**: Various error paths in the connection handlers may not properly decrement the connection counter.

3. **Connection State Tracking**: The connection lifecycle isn't consistently tracked across all code paths.

## Key Areas to Investigate

### 1. Thread-Safe Handler (server_thread_safe.c)
- The handler increments active connections at the start
- For keep-alive connections, it recursively calls itself
- The cleanup section needs to be careful about when to decrement

### 2. Traditional Handler (handle_client.c)  
- Increments active connections at line 173
- Decrements in cleanup at line 827
- This appears balanced but needs verification

### 3. Connection Manager (client_connection.c)
- Maintains its own connection tracking
- Registers/unregisters connections
- Has proper reference counting

## Recommendations for Fix

1. **Track Connection Lifecycle**: Only increment the counter once per actual TCP connection, not per HTTP request.

2. **Use Connection State**: Check the connection state before decrementing to ensure we only decrement when the connection is truly closing.

3. **Separate Request vs Connection Metrics**: Consider having separate metrics for:
   - Active TCP connections
   - Active HTTP requests
   - Total HTTP requests handled

4. **Audit All Exit Paths**: Ensure every code path that increments the counter has a corresponding decrement.

5. **Add Debug Logging**: Add trace logging for all increment/decrement operations to help diagnose issues.

## Testing Strategy

1. Monitor the active connections metric during load testing
2. Test with keep-alive connections making multiple requests
3. Verify connection count returns to 0 after all clients disconnect
4. Test error scenarios (bad requests, timeouts, etc.)
5. Use connection pooling clients to stress test the keep-alive logic

## Implementation Notes

The fix should focus on:
- Clear separation between TCP connection lifecycle and HTTP request lifecycle
- Consistent state management across all handlers
- Proper cleanup in all error paths
- Thread-safe metric operations