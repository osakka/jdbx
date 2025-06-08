# High-Performance Components Integration Plan

## Current State
- ✅ Database uses mmap_storage for document storage
- ✅ Hash index used for primary key (_id) lookups
- ❌ B+tree not integrated for secondary indexes
- ❌ No query optimization using indexes

## Integration Steps

### Phase 1: Add B+Tree Secondary Indexes

#### 1.1 Extend Collection Structure
```c
typedef struct {
    char name[256];
    mmap_storage_t* storage;
    hash_index_t* primary_index;
    
    // Add secondary indexes
    struct {
        char field_name[128];
        btree_disk_t* btree;
    } secondary_indexes[MAX_INDEXES];
    int num_indexes;
    
    generic_cache_t* cache;
    pthread_rwlock_t lock;
    atomic_uint_fast64_t doc_count;
    atomic_uint_fast64_t total_size;
} hp_collection_t;
```

#### 1.2 Create Index Management API
```c
// Create secondary index on field
int db_create_index(database_t* db, const char* collection, 
                   const char* field, index_type_t type);

// Drop index
int db_drop_index(database_t* db, const char* collection, 
                 const char* field);

// List indexes
json_value_t* db_list_indexes(database_t* db, const char* collection);
```

#### 1.3 Update Insert to Maintain Indexes
- Extract field values from documents
- Insert into appropriate B+tree indexes
- Handle multi-value fields (arrays)

#### 1.4 Update Delete to Maintain Indexes
- Remove entries from all indexes
- Handle cascading deletes

### Phase 2: Query Optimization

#### 2.1 Query Planner
```c
typedef struct query_plan {
    enum {
        PLAN_FULL_SCAN,
        PLAN_INDEX_SCAN,
        PLAN_INDEX_RANGE_SCAN,
        PLAN_MULTI_INDEX
    } type;
    
    char* index_name;
    void* start_key;
    void* end_key;
    double estimated_cost;
} query_plan_t;

query_plan_t* create_query_plan(json_value_t* query, 
                               hp_collection_t* collection);
```

#### 2.2 Index-Aware Query Execution
- Analyze query predicates
- Choose optimal index
- Execute using index scan instead of full scan
- Handle compound queries with multiple indexes

### Phase 3: Performance Testing

#### 3.1 Benchmark Suite
- Insert 1M documents
- Create indexes on common fields
- Compare query performance:
  - Full scan vs index scan
  - Range queries
  - Compound queries

#### 3.2 Expected Results
- Point queries: <100μs (from ~100ms)
- Range queries: <1ms (from ~500ms)
- Insert with indexes: <500μs

## Implementation Priority

1. **Start with single-field B+tree indexes** (1-2 days)
   - Integrate btree_disk into collection
   - Add create_index API
   - Update insert/delete

2. **Add query planner** (1-2 days)
   - Analyze queries for index usage
   - Cost estimation
   - Plan selection

3. **Optimize query execution** (1-2 days)
   - Use index scans
   - Handle range queries
   - Compound index support

## Code Changes Required

### 1. database.h
- Add index management functions
- Extend collection structure

### 2. database.c
- Implement index creation/deletion
- Update insert/delete for index maintenance
- Add index-aware query execution

### 3. New file: query_optimizer.c
- Query analysis
- Cost estimation
- Plan generation

## Success Metrics

1. **1M documents**: <10s insert time with indexes
2. **Point queries**: <100μs with index
3. **Range queries**: <1ms for 1000 results
4. **Memory usage**: <100MB for 1M docs + indexes
5. **Startup time**: <1s with preloaded indexes

## Next Immediate Step

Start by adding B+tree secondary index support to the collection structure and implementing the create_index API.