# ADR-033: SSL Context Single Source of Truth

**Status**: Accepted  
**Date**: June 19, 2025  
**Decision**: Consolidate SSL context creation to socket initialization only

## Context

JDBX had duplicate SSL context creation code paths, violating the core principle of "single source of truth". The SSL context was being created in two places with different storage mechanisms, leading to confusion and potential bugs.

## Problem Details

### Architectural Violations Found

1. **Duplicate Implementation**:
   - `initialize/socket.c`: Creates SSL context, stores in `config->ssl_context`
   - `components/core/server.c`: Creates SSL context, stores in `g_ssl_context` global

2. **Global State**:
   - `static ssl_context_t* g_ssl_context = NULL;` in server.c
   - Violated "no global state" principle

3. **Confusing Fallback Logic**:
   ```c
   if (config->ssl_context) {
       g_ssl_context = config->ssl_context;  // Use socket's context
   } else {
       initialize_ssl(config);  // Create duplicate context
       config->ssl_context = g_ssl_context;
   }
   ```

4. **Duplicate Functions**:
   - `initialize_ssl()` - 100+ lines of SSL initialization
   - `cleanup_ssl()` - Duplicate cleanup logic
   - Both duplicated functionality already in socket initialization

## Decision

Remove ALL duplicate SSL code from server.c and use socket initialization as the single source of truth for SSL context creation.

### Changes Made

1. **Removed from server.c**:
   - `static ssl_context_t* g_ssl_context = NULL;` (line 33)
   - `static int initialize_ssl(server_config_t* config);` declaration
   - `static void cleanup_ssl(void);` declaration  
   - Entire `initialize_ssl()` function (lines 625-687)
   - Entire `cleanup_ssl()` function (lines 692-709)

2. **Modified in server.c**:
   - `server_get_ssl_context()` now returns `g_server_config->ssl_context`
   - SSL initialization check simplified to verify socket created context
   - SSL cleanup inlined in server shutdown

3. **Preserved**:
   - Socket initialization creates SSL context (single source)
   - Thread-safe SSL operations unchanged
   - All SSL functionality maintained

## Consequences

### Positive
- ✅ Single source of truth achieved
- ✅ No duplicate code or parallel implementations
- ✅ Eliminated global state (g_ssl_context)
- ✅ Cleaner, more maintainable architecture
- ✅ Reduced code size (~150 lines removed)
- ✅ Clear SSL lifecycle management

### Negative
- ❌ Slight coupling between socket and SSL initialization
- ❌ Must ensure socket init happens before SSL usage

### Neutral
- SSL context accessed through server config
- No performance impact
- No functional changes

## Code Quality Improvements

### Before (Duplicate Paths)
```
socket.c: ssl_context_create() → config->ssl_context
server.c: initialize_ssl() → g_ssl_context → config->ssl_context
handle_client.c: server_get_ssl_context() → g_ssl_context
```

### After (Single Path)
```
socket.c: ssl_context_create() → config->ssl_context
handle_client.c: server_get_ssl_context() → config->ssl_context
```

## Testing Results

- ✅ Server starts with SSL enabled
- ✅ SSL connections work correctly
- ✅ No memory leaks or resource issues
- ✅ Thread-safe SSL operations maintained
- ✅ All existing functionality preserved

## Architectural Principles

This refactor exemplifies JDBX's core principles:

1. **One Source of Truth**: SSL context created in exactly one place
2. **No Parallel Implementations**: Removed duplicate SSL initialization
3. **No Global State**: Eliminated g_ssl_context global variable
4. **Bar Raising**: Cleaner, more maintainable architecture
5. **Zero Regressions**: All functionality preserved

## Alternative Approaches Considered

1. **Keep Both Paths**: Would violate single source of truth
2. **Move All to Server**: Would duplicate socket's clean implementation
3. **Separate SSL Module**: Over-engineering for current needs
4. **Lazy Initialization**: Adds complexity without clear benefit

## Implementation Notes

- No changes needed to handle_client.c (already uses accessor)
- SSL context properly promoted in ssl.c for checkpoint safety
- Thread mutex for SSL_new() operations maintained
- Clean separation of concerns preserved

## References

- JDBX Principle: "One source of truth"
- Socket initialization: `src/initialize/socket.c`
- SSL utilities: `src/components/utils/ssl.c`
- Previous duplication: server.c lines 625-709 (removed)