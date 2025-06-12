# MMAP to JDBX Cleanup Analysis

## Current State (as of June 11, 2025)

The codebase still maintains dual storage backend support (MMAP and JDBX) with the following components:

### 1. Storage Backend Abstraction Layer
- **File**: `src/components/storage/storage_backend.c/h`
- **Purpose**: Provides abstraction for switching between MMAP and JDBX storage
- **Key enum**: `storage_backend_type_t` with values `STORAGE_BACKEND_MMAP` and `STORAGE_BACKEND_JDBX`

### 2. MMAP Storage Implementation
- **File**: `src/components/storage/mmap_storage.c`
- **Header**: `src/include/storage/mmap_storage.h`
- **Purpose**: Memory-mapped file storage for individual collections
- **Creates**: `.mmap` files per collection

### 3. Configuration Support
- **Environment variable**: `JSONDB_STORAGE_BACKEND` (defaults to "mmap")
- **Config defaults**: `DEFAULT_STORAGE_BACKEND` set to "mmap" in `config_defaults.h`
- **Runtime detection**: Code checks storage backend type and creates appropriate storage

### 4. Database Implementation
- **File**: `src/components/database/database.c`
- **Dual logic**: 
  - Lines 513-593: Conditional logic for MMAP vs JDBX storage creation
  - Lines 276-352: Loading logic that looks for `.mmap` files
  - Lines 595-598: Creates `.idx` index files regardless of storage backend

### 5. Index Files
- **Hash Index**: `src/components/index/hash_index.c` - creates `.idx` files
- **BTree Disk**: `src/components/index/btree_disk.c` - separate index implementation
- **Issue**: Index files are created separately from JDBX storage

## Files to Remove for JDBX-Only Implementation

### 1. Storage Files to Remove:
- `src/components/storage/mmap_storage.c`
- `src/components/storage/storage_backend.c`
- `src/include/storage/mmap_storage.h`
- `src/include/storage/storage_backend.h`
- `src/include/storage/indexed_mmap_storage.h`
- `src/include/storage/lsm_tree.h` (if unused)

### 2. Index Files to Modify/Remove:
- `src/components/index/hash_index.c` - Remove or integrate into JDBX
- `src/components/index/btree_disk.c` - Remove or integrate into JDBX
- Associated headers in `src/include/index/`

## Code Changes Required

### 1. Database.c Modifications:
- Remove storage backend type checks
- Remove MMAP-specific collection loading code
- Remove separate index file creation
- Simplify to only use JDBX storage
- Remove `g_database.jdbx_storage` global and make it per-database

### 2. Configuration Changes:
- Remove `JSONDB_STORAGE_BACKEND` environment variable
- Remove `storage_backend` field from server config
- Remove `DEFAULT_STORAGE_BACKEND` from config defaults
- Remove MMAP-related size configurations

### 3. Collection Creation Simplification:
- Remove lines 513-593 conditional logic
- Always create JDBX storage
- Store indices within JDBX, not separate files
- Use JDBX B-tree for all indexing needs

### 4. Collection Loading Simplification:
- Remove code that looks for `.mmap` files
- Remove code that looks for `.idx` files
- Load all data from single JDBX database file

## Benefits of JDBX-Only Implementation

1. **Single File Storage**: All data in one `database.jdbx` file
2. **No File Proliferation**: No more `.mmap` and `.idx` files per collection
3. **Simpler Code**: Remove abstraction layer and conditional logic
4. **Better Performance**: Single B-tree for all operations
5. **Easier Backup**: Just backup one file
6. **Atomic Operations**: WAL provides transaction safety

## Migration Path

1. **Phase 1**: Remove storage backend abstraction
2. **Phase 2**: Integrate indexing into JDBX B-tree
3. **Phase 3**: Remove MMAP loading code
4. **Phase 4**: Clean up configuration
5. **Phase 5**: Update documentation

## Risks and Considerations

1. **Backward Compatibility**: Existing MMAP databases won't be readable
2. **Migration Tool**: Need a tool to convert MMAP databases to JDBX
3. **Testing**: Comprehensive testing needed for all operations
4. **Documentation**: Update all references to dual storage

## Recommendation

Proceed with complete removal of MMAP support to achieve a cleaner, simpler codebase with JDBX as the sole storage backend. This aligns with the "one source of truth" principle in CLAUDE.md.