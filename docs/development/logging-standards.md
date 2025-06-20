# JDBX Logging Standards

**Version**: 1.0.0  
**Last Updated**: June 20, 2025  
**Status**: Active  

## Overview

This document defines the logging standards for the JDBX project to ensure consistent, actionable, and appropriate logging across all components.

## Log Format

All log messages follow this format:
```
timestamp [processid:threadid] [level] functionname.filename line_num: message
```

Example:
```
2025-06-20 10:15:30 [1234:5678] [INFO] handle_request.api 245: Processed document creation request
```

## Log Levels

### Production Levels

#### ERROR (Level 1)
- **Purpose**: Critical failures preventing normal operation
- **Audience**: Production SREs, On-call engineers
- **Examples**:
  - Database corruption detected
  - SSL certificate loading failed
  - Out of memory conditions
  - File system errors

#### WARNING (Level 2)
- **Purpose**: Important issues needing attention but not stopping operation
- **Audience**: Production SREs, Operations teams
- **Examples**:
  - Retry succeeded after transient failure
  - Performance degradation detected
  - Resource usage approaching limits
  - Configuration using defaults

#### INFO (Level 3) - Production Default
- **Purpose**: Key operational events
- **Audience**: Operations teams monitoring system health
- **Examples**:
  - Server started/stopped
  - Configuration loaded
  - Major subsystem initialized
  - Client connections accepted/closed

### Development Levels

#### DEBUG (Level 4) - Development Default
- **Purpose**: Detailed information for troubleshooting
- **Audience**: Developers debugging issues
- **Examples**:
  - Request/response details
  - Query execution plans
  - Cache hit/miss statistics
  - Resource allocation details

#### TRACE (Level 5) - Development Deep Dive
- **Purpose**: Very detailed execution flow
- **Audience**: Developers tracking specific issues
- **Categories**: Enabled per-module for targeted debugging
  - DATABASE: Query building, index selection
  - API: Route matching, parameter validation
  - AUTH: Token validation, permission checks
  - MEMORY: Allocation/deallocation tracking
  - NETWORK: Packet-level details

## Message Guidelines

### DO:
- Write concise, actionable messages
- Include relevant context (IDs, counts, durations)
- Use present tense for actions ("Processing request")
- Use past tense for completions ("Processed 100 documents")
- Include performance metrics where relevant

### DON'T:
- Include function/file/line info (automatically added)
- Use status prefixes (SUCCESS:, FAILURE:, etc.)
- Use component prefixes ([INIT:], [RBAC:], etc.)
- Log sensitive information (passwords, tokens, keys)
- Use excessive punctuation or capitalization

### Examples:

❌ **Bad**:
```c
LOG_INFO("[INIT:DATABASE] SUCCESS: Database initialized successfully!");
LOG_ERROR("FAILURE: Could not open file");
LOG_INFO("Starting server initialization...");
```

✅ **Good**:
```c
LOG_INFO("Database initialized with %d collections", collection_count);
LOG_ERROR("Failed to open file '%s': %s", filename, strerror(errno));
LOG_INFO("Initializing server on port %d", port);
```

## Component-Specific Guidelines

### Initialization
- INFO: Major component initialized
- DEBUG: Configuration details
- ERROR: Initialization failures

### Request Processing
- DEBUG: Request received with details
- DEBUG: Response sent with status
- WARNING: Request validation failures
- ERROR: Processing exceptions

### Resource Management
- DEBUG: Resource allocated/freed
- WARNING: Resource limits approaching
- ERROR: Resource exhaustion

### Performance
- INFO: Periodic performance summaries
- DEBUG: Operation timings
- WARNING: Slow operations

## Configuration

### Environment Variables
```bash
JDBX_LOG_LEVEL=INFO|DEBUG|TRACE|WARNING|ERROR
JDBX_TRACE_CATEGORIES=database,api,auth  # Comma-separated
```

### Command Line Flags
```bash
jdbxd --log-level=DEBUG
jdbxd --trace-categories=database,memory
```

### Runtime API
```
PUT /api/system/logging
{
  "level": "DEBUG",
  "trace_categories": ["database", "api"]
}
```

## Thread Safety

All logging operations are thread-safe through:
- Mutex protection around log writes
- Thread-local buffers for formatting
- Atomic operations for configuration changes

## Performance

When a log level is disabled:
- Macro expansion prevents function calls
- Near-zero CPU overhead
- No string formatting occurs
- No mutex acquisition

## Migration Guide

### Phase 1: Update Existing Messages
1. Remove component prefixes ([INIT:], etc.)
2. Remove status prefixes (SUCCESS:, FAILURE:)
3. Ensure appropriate log level
4. Make messages concise and actionable

### Phase 2: Enable Trace Categories
1. Replace verbose DEBUG with appropriate TRACE
2. Use category-specific trace macros
3. Document trace categories in component headers

### Phase 3: Runtime Configuration
1. Implement API endpoints for log configuration
2. Add environment variable support
3. Update documentation

## Audit Checklist

- [ ] No redundant prefixes in messages
- [ ] Appropriate log level for audience
- [ ] Concise, actionable messages
- [ ] No sensitive information logged
- [ ] Performance metrics included where relevant
- [ ] Trace categories used for detailed debugging
- [ ] Thread-safe logging maintained