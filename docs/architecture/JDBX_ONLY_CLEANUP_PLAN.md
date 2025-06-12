# JDBX-Only Database Cleanup Plan

## Overview
Transform JSONdb to use ONLY JDBX as the storage backend, eliminating all MMAP code and separate index files. Everything will be stored inside a single JDBX file.

## Current Problems
1. Dual storage backend support (MMAP + JDBX)
2. Separate .idx files for hash indexes
3. Separate .idx files for B-tree indexes  
4. Storage backend abstraction layer
5. Configuration complexity
6. Code duplication

## Target Architecture

### Single JDBX File Structure
```
jsondb.jdbx (ONE file for entire database)
├── Header Page
│   ├── Magic number
│   ├── Version
│   ├── Root directories
│   └── Global metadata
├── Collections Directory B-tree
│   └── collection_name -> metadata_page
├── Per Collection Pages:
│   ├── Collection Metadata Page
│   │   ├── Document B-tree root
│   │   ├── Hash index B-tree root
│   │   ├── Secondary B-tree index roots
│   │   └── Collection statistics
│   ├── Document Storage B-tree
│   │   └── doc_id -> JSON document
│   ├── Hash Index B-tree  
│   │   └── hash(doc_id) -> doc_page_offset
│   └── Secondary Index B-trees
│       └── field_value -> [doc_id, ...]
└── Overflow Pages (for large values)
```

## Files to Remove Completely

### Storage Backend Abstraction
- `src/components/storage/storage_backend.c`
- `src/components/storage/storage_backend.h` 
- `src/include/storage/storage_backend.h`

### MMAP Implementation
- `src/components/storage/mmap_storage.c`
- `src/include/storage/mmap_storage.h`

### Separate Index Implementations
- `src/components/index/hash_index.c` (create new jdbx_hash_index.c)
- `src/components/index/btree_disk.c` (create new jdbx_btree_index.c)
- `src/include/index/hash_index.h`
- `src/include/index/btree_disk.h`

## New Clean Implementation

### 1. Database Structure (database.h)
```c
typedef struct {
    char name[256];
    jdbx_database_t* jdbx;  // Single JDBX instance
    pthread_rwlock_t lock;
} database_t;

typedef struct {
    char name[256];
    uint64_t metadata_page;  // Page in JDBX
    jdbx_btree_t* doc_tree;
    jdbx_btree_t* hash_idx;
    jdbx_btree_t* secondary_idx[MAX_INDEXES];
    generic_cache_t* cache;
} collection_t;
```

### 2. Simplified Database Operations
```c
// No more storage backend checks!
int db_init(const char* path) {
    // Always create/open JDBX file
    g_database = jdbx_database_open(path);
    return g_database ? 1 : 0;
}

int db_create_collection(const char* name) {
    // Everything in JDBX
    return jdbx_create_collection(g_database->jdbx, name);
}
```

### 3. Integrated Hash Index
```c
// Hash index stored as B-tree in JDBX
typedef struct {
    uint64_t hash_idx_root;  // B-tree root page in JDBX
    // No separate files!
} hash_index_t;
```

### 4. Integrated B-tree Indexes
```c
// Secondary indexes also in JDBX
typedef struct {
    char field_name[128];
    uint64_t btree_root;  // B-tree root page in JDBX
} btree_index_t;
```

## Implementation Steps

### Phase 1: Create New JDBX-Only Files
1. `src/components/database/database_jdbx_only.c`
2. `src/components/index/jdbx_hash_index.c`
3. `src/components/index/jdbx_btree_index.c`

### Phase 2: Update Existing Files
1. Remove all `storage_backend_t` references
2. Remove all `STORAGE_BACKEND_MMAP` checks
3. Remove storage backend configuration options
4. Update Makefile to exclude removed files

### Phase 3: Clean Configuration
1. Remove `JSONDB_STORAGE_BACKEND` environment variable
2. Remove `--storage-backend` CLI option
3. Update documentation

### Phase 4: Testing
1. Ensure all operations work with JDBX only
2. Verify no .mmap or .idx files are created
3. Performance testing

## Benefits
1. **Simplicity**: One storage implementation
2. **Performance**: No abstraction overhead
3. **Atomicity**: All data in one file
4. **Consistency**: No sync issues between files
5. **Portability**: Single file to backup/move
6. **Less Code**: Remove ~2000 lines

## Configuration After Cleanup
```bash
# Only these remain:
JSONDB_DB_DIR=/opt/jsondb/var
JSONDB_JDBX_INITIAL_SIZE=104857600
JSONDB_JDBX_WAL_SIZE=10485760
# No more JSONDB_STORAGE_BACKEND!
```

## Summary
This cleanup will result in a streamlined database that:
- Uses ONLY JDBX format
- Stores everything in a single file
- Has no storage backend abstraction
- Creates no separate index files
- Is simpler to understand and maintain