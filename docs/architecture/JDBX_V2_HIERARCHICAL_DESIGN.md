# JDBX v2 Hierarchical Storage Design

## Overview

JDBX v2 implements proper hierarchical storage with intrinsic support for the library → collection → document structure. This is a clean break from v1 with no backward compatibility.

## Design Principles

1. **Structural Hierarchy**: Libraries contain collections, collections contain documents
2. **Isolated Namespaces**: Each level has its own B-tree for isolation
3. **Efficient Navigation**: Direct path from library → collection → document
4. **No Mixed Data**: Different data types in different B-trees

## JDBX v2 File Format

```
JDBX File Structure:
┌─────────────────────────────────┐
│ File Header                     │
│ - Magic: "JDB2" (v2 marker)     │
│ - Version: 2                    │
│ - Root directory page           │
│ - Page size: 4096               │
│ - Checksum                      │
├─────────────────────────────────┤
│ Root Directory B-tree           │
│ (Libraries listing)             │
│ Key: library_name               │
│ Value: library_btree_page       │
├─────────────────────────────────┤
│ Library B-trees                 │
│ (Collections per library)       │
│ Key: collection_name            │
│ Value: collection_btree_page    │
├─────────────────────────────────┤
│ Collection B-trees              │
│ (Documents per collection)      │
│ Key: document_id                │
│ Value: document_json            │
└─────────────────────────────────┘
```

## Data Structures

### 1. JDBX Database Structure
```c
typedef struct jdbx_database_v2 {
    jdbx_page_manager_t* pm;
    jdbx_btree_t* root_dir;        // Libraries directory
    
    // Cached library handles
    struct {
        char name[64];
        jdbx_btree_t* collections_dir;
        pthread_rwlock_t lock;
    } libraries[MAX_LIBRARIES];
    int num_libraries;
    
    pthread_rwlock_t global_lock;
} jdbx_database_v2_t;
```

### 2. Library Structure
```c
typedef struct jdbx_library {
    char name[64];
    jdbx_btree_t* collections_dir;  // Collections in this library
    json_value_t* metadata;         // Library settings, quotas, etc.
} jdbx_library_t;
```

### 3. Collection Structure  
```c
typedef struct jdbx_collection {
    char name[64];
    char library[64];
    jdbx_btree_t* documents;        // Documents in this collection
    jdbx_btree_t* indexes[MAX_INDEXES];
    json_value_t* schema;
    json_value_t* metadata;
} jdbx_collection_t;
```

## API Changes

### Path Resolution
All APIs use proper library/collection paths:
```c
// Old: Flat collection name
db_insert(db, "documents", doc);

// New: Hierarchical path
db_insert(db, "library1/products", doc);
db_insert(db, "system/users", doc);
```

### Library Operations
```c
// Create library
int jdbx_create_library(db, "library1");

// List libraries  
json_value_t* jdbx_list_libraries(db);

// Get library metadata
json_value_t* jdbx_get_library(db, "library1");
```

### Collection Operations
```c
// Create collection in library
int jdbx_create_collection(db, "library1", "products");

// List collections in library
json_value_t* jdbx_list_collections(db, "library1");

// Full path also supported
int jdbx_create_collection_path(db, "library1/products");
```

## Implementation Steps

### Phase 1: Core JDBX v2 Implementation
1. Update page manager for v2 header
2. Implement three-level B-tree structure
3. Add library and collection management
4. Update path resolution logic

### Phase 2: Database Layer Integration
1. Update database_jdbx_only.c to use v2 APIs
2. Fix path handling in all db_* functions
3. Remove unified documents logic
4. Update initialization to create system library

### Phase 3: API Layer Fixes
1. Update API handlers to use library/collection paths
2. Fix authentication to use system/users
3. Fix RBAC to use system/roles
4. Update metrics to use system/metrics

### Phase 4: Testing and Verification
1. Test all CRUD operations
2. Verify authentication works
3. Test library/collection creation
4. Benchmark performance

## Benefits

1. **Performance**: Each collection has its own B-tree, reducing traversal time
2. **Scalability**: Libraries and collections can be tuned independently  
3. **Clarity**: Clear separation of concerns
4. **Operations**: Can backup/restore specific libraries
5. **Caching**: Better cache locality per collection

## Migration

Since we're doing a clean break:
1. Bump JDBX version to 2
2. Refuse to open v1 files
3. Document provides migration script if needed
4. Start fresh with proper structure

## Example Usage

```c
// System initialization creates system library
jdbx_create_library(db, "system");
jdbx_create_collection(db, "system", "users");
jdbx_create_collection(db, "system", "roles");
jdbx_create_collection(db, "system", "config");

// User creates application library
jdbx_create_library(db, "myapp");
jdbx_create_collection(db, "myapp", "products");
jdbx_create_collection(db, "myapp", "orders");

// Insert documents with full paths
json_value_t* product = json_parse("{\"name\": \"Widget\", \"price\": 9.99}");
jdbx_insert(db, "myapp/products", product);

// Query with full paths
json_value_t* query = json_parse("{\"price\": {\"$gt\": 5.0}}");
json_value_t* results = jdbx_find(db, "myapp/products", query);
```

This clean architectural break will make JSONdb more scalable, maintainable, and performant.