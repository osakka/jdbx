# Surgical Legacy Code Removal Plan

**Date**: June 12, 2025  
**Objective**: Remove ALL legacy storage code to achieve pure JDBX-only implementation

## Phase 1: Identify Legacy Files to Remove

### Storage Backend Files (REMOVE)
- `/opt/jsondb/src/components/storage/mmap_storage.c` - MMAP implementation
- `/opt/jsondb/src/components/storage/storage_backend.c` - Storage abstraction layer
- `/opt/jsondb/src/include/storage/mmap_storage.h` - MMAP header
- `/opt/jsondb/src/include/storage/storage_backend.h` - Storage abstraction header

### Old Index Files (REMOVE)
- `/opt/jsondb/src/components/index/hash_index.c` - Creates .idx files
- `/opt/jsondb/src/components/index/btree_disk.c` - Creates .idx files  
- `/opt/jsondb/src/include/index/hash_index.h` - Hash index header
- `/opt/jsondb/src/include/index/btree_disk.h` - Btree disk header

### Old Database Files (REMOVE)
- `/opt/jsondb/src/components/database/database.c` - Old database with storage backends
- `/opt/jsondb/src/components/database/database_unified.c` - Unified documents with storage
- `/opt/jsondb/src/components/database/persistence.c` - Old binary persistence
- `/opt/jsondb/src/components/binary/` - Entire binary format directory (replaced by JDBX)

## Phase 2: Update Dependencies

### Files That Need Updates
1. **Makefile** - Remove all references to deleted files
2. **API files** - Update to use JDBX-only database functions
3. **Initialize files** - Ensure only database_jdbx.c is used
4. **Test files** - Update or remove tests for deleted components

## Phase 3: Function Mapping

### Replace These Calls
- `hash_index_create()` → `jdbx_btree_create()` with hash prefix
- `btree_disk_create()` → `jdbx_btree_create()`
- `storage_backend_create()` → Direct JDBX operations
- `db_init()` from database.c → `db_init()` from database_jdbx_only.c

## Phase 4: Verification

### Success Criteria
1. NO .idx files created
2. NO .mmap files created  
3. ONLY .jdbx file exists
4. All CRUD operations work
5. Clean compilation with zero warnings

## Execution Order

1. First, update Makefile to exclude legacy files
2. Remove legacy files one by one
3. Fix compilation errors by updating references
4. Test and verify single-file operation