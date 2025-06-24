# ADR-046: Unified Logging and Early-Stage Support

**Status**: Implemented  
**Date**: 2025-06-24  
**Author**: JDBX Team  

## Context

The JDBX logging system had several inconsistencies and redundancies:

1. **Redundant Prefixes**: Log messages included component prefixes like `[DAEMON]`, `[FILE_SERVING]`, and `[INIT:SOCKET]` that duplicated information already available in the log format (filename, function name).

2. **Early-Stage Logging**: Before the logger was initialized, different components used inconsistent approaches:
   - Direct `fprintf(stderr, ...)` calls
   - Custom macros like `DAEMON_LOG` with fallback logic
   - `INIT_LOG_*` macros with different behavior

3. **Daemonization Challenges**: After daemonization, stderr is closed, making fprintf ineffective. The transition between pre and post-logger initialization needed special handling.

## Decision

We implemented a unified logging architecture with early-stage support:

### 1. Early-Stage Logging Function
```c
void logger_early_log(log_level_t level, const char* component, const char* format, ...);
```
- Provides consistent formatting before logger initialization
- Outputs to stderr with full timestamp, PID/TID, level, and component
- Seamlessly replaced by regular logging once logger is ready

### 2. Unified Macros
```c
// Early-stage logging macros
#define EARLY_LOG_ERROR(component, ...)   logger_early_log(LOG_LEVEL_ERROR, component, __VA_ARGS__)
#define EARLY_LOG_WARNING(component, ...) logger_early_log(LOG_LEVEL_WARNING, component, __VA_ARGS__)
#define EARLY_LOG_INFO(component, ...)    logger_early_log(LOG_LEVEL_INFO, component, __VA_ARGS__)
#define EARLY_LOG_DEBUG(component, ...)   logger_early_log(LOG_LEVEL_DEBUG, component, __VA_ARGS__)
```

### 3. Updated Special-Case Macros
- `DAEMON_LOG`: Now uses either regular logging or early-stage logging
- `INIT_LOG_*`: Updated to use early-stage logging when logger unavailable
- Removed redundant prefixes from all macro implementations

### 4. Removed Redundant Prefixes
Eliminated component prefixes from log messages since the log format already includes:
- Filename (without extension)
- Function name
- Line number

## Consequences

### Positive
- **Consistent Format**: All logs follow the same format regardless of initialization state
- **Reduced Redundancy**: No duplicate component information in messages
- **Smooth Transitions**: Seamless logging during daemonization and initialization
- **Thread-Safe**: Both regular and early-stage logging are thread-safe
- **Cleaner Messages**: More concise, focused log messages

### Negative
- **Slight Overhead**: Early-stage logging adds minimal overhead during initialization
- **Migration Effort**: Required updating many log statements across the codebase

## Implementation Details

### Log Format
```
timestamp [pid:tid] [level] function.filename line: message
```

Example:
```
2025-06-24 09:37:25 [2774260:2774260] [INFO] server_initialize_and_run.server 203: Starting accept loop with SSL support enabled=1
```

### Special Cases Preserved
- Logger initialization itself still uses `fprintf(stderr)` for bootstrap errors
- Main.c uses `fprintf(stderr)` before any logging is available
- These cases are intentionally preserved for reliability

## Future Considerations

1. **Runtime Configuration**: Next phase will add API/ENV/CLI log level control
2. **Trace Categories**: Already implemented per-functionality trace control
3. **Log Rotation**: Consider adding built-in log rotation support
4. **Structured Logging**: Future enhancement could add JSON log format option