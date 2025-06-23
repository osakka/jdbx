# JDBX Development Guidelines

**Version**: 7.2.1 - E2E Testing Fixes Complete  
**Updated**: June 23, 2025

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

## Recent Fixes (v7.2.1)

### E2E Testing Issues Resolved
1. **ART Delete Implementation**: `skiplist_delete()` now functional in art.c
2. **RBAC User Deletion**: Fixed path parameter extraction in rbac_api.c  
3. **Status Endpoint**: Added `/api/status` handler in health_api.c
4. **JSON Metrics**: Support `?format=json` parameter for JSON output

All fixes maintain single source of truth with no parallel implementations.

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

## Documentation

- Full version history: `CLAUDE_FULL.md`
- Detailed guides: `docs/guidelines/`
- Architecture decisions: `docs/architecture/`

## Reference Links

- **Omnicore**: https://github.com/osakka/omnicore (general patterns)
- **Local Guidelines**: `docs/guidelines/` (project-specific)
- **Git Workflow**: Standard practices, clean commits