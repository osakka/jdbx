# Connection Leak Fix Summary

## Deep Dive Analysis Results

### Root Cause Identified
The connection leak was caused by **recursive keep-alive handling** in `server_thread_safe.c`:

1. **Increment on Entry**: Each new TCP connection increments the active connections counter
2. **Recursive Keep-Alive**: When a request has `Connection: keep-alive`, the handler recursively calls itself
3. **Bypass Cleanup**: The recursion causes the original handler (that incremented) to never reach its cleanup section
4. **Accumulating Counters**: This results in increments without corresponding decrements

### Evidence Found
- 665 increments vs 342 decrements (323 leaked)
- Recursive calls shown in logs with `keepalive=1`
- Multiple handlers for same connection pointer
- 5-second timeouts indicating keep-alive timeout pattern

## Fix Implemented

### Surgical Change
Modified the keep-alive handling to ensure cleanup is always reached:

```c
/* Instead of returning after recursion */
handle_client_thread_safe_internal(client_data, 1);
return; // THIS WAS THE BUG - bypasses cleanup

/* Changed to: */
handle_client_thread_safe_internal(client_data, 1);
connection_closing = 1;
goto cleanup; // Ensures cleanup is reached
```

### Enhanced Logging Added
1. **Metrics tracking**: Shows increment/decrement with connection pointer
2. **Cleanup diagnostics**: Logs whether decrement will happen and why
3. **Leak detection**: Warns if a new connection reaches cleanup without closing
4. **Recursion depth**: Tracks nested handler calls

## Results

### Before Fix
- Started with 1 connection
- Peaked at 50+ connections
- Never returned to baseline
- Clear leak of ~300+ connections

### After Fix
- Started with 1 connection
- Peaked at 4 connections during load
- Returned close to baseline (1-3)
- Significant improvement but not perfect

## Remaining Issues

### The Real Problem
The recursive design is fundamentally flawed. The correct solution is to use a **loop** instead of recursion:

```c
while (keep_alive) {
    // Process request
    // Check keep-alive header
    // Continue or break
}
```

### Why Current Fix Isn't Perfect
- Still uses recursion which can cause stack growth
- Complex cleanup paths are error-prone
- Potential for edge cases we haven't covered

## Recommendations

1. **Short Term**: Current fix significantly reduces the leak and is safe to use
2. **Long Term**: Refactor to use a loop-based approach (see `server_thread_safe_fixed.c`)
3. **Monitoring**: Use the enhanced logging to catch any remaining leaks
4. **Testing**: Regular stress tests with keep-alive connections

## Surgical Precision Achieved
- Only modified the keep-alive handling logic
- No changes to SSL, authentication, or other systems
- Enhanced logging without changing behavior
- Minimal invasive changes as requested