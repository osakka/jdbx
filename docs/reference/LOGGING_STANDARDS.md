# JDBX Logging Standards

## Overview

This document defines the logging standards for the JDBX project to ensure consistency, clarity, and appropriate information disclosure across all components.

## Log Format

All log messages MUST follow this format:
```
[timestamp] [LEVEL] [file:line:function] message
```

Example:
```
[2025-05-28 08:37:49] [INFO] [server.c:456:start_server] Server started on port 5000
```

## Log Levels

### ERROR (Level 1)
**Audience**: Production users, SREs, Developers
**Purpose**: Critical failures that prevent normal operation
**Guidelines**:
- Use for unrecoverable errors that require immediate attention
- Include actionable information or error codes
- Always include context (what failed, why, potential resolution)

**Examples**:
```c
LOG_ERROR("Failed to bind socket on port %d: %s", port, strerror(errno));
LOG_ERROR("Database corruption detected at offset %zu", offset);
LOG_ERROR("Authentication failed for user '%s': invalid credentials", username);
```

### WARNING (Level 2)
**Audience**: Production users, SREs, Developers
**Purpose**: Potentially harmful situations that don't prevent operation
**Guidelines**:
- Use for recoverable errors or degraded functionality
- Include information about automatic recovery or workarounds
- Avoid for normal operational events

**Examples**:
```c
LOG_WARNING("Connection pool nearing capacity: %d/%d connections", used, max);
LOG_WARNING("Slow query detected: %s (took %.2fms)", query, elapsed_ms);
LOG_WARNING("Using fallback configuration: config file not found");
```

### INFO (Level 3)
**Audience**: Production users, SREs
**Purpose**: Important operational events and state changes
**Guidelines**:
- Use for significant lifecycle events (startup, shutdown, configuration)
- Log successful completion of important operations
- Keep messages concise and professional
- Default production log level

**Examples**:
```c
LOG_INFO("Server initialized on %s:%d", host, port);
LOG_INFO("Database loaded: %zu collections, %zu documents", collections, docs);
LOG_INFO("User session created for '%s' from %s", username, ip_address);
```

### DEBUG (Level 4)
**Audience**: Developers
**Purpose**: Detailed information for debugging production issues
**Guidelines**:
- Use for flow tracing and state inspection
- Include relevant variable values and decision points
- Avoid in performance-critical paths
- Should not contain sensitive data

**Examples**:
```c
LOG_DEBUG("Processing request: method=%s, path=%s", method, path);
LOG_DEBUG("Query optimization: using index '%s' for field '%s'", index, field);
LOG_DEBUG("Cache hit for key '%s' (hit_rate=%.2f%%)", key, hit_rate);
```

### TRACE (Level 5)
**Audience**: Developers (development environment only)
**Purpose**: Extremely detailed debugging information
**Guidelines**:
- Use for low-level operations (memory, locking, I/O)
- Include function entry/exit, loop iterations
- May impact performance significantly
- Can contain internal implementation details

**Examples**:
```c
LOG_TRACE("Entering function with args: ptr=%p, size=%zu", ptr, size);
LOG_TRACE("Mutex lock acquired: %p (thread=%lu)", mutex, thread_id);
LOG_TRACE("Buffer state: pos=%zu, cap=%zu, data=%.*s", pos, cap, (int)len, data);
```

## Best Practices

### 1. Performance Considerations
- Logging statements are only evaluated if the level is enabled
- Use appropriate levels to minimize production overhead
- TRACE level should have near-zero impact when disabled

### 2. Message Content
- Be specific and actionable
- Include relevant context without being verbose
- Use consistent terminology throughout the codebase
- Avoid redundant information already in the format

### 3. Sensitive Data
- NEVER log passwords, tokens, or keys at any level
- Mask or truncate sensitive identifiers
- Be cautious with user data in DEBUG/TRACE levels

### 4. Common Patterns

**Operation Start/Complete**:
```c
LOG_INFO("Starting database backup to '%s'", backup_path);
// ... operation ...
LOG_INFO("Database backup completed: %zu bytes written in %.2fs", bytes, elapsed);
```

**Error with Context**:
```c
if (result < 0) {
    LOG_ERROR("Failed to write document '%s' to collection '%s': %s",
              doc_id, collection, strerror(errno));
    return -1;
}
```

**Performance Monitoring**:
```c
LOG_DEBUG("Operation completed in %.2fms (threshold: %.2fms)", 
          elapsed_ms, threshold_ms);
if (elapsed_ms > threshold_ms) {
    LOG_WARNING("Slow operation detected: %.2fms exceeds threshold", elapsed_ms);
}
```

## Migration from Direct Output

Replace direct console output with appropriate logging:

**Before**:
```c
printf("Server starting...\n");
fprintf(stderr, "Error: %s\n", error_msg);
```

**After**:
```c
LOG_INFO("Server starting");
LOG_ERROR("%s", error_msg);
```

## Component-Specific Logging

Use consistent prefixes for component identification:
```c
LOG_INFO("[RBAC] Role created: '%s' with %zu permissions", role_name, perm_count);
LOG_DEBUG("[Cache] Evicting entry: key='%s', age=%lds", key, age);
LOG_TRACE("[Binary] Writing TLV: type=%u, length=%u", type, length);
```

## Conditional Compilation

For development-only logging:
```c
#ifdef DEVELOPMENT
    LOG_TRACE("Detailed internal state: %s", internal_state);
#endif
```

## Summary

- **ERROR**: Critical failures requiring attention
- **WARNING**: Issues that don't prevent operation
- **INFO**: Important operational events (production default)
- **DEBUG**: Developer information for troubleshooting
- **TRACE**: Detailed debugging (development only)

Always consider your audience and the performance impact when choosing a log level.