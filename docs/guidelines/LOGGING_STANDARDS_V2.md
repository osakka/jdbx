# JDBX Logging Standards V2.0

## Overview

This document defines the comprehensive logging standards for JDBX to ensure consistency, actionability, and appropriate audience targeting across all log messages.

## Log Format Standard

**Format**: `timestamp [processid:threadid] [level] functionname.filename line_num: message`

**Example**: `2025-06-08 14:30:15 [1234:5678] [INFO] init_database.database 145: Database initialized with 3 collections`

## Log Level Guidelines

### ERROR (Production & Development)
- **Purpose**: Critical failures preventing operation
- **Audience**: SRE/Operations + Developers
- **Content**: What failed, immediate impact
- **Action Required**: Immediate intervention needed
- **Examples**:
  - `Cannot open database file: Permission denied`
  - `Out of memory allocating 1024 bytes`
  - `SSL handshake failed: Connection reset`

### WARNING (Production & Development)  
- **Purpose**: Important issues needing attention but not blocking
- **Audience**: SRE/Operations + Developers
- **Content**: What's concerning, potential impact
- **Action Required**: Investigation recommended
- **Examples**:
  - `Connection attempt exceeded rate limit`
  - `Database file size approaching 90% of limit`
  - `Authentication failed for user: invalid_credentials`

### INFO (Production Default)
- **Purpose**: Key operational events and state changes
- **Audience**: SRE/Operations primarily
- **Content**: What happened operationally
- **Action Required**: Awareness/monitoring
- **Examples**:
  - `Server started on port 5000`
  - `Database loaded with 125,000 documents`
  - `Index created on collection.field`

### DEBUG (Development)
- **Purpose**: Detailed troubleshooting information
- **Audience**: Developers
- **Content**: Program flow, state details
- **Action Required**: Development/debugging
- **Examples**:
  - `Processing query with 3 conditions`
  - `Cache hit for key: user_123`
  - `Thread pool assigned worker 5`

### TRACE (Development + Categories)
- **Purpose**: Very detailed execution flow
- **Audience**: Developers (specific functionality)
- **Content**: Step-by-step execution details
- **Action Required**: Deep debugging
- **Categories**: DATABASE, RBAC, API, AUTH, TRANSACTION, BINARY, JAVASCRIPT, NETWORK, METRICS, MEMORY

## Message Content Standards

### DO:
- Start with uppercase letter
- Use present tense action verbs
- Include specific details in parameters
- Keep messages concise and actionable
- Focus on WHAT happened, not HOW

### DON'T:
- Use redundant prefixes ("Failed to", "Successfully")
- Include function names (already in log format)
- Use implementation-specific jargon in ERROR/WARNING
- Mix log levels inappropriately
- Include redundant context information

### Message Patterns

**Good Examples**:
```c
LOG_ERROR("Cannot connect to database: %s", error_msg);
LOG_WARNING("Query took %dms, exceeding threshold", duration);
LOG_INFO("Created collection '%s' with %d documents", name, count);
LOG_DEBUG("Validating document with %d fields", field_count);
TRACE_DB("Examining field '%s' with value type %d", field, type);
```

**Bad Examples**:
```c
LOG_ERROR("Failed to connect to database: %s", error_msg);  // Redundant "Failed to"
LOG_INFO("Successfully created collection '%s'", name);      // Redundant "Successfully"
LOG_DEBUG("db_insert_document: processing doc");            // Function name redundant
LOG_INFO("TRACE_HANDLER_LOOP: Processing request");         // Wrong level + prefix
```

## Trace Category Usage

### Database Operations (TRACE_DB)
```c
TRACE_DB("Acquiring read lock on collection '%s'", name);
TRACE_DB("Document found at position %d", position);
TRACE_DB("Index lookup returned %d results", count);
```

### RBAC Operations (TRACE_RBAC)
```c
TRACE_RBAC("Checking permission '%s' for user '%s'", perm, user);
TRACE_RBAC("Role '%s' has %d permissions", role, count);
TRACE_RBAC("Session expires at %ld", expiry);
```

### API Operations (TRACE_API)
```c
TRACE_API("Parsing HTTP request with %d headers", header_count);
TRACE_API("Route matched: %s %s", method, path);
TRACE_API("Response content-type: %s", content_type);
```

### Binary Operations (TRACE_BIN)
```c
TRACE_BIN("Serializing %d collections to binary format", count);
TRACE_BIN("Reading TLV header: type=%d, length=%d", type, length);
TRACE_BIN("CRC32 checksum validated: expected=%x, actual=%x", exp, act);
```

### Authentication (TRACE_AUTH)
```c
TRACE_AUTH("Validating JWT token with %d claims", claim_count);
TRACE_AUTH("Session lookup for token hash: %s", hash);
TRACE_AUTH("Password hash comparison for user '%s'", user);
```

## Performance Considerations

### Zero-Cost Disabled Logging
```c
// Efficient - no parameter evaluation when disabled
TRACE_DB("Complex calculation result: %d", expensive_function());

// The logger checks level before evaluating parameters
```

### Thread Safety
- All logging is thread-safe via pthread_mutex
- No additional locking needed in application code
- PID:TID included automatically for correlation

## Configuration Methods

### 1. Environment Variables (Lowest Priority)
```bash
export JDBX_LOG_LEVEL=DEBUG
export JDBX_TRACE_CATEGORIES=database,rbac,api
```

### 2. Command Line Flags (Medium Priority)
```bash
./jdbxd --log-level DEBUG --trace-categories database,api
```

### 3. Runtime API (Highest Priority)
```bash
# Get current settings
curl http://localhost:5000/api/system/log

# Change log level
curl -X POST http://localhost:5000/api/system/log \
  -H "Content-Type: application/json" \
  -d '{"level": "TRACE", "trace_categories": "database,api"}'
```

## Common Anti-Patterns to Fix

### 1. Redundant Prefixes
```c
// Bad
LOG_ERROR("Failed to allocate memory");
LOG_INFO("Successfully created index");

// Good  
LOG_ERROR("Cannot allocate memory");
LOG_INFO("Created index on field 'name'");
```

### 2. Function Name Repetition
```c
// Bad
LOG_DEBUG("db_insert_document: processing document");

// Good
LOG_DEBUG("Processing document with %d fields", field_count);
```

### 3. Wrong Log Levels
```c
// Bad - too verbose for INFO
LOG_INFO("TRACE_CONN_CREATE: Connection created successfully");

// Good - appropriate for DEBUG
LOG_DEBUG("Connection %lu created for client %s", conn_id, client_ip);
```

### 4. Generic Trace Usage
```c
// Bad - generic trace
LOG_TRACE("Database operation completed");

// Good - category-specific
TRACE_DB("Collection query returned %d documents", count);
```

## Implementation Priority

1. **Phase 1**: Fix log level mismatches and redundant prefixes
2. **Phase 2**: Convert generic LOG_TRACE to category-specific TRACE_*
3. **Phase 3**: Enhance message content for better actionability
4. **Phase 4**: Verify thread safety and performance characteristics

## Verification

All logging changes must:
1. Compile without warnings
2. Maintain existing functionality
3. Follow the established format consistently
4. Use appropriate log levels for target audience
5. Provide actionable information for troubleshooting