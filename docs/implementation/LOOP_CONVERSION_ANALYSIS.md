# Surgical Loop Conversion Analysis

## What It Means to Convert to a Loop

### Current Structure (Recursive)
```
handle_client_thread_safe()
  └─> handle_client_thread_safe_internal(keepalive=0)
       ├─> Process request
       ├─> If keep-alive:
       │    └─> handle_client_thread_safe_internal(keepalive=1) [RECURSION]
       │         ├─> Process request
       │         └─> If keep-alive: [MORE RECURSION]
       └─> cleanup (may not reach here)
```

### Target Structure (Loop)
```
handle_client_thread_safe()
  ├─> Increment counter ONCE
  ├─> while (connection alive):
  │    ├─> Read request
  │    ├─> Process request
  │    ├─> Send response
  │    └─> Check keep-alive header
  ├─> Decrement counter ONCE
  └─> cleanup
```

## Benefits of Loop Approach

1. **No Stack Growth**: Each request doesn't add a stack frame
2. **Clear Lifecycle**: One increment, one decrement, guaranteed
3. **Simpler Logic**: No need to track recursion state
4. **Better Error Handling**: Single cleanup path
5. **Memory Efficient**: Constant memory usage regardless of requests

## Surgical Changes Required

1. **Remove Recursion**: Delete the recursive call
2. **Add Loop**: Wrap request processing in `while (keep_alive)`
3. **Move Variables**: Some variables need to be outside the loop
4. **Simplify Cleanup**: Single cleanup path at end

## Risk Assessment

- **Low Risk**: The logic remains the same, just restructured
- **Easy Rollback**: Can revert if issues found
- **Better Than Current**: Even if not perfect, it's cleaner than recursion
- **Testable**: Same test cases apply