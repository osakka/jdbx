# JSONdb Logging Improvement Plan

This document outlines the plan for enhancing logging throughout the JSONdb codebase to improve debuggability, monitoring, and operational visibility.

## Logging Levels

JSONdb uses the following logging levels, in order of increasing verbosity:

1. **ERROR** (LOG_LEVEL_ERROR): Critical errors that prevent normal operation
2. **WARNING** (LOG_LEVEL_WARNING): Issues that don't prevent operation but indicate problems
3. **INFO** (LOG_LEVEL_INFO): Significant events in normal operation
4. **DEBUG** (LOG_LEVEL_DEBUG): Detailed information useful for debugging
5. **TRACE** (LOG_LEVEL_TRACE): Extremely detailed tracing information

## Logging Enhancement Goals

1. **Comprehensive Coverage**: Ensure all components have adequate logging
2. **Appropriate Level Usage**: Use the right level for each message type
3. **Contextual Information**: Include relevant context in log messages
4. **Consistent Formatting**: Maintain consistent message formatting
5. **Performance Awareness**: Minimize logging overhead in performance-critical paths

## Guidelines for Each Log Level

### ERROR
- Function failures that cannot be recovered from
- Critical resource allocation failures
- Security violations
- Data corruption detection
- API call failures that affect reliability

### WARNING
- Configuration issues that can be worked around
- Deprecated feature usage
- Performance degradation
- Unexpected but handleable conditions
- Security-relevant events that don't indicate compromise

### INFO
- Service startup/shutdown
- Configuration settings at startup
- Connection establishment/termination
- User authentication events
- Backup/restore operations
- Transaction commits/rollbacks
- API endpoints being registered

### DEBUG
- Function entry/exit in key functions
- Values of important variables
- Resource allocation/deallocation
- Detailed database operations
- Cache operations
- Configuration processing steps
- Path resolution details

### TRACE
- Internal algorithm steps
- Very frequent events
- Detailed loop iterations
- Low-level I/O operations
- Memory management details

## Implementation Plan

We'll enhance logging in the following order, working through each module systematically:

1. **Core Components**:
   - Server initialization and lifecycle
   - API framework
   - Configuration handling

2. **Database Components**:
   - Database operations
   - Index management
   - Transaction handling
   
3. **Utility Components**:
   - JSON handling
   - Caching
   - Reference counting

4. **Feature Components**:
   - Authentication/RBAC
   - JavaScript integration
   - Backup/restore

## Checklist for Each File

For each file, we'll:

1. ✅ Review existing logging
2. ✅ Identify gaps in logging coverage
3. ✅ Add appropriate ERROR and WARNING logs for error conditions
4. ✅ Add INFO logs for significant events
5. ✅ Add DEBUG logs for detailed operation information
6. ✅ Add TRACE logs for fine-grained debugging
7. ✅ Ensure consistent formatting and sufficient context

## Files to Enhance

### Core Components
- [ ] components/main.c
- [ ] components/core/server.c
- [ ] components/core/api.c
- [x] components/utils/config_loader.c

### Database Components
- [ ] components/database/database.c
- [ ] components/database/index.c
- [ ] components/database/index_optimized.c
- [ ] components/database/schema.c
- [ ] components/database/simplified_db.c
- [ ] components/transaction/transaction.c
- [ ] components/transaction/transaction_log.c
- [ ] components/query/query_language.c

### Utility Components
- [ ] components/utils/json.c
- [ ] components/utils/json_helpers.c
- [ ] components/utils/cache.c
- [ ] components/utils/ref_counter.c
- [ ] components/utils/ref_json.c
- [ ] components/utils/logger.c

### Feature Components
- [ ] components/rbac/rbac.c
- [ ] components/rbac/jwt.c
- [ ] components/js/js_engine.c
- [ ] components/js/js_api.c
- [ ] components/api/backup_api.c
- [ ] components/api/import_export_api.c

## Progress Tracking

We'll update this document as we enhance each file, marking them as completed and noting any special considerations or patterns identified along the way.

### Completed Files

#### config_loader.c

**Enhancements:**
- Added TRACE logs for function entry/exit in all functions
- Added DEBUG logs for detailed configuration processing
- Added WARNING logs for security-sensitive configurations (JWT, CORS, etc.)
- Added proper error logging with context for file operations
- Added INFO logs for significant events like configuration loading
- Enhanced path resolution logging
- Added security-conscious logging that avoids exposing secrets at higher levels
- Fixed an unbalanced brace issue that was preventing compilation

**Patterns Used:**
- Function entry/exit pattern using LOG_TRACE
- Conditional logging with g_logger checks
- Fallback to stderr for critical errors when logger might not be initialized
- Detailed context in error messages (parameter values, return values)
- Security-sensitive information only at TRACE level