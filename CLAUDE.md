# JSONdb Development Guidelines

**Last Updated**: June 9, 2025 (v3.1.1)

## Core Principles

- Do not make minimal implementations. Delete partial concept files and ideas. Focus on:
  1. One source of truth
  2. One build (always result is bin/jsondb_server)
  3. One clear goal
  4. Build with full functionality
  5. Always fix, never regress
  6. Document thoroughly
  7. Repeat the improvement cycle
  8. One Makefile for the project src/Makefile
  9. One main.c for the project src/components/main.c
 10. Components are in src/components
 11. Headers are in src/include
 12. We have impeccable git hygiene!
 13. Guidelines are in docs/guidelines, read them
 14. Maintain zero-warning policy - always compile with -Wall -Wextra
 15. Use proper string handling to prevent buffer overflows
 16. Document all fixes thoroughly for future reference
 17. Never recreate parallel implementations, always integrate and test your fixes directly in the main code
 18. Maintain consistent logging format - avoid redundant prefixes since file/function/line are in log format
 19. Clean workspace regularly - move backup files and temporary scripts to trash/
 20. Verify all changes compile cleanly before committing
 21. Follow three-tier configuration priority: env file (lowest) → binary flags (medium) → database config (highest)
 22. Never hardcode paths, hostnames, or configuration values - all must be configurable
 23. Use production-ready defaults (e.g., 0.0.0.0 for host, not development hostnames)

## Configuration Management Guidelines

The JSONdb server implements a comprehensive three-tier configuration system:

1. **Environment File** (Lowest Priority): `/opt/jsondb/share/config/jsondb.env`
2. **Binary Flags** (Medium Priority): Command-line arguments to jsondb_server
3. **Database Config** (Highest Priority): `_system_config` collection for runtime changes

### Configuration Principles:
- **NO HARDCODED VALUES**: All paths, hostnames, ports, and settings must be configurable
- **PRODUCTION READY**: Use 0.0.0.0 for host binding, not development-specific hostnames
- **RUNTIME FLEXIBILITY**: Database configuration overrides allow live configuration changes
- **SINGLE SOURCE**: Configuration defaults centralized in `src/include/utils/config_defaults.h`

### Thread Pool Configuration:
- **CONFIGURABLE**: Thread pool min/max, queue size, and idle timeout are all configurable
- **DEFAULTS**: Reasonable defaults (4-16 threads) suitable for most deployments
- **SCALABLE**: Can be adjusted per deployment size and resource requirements

## Build and Run Guidelines

1. ONLY BUILD USING THE MAKEFILE in src/ directory:
   ```
   cd /opt/jsondb/src && make
   ```

2. ALWAYS run the server in daemon mode, never in foreground mode:
   ```
   cd /opt/jsondb && build/jsondb_runtime.sh start
   ```

3. ALWAYS interact with the server using the runtime script:
   ```
   # Start server
   build/jsondb_runtime.sh start
   
   # Check status
   build/jsondb_runtime.sh status
   
   # Stop server
   build/jsondb_runtime.sh stop
   ```

4. NEVER run binaries from the build directory directly, only use the runtime script.

5. ALWAYS examine logs for debugging, never rely on stdout/stderr:
   ```
   cat /opt/jsondb/var/jsondb_server.log
   ```
   
6. The server runs on port 5000 by default. You can change this in the runtime script.

7. DO NOT IMPLEMENT MOCK DATA OR DEMO MODE - ALWAYS WORK WITH REAL SERVER DATA.

## Socket Binding Implementation

1. The socket binding implementation follows a specific sequence:
   - First initialize all resources (config, database, RBAC, API)
   - If in daemon mode, daemonize the process
   - Initialize the socket in the final daemon process
   - Initialize the thread pool
   - Run the server main loop

2. The socket is properly bound only in the final daemon process to ensure proper socket state.

3. Socket initialization includes:
   - Creating the socket
   - Setting socket options (SO_REUSEADDR)
   - Properly resolving the hostname
   - Binding the socket
   - Setting the socket to listen state
   - Verifying the socket is properly in listening state

## Modular Initialization System

The server now uses a modular initialization sequence with separate components:

- init_config: Initialize and parse configuration
- init_logger: Set up logging system
- init_database: Initialize database
- init_daemon: Handle daemonization if needed
- init_socket: Set up the server socket
- init_rbac: Initialize role-based access control
- init_api: Set up API routes and handlers
- init_threads: Initialize thread pool
- run_server: Run the main server loop

This modular approach ensures proper sequencing, better error handling, and clear separation of concerns.

## Binary Persistence System

The JSONdb server includes a complete binary persistence system with automatic data saves:

1. **Binary Format**: Database stored in efficient binary format (.jdb) with TLV encoding
2. **Automatic Saves**: Triggered by buffer thresholds (50 operations OR 1MB) and periodic saves (30s)
3. **Thread Safety**: Dedicated persistence thread with proper mutex/condition variable patterns
4. **Error Handling**: API errors on persistence failures with database rollback support
5. **Data Integrity**: CRC32 checksums and magic number verification
6. **Zero Deadlocks**: Fixed serialization locking to prevent thread deadlocks
7. **Buffer Management**: Dynamic allocation with safety margins for complex documents

### Key Implementation Files:
- `src/components/database/persistence.c` - Persistence thread and buffer management
- `src/components/binary/binary_format.c` - Binary serialization/deserialization  
- `src/components/database/simplified_db.c` - Database operations with persistence integration
- `src/include/database/database.h` - Persistence thread structure definitions

### Database File:
- Location: `/opt/jsondb/build/var/database.jdb`
- Format: Binary with magic number 0x4A534442 ("JSDB")
- Persistence: Automatic saves ensure data durability across server restarts

## Adaptive Indexing System (v3.1.0)

The JSONdb server now includes an advanced adaptive indexing system that automatically creates and manages indexes based on query patterns:

1. **Query Pattern Tracking**: All database queries are tracked with field path extraction
2. **Automatic Index Creation**: Background thread analyzes patterns and creates indexes
3. **Dynamic Thresholds**: System collections use lower thresholds for faster indexing
4. **Index Maintenance**: Automatic updates on insert/update/delete operations
5. **Performance Metrics**: ROI tracking, effectiveness scores, and Prometheus integration
6. **Zero Memory Leaks**: Connection handling converted from recursive to loop-based

### Key Implementation Files:
- `src/components/database/query_tracker.c` - Query pattern tracking
- `src/components/database/adaptive_indexer.c` - Automatic index creation
- `src/components/database/index_maintenance.c` - Index update hooks
- `src/components/database/index_metrics.c` - Performance monitoring

### Configuration:
- Query threshold: 10 queries (5 for system collections)
- Time threshold: 50ms average (10ms for system collections)
- Startup delay: 30 seconds
- Check interval: 60 seconds

## Index Cleanup System (v3.1.0)

Intelligent index cleanup and optimization:

1. **Performance Analysis**: Identifies underperforming indexes
2. **Automatic Removal**: Removes indexes with negative ROI
3. **Storage Optimization**: Reclaims disk space from unused indexes
4. **Background Processing**: Hourly cleanup checks

### Key Implementation Files:
- `src/components/database/index_cleanup.c` - Cleanup logic
- `src/components/api/index_cleanup_api.c` - REST API endpoints

### Cleanup Criteria:
- ROI threshold: -50% (configurable)
- Effectiveness threshold: 10% (configurable)
- Minimum age: 24 hours before evaluation
- Minimum queries: 100 before evaluation

## Connection Management (v3.1.0)

Fixed critical connection leak in thread-safe handler:

1. **Root Cause**: Recursive keep-alive handler bypassed cleanup
2. **Solution**: Converted to loop-based implementation
3. **Result**: Zero memory growth with unlimited keep-alive requests
4. **Logging**: Comprehensive connection lifecycle tracking

## Metrics System

The metrics system uses a time-series approach with fixed documents:

1. **Collection**: `_system_metrics` (with underscore prefix for system collections)
2. **Fixed Documents**: 5 metric types (operations, performance, cache, memory, connections)
3. **Data Storage**: Append-and-trim pattern with configurable retention (default 15 data points)
4. **Performance**: O(1) updates, no unbounded growth
5. **Document IDs**: Standard format `doc-<timestamp>-<random>` with searchable `name` field

## Logging Standards

The project follows strict logging standards:

1. **Log Levels**:
   - ERROR: Critical failures that prevent normal operation
   - WARNING: Important issues that need attention but don't stop operation
   - INFO: Important operational events (default production level)
   - DEBUG: Detailed information for troubleshooting
   - TRACE: Very detailed execution flow information

2. **Format Rules**:
   - No redundant prefixes (file/function/line are automatic)
   - Clear, actionable messages
   - Include relevant context in parameters
   - Avoid excessive verbosity

3. **Example**:
   ```c
   LOG_ERROR("Failed to open database file: %s", strerror(errno));
   LOG_INFO("Server started on port %d", port);
   LOG_DEBUG("Processing request from %s", client_ip);
   ```

## SSL/TLS Implementation (v3.1.0)

The JSONdb server includes complete SSL/TLS support with production-ready security:

1. **SSL Enforcement**: Properly rejects HTTP connections when SSL enabled (HTTP 400 response)
2. **Large File Support**: Fixed SSL_write buffer handling for files larger than SSL buffer
3. **Connection Stability**: Fixed SSL_read retry logic for non-blocking sockets  
4. **Single Handler**: All connections use unified handle_client() with full SSL support
5. **No Adapters**: Removed all wrapper functions for true single source of truth
6. **SSL Configuration**: Three-tier configuration support (env → CLI → database)
7. **Default Security**: SSL enabled by default on port 5000
8. **Certificate Management**: Supports standard certificate paths (/etc/ssl/certs/, /etc/ssl/private/)
9. **Runtime Selection**: Can be enabled/disabled via environment, CLI flags, or database config
10. **TLS Version Support**: Modern TLS 1.3 with backward compatibility

### SSL Configuration Options:
- Environment: `JSONDB_USE_SSL=true`, `JSONDB_SSL_CERT=/path/to/cert.pem`, `JSONDB_SSL_KEY=/path/to/key.pem`
- CLI Flags: `--ssl`, `--ssl-cert=/path/to/cert.pem`, `--ssl-key=/path/to/key.pem`
- Runtime: Via runtime script or direct binary execution

### Key Implementation Files:
- `src/components/utils/ssl.c` - SSL read/write with proper retry handling
- `src/components/core/handle_client.c` - Unified client handler with SSL support
- `src/initialize/socket.c` - SSL socket initialization and integration
- `src/include/utils/ssl.h` - SSL interface definitions and error handling

## JavaScript Integration System (v2.0.10)

The JSONdb server features a comprehensive JavaScript integration system with enterprise-grade management capabilities:

### Core JavaScript Components

1. **Script Management**:
   - Native JavaScript storage with QuickJS integration
   - Three script types: validators, transformers, custom functions
   - Collections: `_validators`, `_transformers`, `_functions`
   - Error handling with detailed validation and recovery

2. **Version Control System**:
   - Semantic versioning (major.minor.patch) for all scripts
   - Complete version history storage in `_script_versions` collection
   - Rollback capabilities with automatic backup creation
   - Version comparison with code diff generation
   - Change type classification (major/minor/patch changes)

3. **Performance Monitoring**:
   - Real-time execution metrics (execution time, memory usage)
   - Performance history tracking with configurable retention
   - Script optimization recommendations
   - Bottleneck identification and analysis

4. **Batch Operations**:
   - Bulk enable/disable operations across multiple scripts
   - Batch version creation with progress tracking
   - Import/export functionality with version history preservation
   - Bulk delete operations with confirmation workflows

### JavaScript Development Guidelines

1. **Script Structure**:
   ```javascript
   // All scripts should include proper error handling
   function validateDocument(doc) {
     try {
       // Validation logic with detailed error reporting
       if (!doc.email || !validateEmail(doc.email)) {
         addError('email', 'Invalid email format');
       }
       return isValid;
     } catch (error) {
       logError('Validation failed', error);
       return false;
     }
   }
   ```

2. **Performance Best Practices**:
   - Use `performance.now()` for timing critical operations
   - Implement proper memory management
   - Avoid blocking operations in script execution
   - Leverage built-in optimization hints

3. **Version Management Workflow**:
   - Create versions before making significant changes
   - Use semantic versioning appropriately:
     - Patch (x.x.+1): Bug fixes, minor improvements
     - Minor (x.+1.0): New features, backward compatible
     - Major (+1.0.0): Breaking changes, API modifications
   - Include descriptive change descriptions
   - Test rollback procedures regularly

4. **Error Handling Standards**:
   - Always provide meaningful error messages
   - Use structured error reporting with `addError(field, message)`
   - Include context information in error logs
   - Implement graceful degradation for non-critical failures

### Key Implementation Files

#### Core JavaScript Integration:
- `src/components/js/js_engine.c` - QuickJS engine management
- `src/components/js/js_api.c` - JavaScript API bindings
- `src/components/js/js_native_storage.c` - Native storage integration
- `src/components/database/js_integration.c` - Database integration layer
- `src/components/api/js_native_api.c` - REST API endpoints

#### Browser Interface:
- `share/htdocs/js/app.js` - Complete JavaScript management system
- `share/htdocs/index.html` - UI components and modals
- `docs/guides/javascript-development-guide.md` - Development documentation

#### Examples and Documentation:
- `share/examples/js-examples/` - Comprehensive script examples
- `share/examples/js-examples/comprehensive_examples.js` - Production-ready examples
- `share/examples/js-examples/comprehensive_test.js` - Testing framework integration

### JavaScript Integration Workflow

1. **Development Phase**:
   - Write scripts using provided examples and templates
   - Test scripts in development environment
   - Use performance monitoring to optimize execution
   - Document script functionality and dependencies

2. **Version Management**:
   - Create initial version (1.0.0) for new scripts
   - Use appropriate version increments for changes
   - Maintain detailed change descriptions
   - Test rollback procedures

3. **Production Deployment**:
   - Use batch operations for multiple script updates
   - Monitor performance metrics after deployment
   - Implement gradual rollout for critical scripts
   - Maintain backup versions for quick recovery

4. **Maintenance**:
   - Regular performance analysis and optimization
   - Periodic cleanup of old versions (configurable retention)
   - Security audits of script content
   - Documentation updates for API changes

## Code Audit Process

Regular code audits ensure quality:

1. **Check Git Status**: Identify uncommitted changes
2. **Verify Single Source**: Eliminate duplicate implementations
3. **Check Timestamps**: Find missed integrations
4. **Clean Build**: Ensure zero warnings with -Wall -Wextra
5. **Clean Workspace**: Move redundant files to trash/
6. **Update Documentation**: Keep READMEs current
7. **Commit Changes**: Use descriptive commit messages
8. **Tag Releases**: When appropriate with comprehensive messages

## Recent Updates (v3.1.1 - June 9, 2025)

### Code Quality Improvements
1. **Zero Compiler Warnings**: Achieved clean compilation with -Wall -Wextra
   - Fixed all unused parameter warnings with proper (void) casts
   - Resolved implicit function declaration warnings
   - Fixed string truncation warnings by increasing buffer sizes
   - Added missing function declarations to headers

2. **Workspace Hygiene**: Comprehensive cleanup
   - Removed all object files (.o) from source directories
   - Cleaned up temporary files and build artifacts
   - Moved obsolete patch files to trash/
   - Removed duplicate js_extensions directory

3. **Single Source of Truth**: Maintained strict adherence
   - No duplicate implementations
   - All patches integrated into main codebase
   - Clean git status with proper file organization

### Previous Updates (v3.1.0 - June 8, 2025)
1. **Adaptive Indexing System**: Automatic index creation based on query patterns
2. **Connection Leak Fix**: Resolved critical memory growth in keep-alive connections
3. **Configuration Management**: Complete three-tier configuration system
4. **Logging Standards**: 100% compliance with redundant prefix removal
5. **Documentation Accuracy**: Fixed version inconsistencies, default values, and API documentation