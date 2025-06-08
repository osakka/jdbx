# Connection Leak Root Cause Analysis

## The Fundamental Problem

The thread-safe handler uses **recursion** for keep-alive connections, which causes:

1. **Stack Growth**: Each keep-alive request adds another stack frame
2. **Complex Cleanup**: The cleanup logic becomes convoluted with recursion
3. **Metric Tracking Issues**: Hard to track when to increment/decrement with nested calls
4. **Potential Race Conditions**: Multiple recursive calls might process the same connection

## Current Flow (Broken)

```
handle_client_thread_safe(data)
  -> increment counter
  -> process request
  -> if keep-alive:
       -> recursively call handle_client_thread_safe_internal(data, 1)
          -> process request
          -> if keep-alive:
               -> recursively call handle_client_thread_safe_internal(data, 1)
                  -> ... (stack keeps growing)
                  -> timeout after 5 seconds
                  -> cleanup (no decrement - is_keepalive_continuation=1)
               <- return
          -> cleanup (no decrement - is_keepalive_continuation=1)  
       <- return
  -> cleanup (should decrement but may not reach here)
```

## The Correct Solution

Replace recursion with a **loop**:

```
handle_client_thread_safe(data)
  -> increment counter
  -> while (connection alive):
       -> process request
       -> if not keep-alive:
            -> break
  -> decrement counter
  -> cleanup
```

This ensures:
- One increment per TCP connection
- One decrement per TCP connection
- No stack growth
- Clear cleanup path