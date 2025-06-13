# Database.c Replacement Plan

## Current Database Structure Analysis

### Overview
The current database implementation is spread across multiple files in `/opt/jdbx/src/components/database/`:

1. **db.c** (1219 lines) - Main database implementation with binary format support
2. **database_with_optimizations.c** (838 lines) - Alternative implementation with optimizations
3. **operations.c** (618 lines) - Simplified database operations
4. **collection_ops.c** (82 lines) - Collection management operations
5. **collection_v2.c** (219 lines) - Enhanced collection operations
6. **persistence.c** (338 lines) - Persistence thread management
7. **index.c** (1225 lines) - Index management
8. **indexed_document_operations.c** (907 lines) - Indexed document operations
9. **schema.c** (1188 lines) - Schema validation
10. **json_schema_manager.c** (273 lines) - JSON schema management
11. **json_schema_validator.c** (331 lines) - JSON schema validation
12. **system_schemas.c** (312 lines) - System collection schemas
13. **js_integration.c** (335 lines) - JavaScript integration
14. **lock_manager.c** (1365 lines) - Lock management for concurrency

### Current Makefile Configuration
The Makefile currently excludes:
- `database.c` 
- `database_operations.c`
- `database_with_optimizations.c`

And uses only `db.c` as the main database implementation.

### Key Database Components

#### Core Database Structure (from database.h):
```c
typedef struct database {
    char* path;                    /* Path to database file */
    json_value_t* collections;     /* JSON object of collections */
    pthread_mutex_t lock;          /* Database lock for thread safety */
    int is_modified;               /* Flag to track if database is modified */
    cache_t* cache;                /* Document cache */
    int cache_enabled;             /* Flag indicating if caching is enabled */
    transaction_manager_t* transaction_manager; /* Transaction manager */
    persistence_thread_t* persistence; /* Persistence thread management */
    int is_bootstrap_mode;         /* Flag for bootstrap initialization mode */
} database_t;
```

#### Core Database Functions (from db.c):
1. **Initialization/Lifecycle**:
   - `db_init()` - Initialize database
   - `db_close()` - Close database
   - `db_save()` - Save to binary format
   - `db_load()` - Load from binary format

2. **Collection Operations**:
   - `db_create_collection()`
   - `db_drop_collection()`
   - `db_get_collection()`
   - `db_list_collections()`
   - `db_list_collections_with_info()`

3. **Document Operations**:
   - `db_insert_document()`
   - `db_get_document()`
   - `db_update_document()`
   - `db_delete_document()`
   - `db_query_documents()`

4. **Cache Management**:
   - `db_enable_cache()`
   - `db_disable_cache()`
   - `db_configure_cache()`
   - `db_get_cache_stats()`
   - `db_clear_cache()`

5. **Index Operations**:
   - `db_rebuild_indices()`
   - `process_cache_invalidations()`

### Key Features to Preserve:
1. **Binary Format Persistence** - Database is saved in .jdb binary format
2. **Persistence Thread** - Automatic saves triggered by thresholds
3. **Thread Safety** - Mutex-based locking for concurrent access
4. **Cache Integration** - Optional caching layer
5. **Transaction Support** - Transaction manager integration
6. **Index Support** - Document indexing capabilities
7. **Schema Validation** - JSON schema validation
8. **JavaScript Integration** - QuickJS integration for validators/transformers

## Replacement Strategy

### Goals:
1. Create a single, unified database.c that consolidates functionality
2. Eliminate duplicate implementations
3. Maintain all existing functionality
4. Improve code organization and readability
5. Ensure zero regression in features

### Approach:
1. Merge core database operations from `db.c` into new `database.c`
2. Integrate collection operations from `collection_ops.c` and `collection_v2.c`
3. Include simplified operations from `operations.c` where appropriate
4. Maintain separate files for:
   - Persistence thread management (persistence.c)
   - Index management (index.c)
   - Schema validation (schema.c, json_schema_*.c)
   - JavaScript integration (js_integration.c)
   - Lock management (lock_manager.c)

### File Structure Plan:
- **database.c** - Core database operations, collection management, document CRUD
- Keep existing:
  - persistence.c
  - index.c
  - indexed_document_operations.c
  - schema.c
  - json_schema_manager.c
  - json_schema_validator.c
  - system_schemas.c
  - js_integration.c
  - lock_manager.c

### Implementation Steps:
1. Create new `database.c` based on `db.c`
2. Integrate collection operations
3. Merge simplified operations where they improve clarity
4. Update Makefile to use new `database.c`
5. Remove old implementations: `db.c`, `database_with_optimizations.c`, `collection_ops.c`, `collection_v2.c`, `operations.c`
6. Test thoroughly to ensure no regression