# Connection Leak Fix V2 - Analysis

## Root Cause Analysis

The connection leak is caused by the recursive design of keep-alive handling in `server_thread_safe.c`:

1. When a connection is established, `handle_client_thread_safe()` is called
2. This increments the active connections counter
3. If the request has `Connection: keep-alive`, the handler recursively calls itself with `is_keepalive_continuation=1`
4. When a keep-alive connection times out (5 seconds with no new request), it goes to cleanup
5. The cleanup only decrements if `should_decrement_on_exit` is true (which is only true for the initial connection)
6. This leaves the counter incremented

## The Problem with Current Fix

The current fix tracks whether we incremented in THIS invocation, but the real issue is:
- We increment once per TCP connection (good)
- But we may exit through cleanup multiple times (once per timeout on keep-alive)
- Only the final exit should decrement

## Correct Solution

The correct solution is to only decrement when the TCP connection is truly closing, not when a keep-alive request times out. We need to check the connection state.

## Implementation

Instead of tracking `should_decrement_on_exit`, we should:
1. Only increment for new connections (current behavior)  
2. Only decrement when setting connection state to CLOSING and we're not a keep-alive continuation

This ensures one increment and one decrement per TCP connection.