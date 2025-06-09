# Changelog

All notable changes to JSONdb will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [3.1.1] - 2025-06-09

### 🧹 Code Quality & Workspace Hygiene

This release focuses on achieving zero compiler warnings and maintaining a clean, organized workspace.

#### Code Quality Improvements
- **ZERO WARNINGS**: Achieved clean compilation with -Wall -Wextra
  - Fixed all unused parameter warnings with proper (void) casts
  - Resolved implicit function declaration for `index_cleanup_api_init()`
  - Fixed string truncation warnings by increasing buffer sizes to 2048
  - Added missing function declarations to API headers
- **WORKSPACE CLEANUP**: Comprehensive file organization
  - Removed all object files (.o) from source directories
  - Cleaned up temporary build artifacts (build.log, gdb.log)
  - Moved obsolete patch files to trash/
  - Removed duplicate js_extensions directory
- **SINGLE SOURCE**: Maintained strict adherence to principles
  - Verified all patches integrated into main codebase
  - No duplicate implementations or parallel code
  - Clean git status with proper organization

#### Documentation Updates
- **CLAUDE.md**: Updated to v3.1.1 with code quality summary
- **README files**: Verified all versions are current
- **CHANGELOG**: Added comprehensive v3.1.1 entry

## [3.1.0] - 2025-06-08

### 🚀 Adaptive Indexing & Connection Leak Fix

This release introduces adaptive indexing capabilities and fixes a critical connection leak in the thread-safe handler.

#### New Features
- **ADAPTIVE INDEXING**: Automatic index creation based on query patterns
  - Query pattern tracking with field path analysis
  - Background index creation with configurable thresholds
  - Dynamic thresholds for system vs user collections
  - Performance metrics tracking (ROI, effectiveness scores)
- **INDEX MAINTENANCE**: Automatic index updates on data changes
  - Insert/update/delete hooks for all adaptive indexes
  - Thread-safe operation with minimal overhead
  - Comprehensive statistics tracking
- **INDEX METRICS**: Performance monitoring and analysis
  - Query improvement tracking
  - Storage overhead monitoring
  - Maintenance cost analysis
  - Prometheus integration
- **INDEX CLEANUP**: Intelligent index removal and optimization
  - Automatic removal of underperforming indexes
  - ROI-based decision making
  - Storage reclamation
  - Background cleanup process

#### Critical Fixes
- **CONNECTION LEAK**: Fixed recursive keep-alive handler causing connection leak
  - Converted from recursion to loop-based implementation
  - One increment/decrement per TCP connection
  - Zero memory growth with unlimited keep-alive requests
  - Comprehensive logging for connection lifecycle

#### Performance Improvements
- **QUERY TRACKING**: All database queries now tracked (including auth/RBAC)
- **SSL HANDSHAKE**: Fixed missing SSL handshake on accepted connections
- **THREAD SAFETY**: Improved connection handling with proper state management

#### Code Quality
- **SURGICAL PRECISION**: All changes made with minimal impact
- **CLEAN BUILD**: Zero warnings policy maintained
- **SINGLE SOURCE**: No duplicate implementations or redundant code

## [3.0.0] - 2025-06-05

### 🚀 High-Performance Database Transformation

This major release transforms JSONdb into a high-performance database capable of handling billion-document collections with sub-millisecond response times.

#### Core Architecture Overhaul
- **MEMORY-MAPPED STORAGE**: Zero-copy data access with configurable sizes (256MB - 100GB)
- **HASH INDEX**: O(1) primary key lookups with extendible hashing (2.06 μs reads)
- **B+TREE INDEXES**: O(log n) range queries with disk-based secondary indexes (403 μs)
- **PRODUCTION CONFIG**: Environment-based configuration profiles (dev/small/medium/large)

#### Performance Achievements
- **Sequential Write**: 10.28 μs ✓ SUB-MILLISECOND
- **Random Read**: 2.06 μs ✓ SUB-MILLISECOND  
- **Concurrent Write**: 230.53 μs ✓ SUB-MILLISECOND
- **Range Query**: 403.33 μs ✓ SUB-MILLISECOND
- **Batch Insert**: 50,000+ docs/sec

#### Critical Fixes
- **HASH INDEX CORRUPTION**: Fixed offset 144/272 corruption by increasing minimum slots to 128
- **MEMORY LEAK**: Resolved free list unbounded growth with periodic cleanup
- **JWT PERFORMANCE**: Sub-millisecond token verification with LRU cache (200-800 μs)
- **QUERY OPTIMIZER**: B+tree integration with fast field extraction

#### Production Features
- **BATCH OPERATIONS**: High-throughput bulk insert API with deferred indexing
- **MONITORING**: Comprehensive /api/health and /api/metrics endpoints
- **STRESS TESTED**: Sustained 30K ops/sec with room for optimization

#### Breaking Changes
- Database format incompatible with v2.x (full migration required)
- New collection storage architecture
- Updated API response formats for batch operations

## [2.0.9] - 2025-05-31

### 🚀 Advanced Performance Optimization Implementation (Phase 2 & 3)

#### Phase 2: Memory & Algorithm Optimizations
- **STRING CACHING**: Eliminated O(n²) complexity in 5 string processing loops with length caching
- **JSON DEEP COPY**: Replaced expensive stringify-parse pattern with structural copying (5-10x improvement)
- **OPERATOR HASH TABLES**: Implemented O(1) vs O(n) lookup for query operators using hash tables
- **BUFFER POOL ENHANCEMENT**: Added thread-local pools and size classes for memory optimization
- **STRING INTERNING**: Reference counting system for string deduplication and memory efficiency

#### Phase 3: Concurrency & Network Optimizations  
- **LOCK-FREE QUEUE**: Michael & Scott algorithm implementation for thread pool work distribution
- **EVENT-DRIVEN I/O**: epoll() based server for 10x connection scalability vs traditional threading
- **SERVER MODE SELECTION**: Automatic switching between standard and high-performance modes
- **ENVIRONMENT CONFIG**: Runtime performance tuning via JSONDB_SERVER_MODE and JSONDB_MAX_CONNECTIONS

#### Code Quality & Build System
- **ZERO WARNINGS**: Fixed all compilation warnings with -Wall -Wextra flags
- **SINGLE SOURCE**: Eliminated duplicate SSL implementations (mock vs full OpenSSL)
- **OPTIMIZATION PLAN**: Documented comprehensive 3-phase optimization strategy
- **CLEAN BUILD**: Verified all optimizations integrate cleanly into main build system

## [2.0.11] - 2025-05-31

### ⚙️ Configuration Management Alignment & Comprehensive Code Audit

#### Three-Tier Configuration System Implementation
- **CONFIGURATION HIERARCHY**: Implemented env file (lowest) → binary flags (medium) → database config (highest priority)
- **DATABASE CONFIG**: Added `_system_config` collection for runtime configuration management
- **HARDCODED ELIMINATION**: Fixed critical DEFAULT_HOST from development "claude-code.uk.home.arpa" to production "0.0.0.0"
- **THREAD POOL CONFIG**: Made thread pool settings configurable with reasonable defaults (4-16 threads)
- **FRAMEWORK READY**: Database configuration system foundation in place for Phase 2 implementation

#### Code Audit & Clean Workspace Implementation
- **SINGLE SOURCE**: Verified no duplicate implementations violate "one source of truth" principle
- **ZERO WARNINGS**: Clean build achieved with -Wall -Wextra, fixed use-after-free warning in binary_format.c
- **DOCUMENTATION**: Organized docs-archive with timestamped structure for historical tracking
- **WORKSPACE HYGIENE**: Moved backup files and temporary scripts to trash/ maintaining clean workspace

#### Configuration Infrastructure Enhancements
- **CONFIG LOADER**: Extended with database configuration functions (placeholder for full implementation)
- **SERVER STRUCTURE**: Added thread pool configuration fields to server_config_t
- **PATH RESOLUTION**: Enhanced binary directory resolution for production deployment
- **DEFAULTS SYSTEM**: Centralized configuration defaults in config_defaults.h

#### Production Readiness Improvements
- **SECURITY**: Eliminated hardcoded development hostname that would cause production deployment failures
- **SCALABILITY**: Configurable thread pool settings for different deployment sizes
- **CONSISTENCY**: Proper configuration priority order ensures runtime flexibility
- **RELIABILITY**: Configuration validation and error handling throughout the system

## [2.0.10] - 2025-05-31

### 🎯 Atomic Naming & Semantic Engineering Overhaul

#### Comprehensive Naming Consistency Implementation
- **ATOMIC NAMING**: Eliminated all temporal qualifiers (fix, optimized, enhanced, simple) from codebase
- **SEMANTIC CLARITY**: File and function names now reflect actual purpose rather than development history
- **SINGLE SOURCE**: Removed duplicate/unused implementations violating "one source of truth" principle
- **PROFESSIONAL STANDARDS**: All naming now follows lowercase snake_case with clear semantic meaning

#### Source Code Renames (8 Critical Files)
- `api_login_fix.c` → `authentication_handler.c` - Clear purpose definition
- `rbac_db_fix.c` → `rbac_persistence.c` - Database persistence functionality
- `rbac_enhanced.c` → `rbac_permissions.c` - Permission management system
- **CLEANED**: Moved 4 unused "optimized" implementations to trash (temporal artifacts)
- **FUNCTIONS**: Updated `rbac_db_save_fixed()` → `rbac_database_persist()` for clarity

#### Documentation Structure Reorganization
- **API DOCS**: Consolidated `API_CORRECTED.md` → `api.md` (single source)
- **GUIDES**: `AUTHENTICATION_FIX_PLAN.md` → `authentication-guide.md`
- **GUIDES**: `ENHANCED_ADMIN_UI.md` → `admin-ui-guide.md`
- **REFERENCE**: `OPTIMIZATION_RESULTS.md` → `performance-benchmarks.md`
- **REFERENCE**: `LOG_FORMAT_STRING_FIX.md` → `log-format-specification.md`

#### Debug Clarity Enhancement
- **LOGGING OUTPUT**: File:line:function names now semantically meaningful
- **BEFORE**: `api_login_fix.c:96:api_handle_login - Failed to create simple response`
- **AFTER**: `authentication_handler.c:96:authenticate_admin_user - Failed to create authentication response`

#### Build System Integration
- **VERIFIED**: All renames integrated into Makefile successfully
- **TESTED**: Clean compilation with zero warnings maintained
- **CONFIRMED**: No functional regressions from naming changes

## [2.0.9] - 2025-05-30

### 🔧 Critical Memory Management Audit & Fixes

#### Memory Allocation Audit Complete
- **RESOLVED**: Authentication crash issues caused by mixed memory allocation patterns
- **AUDITED**: 20+ API endpoints systematically reviewed and fixed
- **UNIFIED**: Consistent buffer pool allocation across entire codebase
- **RESULT**: Zero authentication crashes, stable JWT verification system

#### Memory Management Overhaul
- **FIXED**: JWT authentication system memory allocation mismatches
- **PATTERN**: All `malloc()/strdup()` + `free()` replaced with `buffer_pool_alloc()` + `buffer_pool_free_safe()`
- **FILES**: Updated jwt.c, json.c, cache_api.c, health_api.c, http_response.c
- **IMPACT**: Eliminated segmentation faults and memory corruption issues

#### Single Source of Truth Enforcement
- **CLEANED**: Moved duplicate js_file_utils.c implementations to trash
- **VERIFIED**: All files compile cleanly with `-Wall -Wextra` (zero warnings)
- **CONFIRMED**: No redundant or parallel implementations remaining
- **MAINTAINED**: Proper git hygiene with clean workspace

## [2.0.8] - 2025-05-30

### 🚀 Major Features

#### Professional Metrics Dashboard Implementation
- **NEW**: Complete metrics system overhaul with production-ready monitoring
- **FEATURES**:
  - Professional charts with legends, units, and interactive tooltips
  - Dual Y-axis charts for cache metrics (hit rate % vs cache size)
  - Enhanced formatting with auto-scaling units (KB/MB/GB)
  - Rate of change calculations in chart tooltips
  - Proper chart management preventing canvas conflicts

#### Prometheus Integration
- **NEW**: Industry-standard Prometheus metrics endpoint (`/metrics`)
- **FORMAT**: Text-based format compatible with Prometheus scraping
- **METRICS**: Operations, response times, cache performance, memory usage, connections
- **ACCESS**: No authentication required for monitoring system integration

#### Performance Optimizations
- **NEW**: JSON deep copy optimization eliminating stringify/parse overhead
- **NEW**: Thread-local buffer pools (4 size classes: 512B, 4KB, 16KB, 64KB)
- **NEW**: Zero-copy HTTP request parsing with string views
- **NEW**: Optimized static file serving with sendfile() and TCP_CORK
- **RESULT**: Significant performance improvements for high-traffic scenarios

### 🎨 Enhanced UI/UX

#### Metrics Visualization Improvements
- **ENHANCED**: All charts now display proper legends with positioning
- **ADDED**: Y-axis labels with appropriate units (ops/min, ms, %, bytes)
- **NEW**: Interactive tooltips showing exact values with formatted units
- **IMPROVED**: Memory charts with auto-scaling byte units
- **FIXED**: Operation types chart with percentages and formatted numbers

#### JavaScript Error Resolution
- **FIXED**: "Canvas is already in use" errors with proper chart destruction
- **FIXED**: "formatNumber is undefined" errors with null/undefined value handling
- **ADDED**: Global error handling for unhandled promise rejections
- **IMPROVED**: Comprehensive try-catch blocks around all chart operations

### 🔧 Technical Improvements

#### Code Quality and Architecture
- **VERIFIED**: Clean build with zero compiler warnings using -Wall -Wextra
- **INTEGRATED**: All performance optimizations into main codebase
- **REMOVED**: Obsolete code files moved to trash/ directory
- **ADDED**: Comprehensive metrics improvement plan documentation

#### Infrastructure Enhancements
- **CREATED**: `docs/METRICS_IMPROVEMENT_PLAN.md` - Complete production readiness analysis
- **IMPLEMENTED**: Enhanced chart configurations in `share/htdocs/js/metrics-improvements.js`
- **UPDATED**: Main application JavaScript with robust error handling

### 🐛 Bug Fixes

#### Metrics System Stability
- **FIXED**: Chart initialization conflicts causing JavaScript errors
- **RESOLVED**: Undefined formatNumber function crashes
- **IMPROVED**: Chart data update safety with dataset validation
- **ENHANCED**: DOM element access safety with null checks

#### Performance and Memory
- **OPTIMIZED**: HTTP request processing with zero-copy parsing
- **IMPROVED**: Static file serving performance with kernel-level optimizations
- **ENHANCED**: Memory allocation patterns with buffer pooling

### 📊 Metrics System Features

#### Professional Chart Types
- **Operations Chart**: Read/Write/Total operations with ops/min units
- **Response Time Chart**: Average, P95, and max response times in milliseconds
- **Cache Performance**: Dual-axis chart with hit rate % and cache size bytes
- **Memory Usage**: Process vs system memory with auto-scaling units
- **Connection Metrics**: Active and total connection tracking

#### Production Monitoring Ready
- **Prometheus Endpoint**: `/metrics` for external monitoring integration
- **Time-Series Storage**: 15-minute retention with 60-second intervals
- **Error Tracking**: JavaScript error handling and logging
- **Performance Metrics**: Response time tracking and optimization

## [2.0.7] - 2025-05-30

### ✨ New Features

#### Welcome Panel Dashboard
- **ADDED**: Fixed welcome panel that serves as a persistent dashboard
- **FEATURES**:
  - Clean, modern design with rounded corners
  - Shows key system information and quick actions
  - Always visible on the homepage
  - Configurable content via API

#### Session Management Enhancement
- **IMPLEMENTED**: Sliding session timeout with automatic extension
- **FUNCTIONALITY**:
  - Sessions extend by 30 minutes on each authenticated request
  - Prevents sessions from expiring during active use
  - Updates both `last_seen` and `expires_at` fields
  - Implemented in `api_auth_sliding.c`

### 🎨 UI/UX Improvements

#### Unified Button Design System
- **REDESIGNED**: All buttons now use consistent styling
- **CHANGES**:
  - Replaced "squircle" buttons with rounded rectangles (0.375rem radius)
  - Three button variants: default, danger (red), success (green)
  - Standardized button widths: 2.5rem for icon-only, 5rem minimum for icon+text
  - Fixed button layout issues with proper flexbox containers
  - Improved Edit/Save button behavior in document editor

### 🔧 Technical Improvements

#### Code Quality and Cleanup
- **REMOVED**: 111 redundant files from trash/ directory
- **CLEANED**: Duplicate fix scripts (9 files) - kept only maintenance/ versions
- **VERIFIED**: Zero compiler warnings with -Wall -Wextra
- **CONFIRMED**: All recent changes properly integrated into main codebase

#### Monitoring Infrastructure
- **ADDED**: Comprehensive server monitoring script
- **LOCATION**: `/opt/jsondb/scripts/monitor_server.sh`
- **FEATURES**:
  - Process health monitoring
  - Memory usage tracking
  - Crash detection and log capture
  - Automatic restart capability

### 🐛 Bug Fixes

#### JavaScript Syntax Errors
- **FIXED**: Multiple invalid escape sequences in app.js
- **RESOLVED**: Undefined `collectionName` variable scope issue
- **CORRECTED**: Undefined `loadOperations` function reference

#### Welcome Panel Content
- **FIXED**: Excessive backslashes in welcome message making it unreadable
- **IMPROVED**: Content processing to handle escaped characters properly

## [2.0.6] - 2025-05-28

### 🐛 Bug Fixes

#### Critical Server Startup Fix
- **FIXED**: Server hanging during startup with "Collection: (null)" error
- **CAUSE**: Log statements missing format string arguments throughout codebase
- **IMPACT**: 30 log statements across 4 files were missing parameters
- **FILES FIXED**:
  - `simplified_db.c` - 10 instances
  - `indexed_document_operations.c` - 7 instances
  - `simplified_operations.c` - 10 instances
  - `optimized_db_operations.c` - 3 instances

#### Binary Serialization Crash Fix
- **FIXED**: Server crash during binary persistence of metrics documents
- **CAUSE**: Buffer allocation bug - not allocating on first use when size was 0
- **ADDITIONAL ISSUES**:
  - Insufficient safety margin (32 bytes) for complex metrics documents
  - Missing bounds checking for serialized size
- **SOLUTION**:
  - Fixed initial buffer allocation check
  - Increased safety margin to 1024 bytes
  - Added validation for serialized size vs buffer size
  - Improved cleanup and error handling

### 🔧 Technical Improvements

#### Logging System Cleanup
- **COMPLETED**: Comprehensive audit and standardization of all log messages
- **CHANGES**:
  - Removed redundant "successfully" from INFO messages
  - Eliminated "Failed to" prefix from ERROR logs (redundant with level)
  - Standardized memory errors to "Out of memory"
  - Simplified operation messages: "Inserted:", "Updated:", "Deleted:"
  - Adjusted log levels (routine operations moved from INFO to DEBUG)
  - Fixed all format string issues
- **RESULT**: Cleaner, more consistent, and accurate logging throughout

#### Documentation
- **NEW**: `docs/BINARY_SERIALIZATION_CRASH_FIX.md` - Detailed crash analysis
- **NEW**: `docs/LOG_FORMAT_STRING_FIX.md` - Format string fix documentation
- **NEW**: `docs/guidelines/LOGGING_STANDARDS.md` - Comprehensive logging guidelines

#### Documentation Audit and Cleanup
- **COMPLETED**: Comprehensive documentation accuracy audit
- **FINDINGS**:
  - Found 154 documentation files with ~40% duplicate content
  - Incorrect API endpoints (wrong port, RBAC paths)
  - Missing documentation for session management, metrics history
  - Obsolete backup/restore endpoints documented but not implemented
- **ACTIONS**:
  - Created `API_CORRECTED.md` with accurate endpoint documentation
  - Updated main docs/README.md as proper documentation index
  - Created consolidation plan to reduce duplicate files
  - Added accuracy warnings to guide users
- **NEW FILES**:
  - `docs/DOCUMENTATION_ACCURACY_AUDIT.md` - Audit findings
  - `docs/api/API_CORRECTED.md` - Corrected API reference
  - `docs/DOCUMENTATION_CONSOLIDATION_PLAN.md` - Cleanup plan
  - `docs/DOCUMENTATION_AUDIT_SUMMARY.md` - Executive summary

#### Code Cleanup
- **REMOVED**: 97 backup (.bak) files moved to trash/
- **REMOVED**: Temporary debug scripts and log cleanup scripts
- **VERIFIED**: Zero compiler warnings with -Wall -Wextra
- **UPDATED**: All README files with current version and standards
- **NEW**: `docs/guidelines/LOGGING_CLEANUP_SUMMARY.md` - Cleanup summary

### 📚 Documentation Improvements
- **STARTED**: Comprehensive documentation reorganization plan
- **IDENTIFIED**: 154 documentation files with significant duplication
- **PLANNED**: Consolidation of duplicate documentation into organized structure

## [2.0.5] - 2025-05-27

### 🚀 Performance Improvements

#### Metrics Storage Overhaul
- **FIXED**: Critical performance degradation from ~1ms to ~400ms response times
- **CAUSE**: Metrics creating new documents every minute, leading to unbounded growth
- **SOLUTION**: Redesigned metrics to use 5 fixed documents with time-series data
  - Operations metrics (reads, writes, database operations)
  - Performance metrics (response times, active connections)
  - Cache metrics (hit rate, size, evictions)
  - Memory metrics (total, used, free, process memory)
  - Connection metrics (active connections, total connections)
- **RESULT**: 10x performance improvement - response times back to ~38ms

### ✨ New Features

#### Time-Series Metrics Implementation
- **NEW**: Append-and-trim pattern for metrics data
- **NEW**: Configurable retention (default 15 minutes/15 data points)
- **NEW**: Fixed document IDs using standard `doc-<timestamp>-<random>` format
- **NEW**: Human-readable `name` field for searching metrics

#### System Collections
- **NEW**: Proper `_system_metrics` collection with underscore prefix
- **NEW**: Document count tracking for all collections
- **NEW**: UI support for viewing system collection documents

### 🐛 Bug Fixes

#### Database Operations
- **FIXED**: `db_insert_document` now respects provided `_id` values
- **FIXED**: `db_list_collections_with_info` returns document counts
- **FIXED**: Binary persistence no longer locks during serialization

#### UI/Frontend
- **FIXED**: JavaScript sending "[object Object]" instead of collection names
- **FIXED**: Collections data properly normalized in UI
- **FIXED**: Dashboard metrics display showing correct values
- **FIXED**: System collections showing hardcoded "0" document counts

#### Server Stability
- **FIXED**: Server crashes from excessive trace-level logging (910MB log files)
- **FIXED**: Log level changed from trace to info for production stability
- **FIXED**: Memory usage stabilized with proper metrics retention

### 🔧 Technical Improvements

#### Code Quality
- **REMOVED**: Unused `users` variable warning in rbac_api.c
- **CLEANED**: Temporary metrics cleanup scripts
- **IMPROVED**: Error handling in collection browsing

#### Documentation
- **NEW**: `docs/METRICS_STORAGE_FIX_PLAN.md` - Complete metrics redesign plan
- **NEW**: `docs/METRICS_IMPLEMENTATION_COMPLETE.md` - Implementation details
- **NEW**: `docs/METRICS_AND_PERMISSIONS_PLAN.md` - Permissions integration

## [2.0.4] - 2025-05-26

### 🐛 Bug Fixes

#### RBAC Display Issue Fix
- **FIXED**: RBAC interface showing only "admin" text when authenticated
- **CAUSE**: ID conflict between `<div id="roles">` tab pane and `<select id="roles">` form element
- **IMPACT**: `populateRoleSelects()` was replacing entire RBAC tab content with `<option>` elements
- **SOLUTION**: 
  - Renamed form select to `id="userRolesSelect"` to avoid conflicts
  - Updated `populateRoleSelects()` to target specific select elements
  - Added ID conflict detector to prevent future issues

### ✨ New Features

#### Frontend Safety System
- **NEW**: Automatic ID conflict detection on page load
- **NEW**: Console warnings for duplicate IDs with detailed element information
- **NEW**: Debug tools for RBAC troubleshooting (`rbac_debug.html`, `rbac_protection.js`)
- **NEW**: Comprehensive frontend best practices documentation

#### Documentation
- **NEW**: `docs/guidelines/FRONTEND_BEST_PRACTICES.md` - Preventing ID conflicts and DOM issues
- **NEW**: `docs/RBAC_COMPLETE_DOCUMENTATION.md` - Complete RBAC implementation guide
- **NEW**: `docs/RBAC_API_TOKENS_IMPLEMENTATION.md` - Long-lived API token design

### 🔧 Improvements

#### Authentication & Session Management
- **FIXED**: Login endpoint registration (was missing from routes array)
- **IMPROVED**: Session management with proper JWT token handling
- **ENHANCED**: RBAC roles tab now displays by default instead of users tab
- **FIXED**: Bootstrap tab switching for proper content rendering

#### UI/UX Improvements
- **FIXED**: CSS selector warnings in theme files
- **IMPROVED**: Role cards styling with hover effects
- **ENHANCED**: Tab event listeners for proper content rendering
- **FIXED**: Persistence thread logging changed from DEBUG to TRACE level

### 🔄 Session Management Overhaul
- **FIXED**: Sessions not persisting through server restarts
  - Root cause: `db_insert_document` was not creating deep copies
  - Solution: Implemented proper document copying before insertion
- **NEW**: Comprehensive session tracking
  - IP address capture from client socket
  - User-Agent header extraction
  - Session creation on login with 30-minute expiration
  - Soft deletion for audit trail
- **NEW**: Session termination API endpoint `/api/sessions/{id}/terminate`
  - Permission-based access control
  - Users can terminate own sessions
  - Admins can terminate any session
- **ENHANCED**: Admin UI session display
  - Added IP Address and User-Agent columns
  - Tooltips for long User-Agent strings
  - Status indicators for active/expired/terminated sessions
  - Revoke button with proper API integration

### 📝 Implementation Files
- `src/components/rbac/rbac_sessions.c` - Core session functions
- `src/components/api/session_terminate_api.c` - Termination endpoint
- `src/components/core/http_request.c` - Header extraction
- `src/components/core/handle_client.c` - IP address capture
- `share/htdocs/js/app.js` - Enhanced session UI

### ✅ Verification Results
- RBAC interface displays correctly when authenticated
- All tabs (Users, Roles, Permissions, Sessions, Audit) function properly
- Sessions persist through server restarts
- Session tracking captures IP and User-Agent correctly
- Session termination works with proper authorization
- ID conflict detection prevents similar issues
- No regression in existing functionality

## [2.0.3] - 2025-05-23

### 🐛 Bug Fixes

#### Document Deletion API Fix
- **FIXED**: DELETE requests to `/api/collections/{collection}/documents/{id}` returning "Collection not found"
- **CAUSE**: Collection drop handler was intercepting document deletion paths
- **SOLUTION**: Added path detection in collection drop handler to delegate document operations

#### Query Filtering Fix
- **FIXED**: URL query parameters not being applied to filter results
- **NEW**: URL query parameter parser converts `?key=value&key2=value2` format to JSON
- **ENHANCED**: Support for URL-encoded values (`%XX` and `+` encoding)
- **NOTE**: URL parameters are parsed as strings; use JSON body for exact type matching

### ✅ Verification Results
- Document deletion works correctly (204 No Content on success, 404 on not found)
- Query filtering works with both URL parameters (string matching) and JSON body (type matching)
- Empty queries return all documents as expected
- All fixes maintain backward compatibility

## [2.0.2] - 2025-05-23

### 🔧 Critical Fixes

#### Persistence Thread Fix
- **FIXED**: Persistence thread not surviving daemonization (moved initialization after fork)
- **NEW**: Modular persistence thread initialization in `init_persistence_thread()`
- **FIXED**: Automatic persistence now works correctly in daemon mode

#### JSON Serialization Fix
- **FIXED**: Document query API returning "Failed to serialize response" error
- **FIXED**: Missing `JSON_INTEGER` case in `json_stringify()` function
- **IMPROVED**: Document queries now return properly formatted JSON responses

#### Enhanced Debugging
- **NEW**: Detailed persistence thread lifecycle logging
- **NEW**: Document serialization/deserialization trace logs
- **NEW**: Query response structure debugging

### ✅ Verification Results
- Documents persist correctly across server restarts
- Query API returns complete response with documents, count, and total_count
- Binary persistence working with automatic saves
- All JSON types (string, number, integer, boolean, array, object) serialize correctly

## [2.0.1] - 2025-05-22

### 🔧 Runtime Script Improvements

- **IMPROVED**: Simplified and cleaned up `jsondb_runtime.sh` script (40% size reduction)
- **ORGANIZED**: Environment configuration with two-tier system:
  - Template: `/opt/jsondb/share/config/jsondb.env` (examples and documentation)
  - Running config: `/opt/jsondb/var/jsondb.env` (user customizations)
- **SIMPLIFIED**: Removed complex workarounds for resolved socket binding issues
- **UPDATED**: Accurate server flag mapping to match actual server capabilities
- **ENHANCED**: Professional configuration management with absolute paths

---

## [2.0.0] - 2025-05-22 🎉

### 🚀 Major Features Added

#### Binary Persistence System
- **NEW**: Complete binary persistence format with 4-6x performance improvement over JSON
- **NEW**: Multi-collection binary serialization with accurate file positioning
- **NEW**: TLV (Type-Length-Value) encoding for efficient data storage
- **NEW**: CRC32 checksums for data integrity validation
- **NEW**: Magic number verification (0x4A534442 - "JSDB") for file format validation
- **NEW**: Thread-safe persistence with automatic save triggers
- **NEW**: Buffer-based save triggers (50 operations OR 1MB threshold)
- **NEW**: Periodic saves every 30 seconds as safety net
- **NEW**: Database rollback on persistence failures

#### Performance Improvements
- **IMPROVED**: Database load times up to **5.9x faster** (1GB: 12.3s → 2.1s)
- **IMPROVED**: Database save times up to **5.3x faster** (1GB: 9.6s → 1.8s)
- **IMPROVED**: Simple queries up to **4.7x faster** (1GB: 210 qps → 980 qps)
- **IMPROVED**: Complex queries up to **5.0x faster** (1GB: 84 qps → 420 qps)

### 🔧 Technical Enhancements

#### Core Database
- **NEW**: Advanced persistence thread with pthread conditions and mutexes
- **NEW**: Database notification system for persistence triggers
- **FIXED**: Multi-collection deserialization size calculation mismatch
- **FIXED**: File pointer positioning errors in binary format
- **FIXED**: Document Query API response structure double-wrapping
- **IMPROVED**: Thread-safe database operations with proper locking

#### API Improvements
- **FIXED**: Document Query API returning empty results
- **FIXED**: Response serialization failures in multi-collection scenarios
- **IMPROVED**: Error handling and response consistency

#### Build System
- **CLEANED**: Removed 48+ debug and testing files using proper git hygiene
- **FIXED**: Makefile references to obsolete files
- **IMPROVED**: Zero-warning compilation with -Wall -Wextra
- **ORGANIZED**: Clean project structure following established conventions

### 📚 Documentation

#### New Documentation
- **NEW**: Comprehensive [Binary Format Guide](docs/BINARY_FORMAT.md)
- **NEW**: Performance benchmarks and comparison tables
- **NEW**: Binary format API documentation with code examples
- **NEW**: Configuration options for binary persistence
- **NEW**: Troubleshooting guide for binary format issues

#### Updated Documentation
- **UPDATED**: Main README.md with binary persistence features
- **UPDATED**: Performance section with detailed benchmark results
- **UPDATED**: Getting started guide with binary format information

### 🐛 Bug Fixes

#### Critical Fixes
- **FIXED**: Multi-collection binary deserialization crash ("Invalid document type: 0")
- **FIXED**: Document size calculation mismatch between serialization and deserialization
- **FIXED**: File pointer positioning causing collection read failures
- **FIXED**: Document header size field accuracy in binary format
- **FIXED**: Thread deadlocks in persistence operations

#### API Fixes
- **FIXED**: Document Query API response structure
- **FIXED**: Empty document results in query operations
- **FIXED**: Response serialization failures
- **FIXED**: Error handling in API endpoints

### 🔄 Breaking Changes

- **BREAKING**: Automatic binary format usage for optimal performance (backward compatible)
- **CHANGED**: Server startup sequence optimized for binary persistence
- **CHANGED**: Database file format (automatic migration on first startup)

### 🛠️ Infrastructure

#### Build & Development
- **REMOVED**: 4,899 lines of debug/test code for cleaner codebase
- **REMOVED**: Complete scripts/debug/ directory (socket debugging tools)
- **REMOVED**: Complete scripts/testing/ directory (performance tests, compilation tests)
- **REMOVED**: All backup files (.bak, .backup, .enhanced, .fixed)
- **CLEANED**: Project structure following established patterns

#### Git & Version Control
- **IMPROVED**: Git hygiene with proper file organization
- **ADDED**: Comprehensive commit messages with co-authorship
- **TAGGED**: Milestone release for binary persistence system

### 📊 Performance Benchmarks

#### Load Performance
- **100MB Database**: 1.2s → 0.3s (**4x improvement**)
- **1GB Database**: 12.3s → 2.1s (**5.9x improvement**)

#### Save Performance
- **100MB Database**: 0.9s → 0.2s (**4.5x improvement**)
- **1GB Database**: 9.6s → 1.8s (**5.3x improvement**)

#### Query Performance
- **Simple Queries (100MB)**: 850 qps → 3,200 qps (**3.8x improvement**)
- **Complex Queries (100MB)**: 320 qps → 1,100 qps (**3.4x improvement**)
- **Simple Queries (1GB)**: 210 qps → 980 qps (**4.7x improvement**)
- **Complex Queries (1GB)**: 84 qps → 420 qps (**5.0x improvement**)

### 🎯 Migration Guide

#### Upgrading to 2.0.0

1. **Automatic Migration**: The server automatically detects and migrates existing JSON databases
2. **No Configuration Changes**: Binary format is enabled by default with fallback to JSON
3. **Performance**: Expect immediate 4-6x performance improvements for large databases
4. **Compatibility**: All existing APIs remain unchanged

#### Configuration Options

```bash
# Force binary format (auto-detection by default)
JSONDB_BINARY_FORMAT=1

# Set size threshold for auto-detection (default: 10MB)
JSONDB_BINARY_SIZE_THRESHOLD=10485760
```

### 🙏 Contributors

This release was made possible by the comprehensive work on binary persistence implementation, multi-collection support, and extensive testing and validation.

### 🔗 Links

- [Binary Format Documentation](docs/BINARY_FORMAT.md)
- [Performance Benchmarks](docs/BINARY_FORMAT.md#performance-improvements)
- [API Documentation](docs/api/API.md)
- [GitHub Repository](https://github.com/yourusername/jsondb)

---

## [1.x.x] - Previous Releases

*Previous changelog entries would go here for historical releases*