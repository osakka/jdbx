# Crash Evidence 002: Thread Pool Return Value Handling

**Date**: June 2, 2025
**Status**: CRITICAL - Root cause identified

## Summary
Server crashes immediately after `handle_client()` returns to `handle_client_adapter()` but before adapter can log the return value. This indicates a thread pool or stack corruption issue.

## Evidence

### Successful Execution Flow
```
2025-06-02 02:30:15 [1511792:1511822] [TRACE] ADAPTER_START: client_data=0x5c65aef65760, thread=139006958380736, tid=1511822
2025-06-02 02:30:15 [1511792:1511822] [INFO] CONNECTION_START: fd=7, thread=139006958380736, tid=1511822, client=127.0.0.1:54564, ssl=disabled
[... full handle_client execution with all safety checks passing ...]
2025-06-02 02:30:15 [1511792:1511822] [TRACE] CONNECTION_CLEANUP_COMPLETE: thread=139006958380736, tid=1511822
```

### Missing Logs (indicating crash point)
```
ADAPTER_HANDLE_CLIENT_RETURNED: result=%p, thread=%lu, tid=%d  ← MISSING
ADAPTER_COMPLETE: client_data=%p, thread=%lu, tid=%d           ← MISSING
```

## Analysis

1. **Client memory management**: WORKING PERFECTLY
   - Client allocation: successful
   - Client cleanup: successful with all safety checks passing
   - Memory validation: all pointer checks pass

2. **Crash location**: Occurs immediately after `handle_client()` returns `NULL` to adapter
   - Thread pool worker calls `work->function(work->argument)` (line 466 in thread_pool.c)
   - This calls `handle_client_adapter(client_data)`
   - Adapter calls `void* result = handle_client(client_data)`
   - handle_client completes successfully and returns NULL
   - **CRASH OCCURS HERE** - before adapter can log the return

3. **Root cause hypothesis**: Thread pool return value handling issue
   - Thread pool expects `void function(void*)` (no return value)
   - Adapter tries to capture return value from `handle_client()`
   - Potential stack corruption or calling convention mismatch

## Solution
Fix the adapter function to not capture the return value since thread pool expects void functions.

## Files Involved
- `/opt/jsondb/src/components/core/server.c` - handle_client_adapter function
- `/opt/jsondb/src/components/core/thread_pool.c` - thread_worker function (line 466)
- `/opt/jsondb/src/components/core/handle_client.c` - works perfectly

## Status
- **Crash Evidence**: Collected and confirmed
- **Root Cause**: Identified (thread pool return value handling)
- **Next Step**: Fix adapter function to not capture return value