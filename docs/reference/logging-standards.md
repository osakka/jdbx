# JDBX Logging Standards

**Last Updated**: January 31, 2025

## Overview

JDBX uses a unified, high-performance logging system designed for both development and production environments. The system supports dynamic log level control, trace-by-functionality, and consistent message formatting.

## Log Format

All log messages follow this standardized format:
```
[timestamp] [level] [pid:tid] [filename:line:function] message
```

**Example**:
```
[2025-01-31 14:30:25] [INFO] [1234:5678] [server:120:init_server] Server started on port 5000
```

## Log Levels

### Production Levels
- **ERROR**: Critical failures that prevent normal operation
- **WARNING**: Important issues needing attention but don't stop operation  
- **INFO**: Key operational events (default production level)

### Development Levels
- **DEBUG**: Detailed troubleshooting information
- **TRACE**: Very detailed execution flow (with trace categories)

## Trace Categories

When TRACE level is enabled, use trace categories to control verbosity:

- `TRACE_DATABASE` - Database operations and persistence
- `TRACE_RBAC` - Role-based access control and permissions
- `TRACE_API` - API request/response processing
- `TRACE_AUTH` - Authentication and JWT operations
- `TRACE_TRANSACTION` - Transaction management
- `TRACE_BINARY` - Binary format serialization
- `TRACE_JAVASCRIPT` - JavaScript engine operations
- `TRACE_NETWORK` - Network and HTTP operations
- `TRACE_METRICS` - Performance metrics collection
- `TRACE_ALL` - Enable all trace categories

### Trace Macros
Use category-specific macros for trace logging:
```c
TRACE_DB("Processing query for collection: %s", collection_name);
TRACE_AUTH("JWT token validation successful for user: %s", user_id);
TRACE_API("Handling %s request to %s", method, path);
```

## Dynamic Configuration

### Command Line Flags
```bash
# Set log level
./jdbxd --log-level=debug

# Enable trace categories
./jdbxd --log-level=trace --trace-categories="database,auth"
```

### Environment Variables
```bash
export JDBX_LOG_LEVEL="debug"
export JDBX_TRACE_CATEGORIES="database,rbac,api"
./jdbxd
```

### Runtime API Control
```bash
# Get current log configuration
curl -X GET http://localhost:5000/api/system/log-control

# Change log level to debug
curl -X POST http://localhost:5000/api/system/log-control \
  -H "Content-Type: application/json" \
  -d '{"log_level": "debug"}'

# Enable database and auth tracing
curl -X POST http://localhost:5000/api/system/log-control \
  -H "Content-Type: application/json" \
  -d '{"log_level": "trace", "trace_categories": "database,auth"}'
```

## Message Guidelines

### Appropriate Log Levels

#### ERROR Messages
- System failures that prevent operation
- Database corruption or inaccessible
- Network binding failures
- Authentication system failures

```c
LOG_ERROR("Database initialization failed: %s", strerror(errno));
LOG_ERROR("Server socket bind failed on port %d: %s", port, strerror(errno));
```

#### WARNING Messages  
- Configuration issues that don't prevent startup
- Deprecated functionality usage
- Resource limits approaching
- Authentication failures (invalid credentials)

```c
LOG_WARNING("Database load failed, creating new database");
LOG_WARNING("Invalid login attempt for user: %s", username);
```

#### INFO Messages
- Server startup/shutdown
- Major state changes
- User authentication successes
- Database connections/disconnections

```c
LOG_INFO("Server started on port %d", port);
LOG_INFO("User authenticated: %s", username);
LOG_INFO("Database initialized: %s", db_path);
```

#### DEBUG Messages
- Request processing details
- Configuration loading
- Document operations
- Performance metrics

```c
LOG_DEBUG("Processing authentication for user: %s", username);
LOG_DEBUG("Document inserted with ID: %s", document_id);
```

#### TRACE Messages
- Detailed execution flow
- Function entry/exit
- Variable states
- Step-by-step processing

```c
TRACE_DB("Creating document copy");
TRACE_AUTH("JWT token validation starting");
TRACE_API("Parsing request body with %zu bytes", body_size);
```

### Message Format Rules

1. **No redundant prefixes** - filename:function already provided
   ```c
   // WRONG
   LOG_ERROR("JWT_DEBUG: Token validation failed");
   
   // CORRECT  
   LOG_ERROR("Token validation failed: %s", error_message);
   ```

2. **Clear, actionable messages**
   ```c
   // WRONG
   LOG_ERROR("create document copy");
   
   // CORRECT
   LOG_ERROR("Document insertion failed: unable to create document copy");
   ```

3. **Include relevant context**
   ```c
   // WRONG
   LOG_ERROR("Invalid parameters");
   
   // CORRECT
   LOG_ERROR("Document insertion failed: invalid parameters (db=%p, collection=%s)", db, collection);
   ```

4. **Use appropriate audience language**
   - **Production (INFO/WARN/ERROR)**: Business/operational terms
   - **Development (DEBUG/TRACE)**: Technical implementation details

## Performance Considerations

- Log messages above the current level incur **zero CPU overhead**
- Trace categories are checked only when TRACE level is active
- Use format strings efficiently - avoid complex computations in disabled log calls
- String formatting only occurs when the message will actually be logged

## Best Practices

1. **Use appropriate levels** for target audience
2. **Include error context** with errno/strerror when applicable  
3. **Avoid excessive verbosity** at INFO level
4. **Use trace categories** to control development logging granularity
5. **Test log output** at different levels during development
6. **Monitor log volume** in production environments

## Example Usage

```c
#include "utils/logger.h"

// Production logging
LOG_INFO("User session created: %s", session_id);
LOG_WARNING("Rate limit exceeded for user: %s", user_id);
LOG_ERROR("Database connection failed: %s", strerror(errno));

// Development logging  
LOG_DEBUG("Processing request with %zu bytes", content_length);

// Trace logging with categories
TRACE_DB("Acquiring database lock for collection: %s", collection);
TRACE_AUTH("Validating JWT claims for user: %s", user_id);
TRACE_API("Building HTTP response with status %d", status_code);
```