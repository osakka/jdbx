# JDBX Development Guidelines

**Version**: 7.2.5 - Unified Logging Architecture  
**Updated**: June 24, 2025

## Project Overview

JDBX is a revolutionary database engine with:
- **ART Engine**: Adaptive Radix Tree data structures (O(k) operations)
- **Unified Documents**: Everything stored as documents with type-based discrimination  
- **Checkpoint Memory**: Automatic memory management with promotion system
- **Modular APIs**: Clean separation across focused modules

## Essential Architecture

### Single Source of Truth
- **One Build**: `cd src && make` → `bin/jdbxd`
- **One Storage**: All entities in unified documents collection
- **One Memory System**: Checkpoint-based with automatic cleanup
- **No Duplicates**: Zero parallel implementations allowed

### File Structure
```
src/
├── components/     # Core implementation
├── include/        # Headers
└── Makefile       # Single build system

Key files:
- main.c: Entry point
- art.c: ART engine (replaces skiplist)
- memory_manager.c: Checkpoint system
- api_*.c: Modular API handlers
```

### Memory Management (CRITICAL)
```c
// Checkpoint pattern for transactions
memory_checkpoint_t* cp = memory_checkpoint_create();
// ... allocate and work ...
memory_checkpoint_rewind(cp);  // Auto-cleanup on error
memory_checkpoint_commit(cp);  // Make permanent

// Promote long-lived objects
memory_promote(persistent_data);
```

**NEVER use json_free()** - All JSON managed by checkpoints

## Recent Fixes (v7.2.5)

### 🔧 Unified Logging Architecture (v7.2.5)
**COMPREHENSIVE LOGGING STANDARDIZATION**: Implemented unified logging with early-stage support, eliminating redundant prefixes and ensuring consistent formatting across all initialization phases.

**Technical Achievements:**
1. **Early-Stage Logging**: Created `logger_early_log()` for consistent logging before main logger initialization
2. **Redundant Prefix Elimination**: Removed [DAEMON], [FILE_SERVING], [INIT:*] prefixes - file/function info already in log format
3. **Unified Macros**: DAEMON_LOG and INIT_LOG_* now seamlessly transition between early and regular logging
4. **Thread-Safe Implementation**: Both regular and early-stage logging are fully thread-safe

**Logging Format:**
```
timestamp [pid:tid] [level] function.filename line: message
```

**Key Improvements:**
- ✅ Consistent format across all initialization phases
- ✅ Seamless daemonization transitions (stderr → logger)
- ✅ Cleaner, more focused log messages
- ✅ Per-functionality trace control (TRACE_DB, TRACE_RBAC, etc.)

## Recent Fixes (v7.2.4)

### 🔧 API Server-Side Filtering Fix Complete
**CRITICAL API BUG ELIMINATED**: Fixed server-side document filtering by implementing proper URL decoding in query parameter parsing, transforming from 100% query bypass to precise server-side filtering.

**Technical Achievement:**
1. **URL Decoding Implementation**: Added comprehensive hex decoding (%XX) and plus-to-space conversion for query parameter values in `parse_url_query_to_json()`
2. **Query Bypass Eliminated**: URL-encoded JSON queries were stored as raw strings, causing complete filter bypass
3. **Server-Side Filtering Restored**: Queries like `{"type":"user"}` now return 7 documents (was returning all 15+ documents)
4. **Single Source of Truth Maintained**: Eliminates need for client-side filtering workarounds

**Validation Results:**
- ✅ Query `{"nonexistent_field":"value"}` returns 0 documents (was returning all)
- ✅ Query `{"type":"user","library":"system"}` returns 7 user documents (was returning all 15+)
- ✅ UI collection display now shows correct document counts filtered by type
- ✅ All `/api/documents?query=...` requests now filter server-side as intended

### UI Collection Display Fixed (v7.2.3)
1. **Collection Name Mapping**: Added plural to singular mapping in app.js
   - Collections show plural names (users, roles, sessions)
   - Documents have singular types (user, role, session)
   - UI now correctly maps when querying documents

### UI Library Switching Fixed (v7.2.2)
1. **Login Endpoint**: Fixed login.html to use `/api/auth/login` (not `/api/login`)
2. **Single Source of Truth**: Removed duplicate `api_handle_switch_library` implementations
   - Kept authoritative version in api_auth.c
   - Removed duplicates from auth_session_api.c
3. **URL Parameter Extraction**: Library switch handler now extracts from URL path
   - UI sends: `/api/auth/library/{name}`
   - Handler extracts library name from path
4. **Access Control**: Simplified library access permissions
   - Admin users have access to system library
   - All users have access to default library

### Previous Fixes (v7.2.1)
1. **ART Delete Implementation**: `skiplist_delete()` now functional in art.c
2. **RBAC User Deletion**: Fixed path parameter extraction in rbac_api.c  
3. **Status Endpoint**: Added `/api/status` handler in health_api.c
4. **JSON Metrics**: Support `?format=json` parameter for JSON output

## Build & Run

```bash
# Build
cd /opt/jdbx/src && make

# Run (daemon mode ONLY)
cd /opt/jdbx && build/jdbx_runtime.sh start

# Check status/logs
build/jdbx_runtime.sh status
cat var/jdbxd.log
```

## Core Principles

1. **Single Source of Truth** - No duplicate implementations
2. **Zero Warnings** - Always compile with `-Wall -Wextra`
3. **Memory Promotion** - Promote objects that survive checkpoints
4. **Security First** - Use OpenSSL, not custom crypto
5. **Document Everything** - All fixes thoroughly documented
6. **Clean Workspace** - Move temp files to trash/
7. **Impeccable Git** - Clean commits, proper hygiene
8. **Unified Logging** - Consistent format, no redundant prefixes

## Critical Memory Rules

- **Checkpoint Promotion**: Objects crossing boundaries need `memory_promote()`
- **SSL Memory**: Promote SSL contexts/connections for OpenSSL references
- **HTTP Responses**: Promote ALL responses before checkpoint operations
- **JWT Cache**: Promote duplicated JWT payloads and claims
- **JSON Objects**: Use `json_promote()` for checkpoint-managed JSON

## API Architecture

```c
// Storage operations (direct unified collection)
storage_insert_document(db, doc);
storage_query_documents(db, query);

// Virtual operations (logical entities)
virtual_create_user(db, username, password, library);
virtual_query_users(db, library, filters);

// FORBIDDEN: Legacy hierarchical functions
// db_insert_document(db, library, collection, doc);  ❌
```

## Security Requirements

- **Environment Config**: No hardcoded credentials
- **PBKDF2 Passwords**: Never plain text or custom hashing
- **Three-Tier Config**: env → CLI → database priority
- **Rate Limiting**: Use JDBX database for state storage
- **SSL/TLS**: Production-ready defaults

## Logging Standards

### Format
```
timestamp [pid:tid] [level] function.filename line: message
```

### Levels
- **ERROR**: Critical errors preventing operation
- **WARNING**: Important issues needing attention  
- **INFO**: Key operational events (production default)
- **DEBUG**: Detailed troubleshooting information
- **TRACE**: Per-functionality execution flow

### Early-Stage Logging
Use `EARLY_LOG_*` macros before logger initialization:
```c
EARLY_LOG_INFO("COMPONENT", "Message with %s", "parameters");
```

### Trace Categories
- `TRACE_DB(...)` - Database operations
- `TRACE_RBAC(...)` - Role-based access control
- `TRACE_API(...)` - API request handling
- `TRACE_AUTH(...)` - Authentication flows
- `TRACE_NET(...)` - Network operations

## Documentation

- Full version history: `CLAUDE_FULL.md`
- Detailed guides: `docs/guidelines/`
- Architecture decisions: `docs/architecture/`

## Reference Links

- **Omnicore**: https://github.com/osakka/omnicore (general patterns)
- **Local Guidelines**: `docs/guidelines/` (project-specific)
- **Git Workflow**: Standard practices, clean commits