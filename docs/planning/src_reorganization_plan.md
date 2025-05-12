# Source Code Reorganization Plan

## New Directory Structure

```
src/
├── api/                      # API-related code
│   ├── admin_api.c
│   ├── backup_api.c
│   ├── cache_api.c
│   ├── index_api.c
│   ├── index_query_api.c
│   ├── schema_api.c
│   ├── system_api.c
│   ├── transaction_api.c
│   ├── transaction_log_api.c
│   └── visualization_api.c
├── database/                 # Core database functionality
│   ├── database.c
│   ├── index.c
│   ├── lock_manager.c
│   └── schema.c
├── js/                       # JavaScript engine integration
│   ├── js_api.c              # JavaScript API functions
│   ├── js_db.c               # Database operations exposed to JS
│   └── js_engine.c           # JavaScript engine core
├── transaction/              # Transaction system
│   ├── transaction.c
│   ├── transaction_log.c
│   ├── transaction_retry.c
│   └── transaction_visualization.c
└── utils/                    # Utility modules
    ├── cache.c
    ├── cors.c
    ├── import_export.c
    ├── json.c
    ├── jwt.c
    ├── memory/               # Memory management utilities
    │   ├── mem_pool.c        # Memory pool for more efficient allocation
    │   ├── mem_tracker.c     # Memory usage tracking
    │   └── ref_counter.c     # Reference counting for shared resources
    ├── metrics.c
    ├── server.c
    └── ssl.c

include/
├── api/
│   ├── admin_api.h
│   ├── backup_api.h
│   ├── cache_api.h
│   └── ...
├── database/
│   ├── database.h
│   ├── index.h
│   └── ...
├── js/
│   ├── js_api.h
│   ├── js_db.h
│   └── js_engine.h
├── transaction/
│   ├── transaction.h
│   └── ...
└── utils/
    ├── memory/
    │   ├── mem_pool.h
    │   ├── mem_tracker.h
    │   └── ref_counter.h
    └── ...
```

## File Reorganization Steps

1. Create new directory structure
2. Move files to appropriate directories
3. Update include paths in all files
4. Update Makefile to reflect new structure
5. Test compilation to ensure everything works
6. Commit changes

## Memory Management Improvements

As part of this reorganization, we will implement:

1. **Memory Pool**: A more efficient allocator for frequently used objects
2. **Memory Tracker**: Debug tooling to track allocations and detect leaks
3. **Reference Counting**: A system for tracking shared objects to prevent double-free issues

## Specific Memory Issues to Address

The main memory issue to solve is the double-free error occurring during cleanup, particularly:

1. **Root Cause**: Shared JSON objects between the RBAC system and JavaScript engine
2. **Solution**: Implement reference counting for these objects so they're only freed when no longer referenced
3. **Affected Files**: 
   - src/rbac.c
   - src/js_engine.c
   - src/json.c
   - src/main.c

## Implementation Schedule

1. First implement the memory management improvements
2. Then reorganize the directory structure
3. Finally update documentation to reflect changes