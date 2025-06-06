# JSONdb Performance Optimization Plan

## Overview
This document outlines a phased approach to implement three major performance optimizations that will dramatically improve JSONdb's throughput and latency.

## Current Performance Bottlenecks

1. **Global Mutex Lock**: Every database operation acquires an exclusive lock
2. **No Connection Reuse**: Each HTTP request creates a new SSL connection
3. **No Indexes**: All queries perform full collection scans

## Phase 1: Read-Write Lock Implementation

### Goal
Replace the global mutex with a read-write lock to allow concurrent read operations.

### Implementation Steps

1. **Update Database Structure** (`src/include/database/database.h`)
   ```c
   typedef struct database_t {
       pthread_rwlock_t rwlock;  // Replace pthread_mutex_t lock
       // ... rest of structure
   } database_t;
   ```

2. **Initialize RWLock** (`src/components/database/database.c`)
   - Replace `pthread_mutex_init` with `pthread_rwlock_init`
   - Add proper cleanup with `pthread_rwlock_destroy`

3. **Update All Read Operations**
   - `db_query_documents()` 
   - `db_get_document()`
   - `db_list_collections()`
   - Use `pthread_rwlock_rdlock()` and `pthread_rwlock_unlock()`

4. **Update All Write Operations**
   - `db_insert_document()`
   - `db_update_document()`
   - `db_delete_document()`
   - Use `pthread_rwlock_wrlock()` and `pthread_rwlock_unlock()`

### Testing
- Concurrent read performance test
- Mixed read/write workload test
- Deadlock detection test

### Expected Impact
- 3-5x improvement in read throughput
- No impact on write performance
- Significantly reduced read latency

## Phase 2: HTTP Keep-Alive Implementation

### Goal
Implement HTTP/1.1 keep-alive to reuse connections and reduce SSL overhead.

### Implementation Steps

1. **Connection State Management** (`src/components/core/client_connection.h`)
   ```c
   typedef struct {
       int socket_fd;
       SSL *ssl;
       int keep_alive;
       time_t last_activity;
       int request_count;
   } client_connection_t;
   ```

2. **Update HTTP Response Headers** (`src/components/core/http_response.c`)
   - Add `Connection: keep-alive` header
   - Add `Keep-Alive: timeout=5, max=100` header

3. **Modify Client Handler** (`src/components/core/handle_client.c`)
   - Loop to handle multiple requests per connection
   - Implement timeout handling
   - Track request count per connection

4. **Update Thread Pool** (`src/components/core/thread_pool.c`)
   - Keep connections alive between requests
   - Implement connection timeout cleanup

### Testing
- Connection reuse verification
- Timeout handling test
- Load test with keep-alive enabled

### Expected Impact
- 30-50% reduction in SSL handshake overhead
- Lower latency for sequential requests
- Reduced server CPU usage

## Phase 3: Index Implementation

### Goal
Add B-tree indexes for fast document lookups and range queries.

### Current State
- B-tree implementation exists in `src/components/index/btree_disk.c`
- Hash index exists in `src/components/index/hash_index.c`
- Not integrated with query execution

### Implementation Steps

1. **Index Management API** (`src/include/database/index.h`)
   ```c
   typedef struct {
       btree_t *btree;
       char *field_name;
       char *collection_name;
   } index_t;
   ```

2. **Create Index Command** (`src/components/api/index_api.c`)
   - `POST /api/collections/{name}/indexes`
   - Index on specific fields
   - Automatic index updates on insert/update

3. **Query Optimizer** (`src/components/query/query_optimizer.c`)
   - Detect if query can use index
   - Choose between index scan vs full scan
   - Support for compound indexes

4. **Index Integration**
   - Update `db_insert_document()` to update indexes
   - Update `db_update_document()` to maintain indexes
   - Update `db_query_documents()` to use indexes

### Testing
- Index creation performance
- Query performance with/without indexes
- Index maintenance overhead

### Expected Impact
- O(log n) lookups instead of O(n)
- 10-100x faster queries on indexed fields
- Efficient range queries

## Implementation Timeline

### Week 1: Read-Write Locks
- Days 1-2: Implement rwlock changes
- Days 3-4: Update all database operations
- Day 5: Testing and benchmarking

### Week 2: HTTP Keep-Alive
- Days 1-2: Connection state management
- Days 3-4: Client handler modifications
- Day 5: Testing and optimization

### Week 3: Index Implementation
- Days 1-2: Index management API
- Days 3-4: Query optimizer integration
- Day 5: Testing and benchmarking

## Success Metrics

1. **Latency Reduction**
   - Target: < 10ms average response time
   - Current: 50-200ms

2. **Throughput Improvement**
   - Target: 10,000+ queries/second
   - Current: ~1,000 queries/second

3. **Concurrent Connections**
   - Target: 1,000+ concurrent clients
   - Current: ~100 clients

## Risk Mitigation

1. **Backward Compatibility**
   - All changes maintain API compatibility
   - Existing clients continue to work

2. **Data Integrity**
   - Extensive testing before each phase
   - Rollback plan for each change

3. **Performance Regression**
   - Benchmark after each change
   - Keep optimization behind feature flags initially

## Next Steps

1. Create feature branch: `feature/performance-optimizations`
2. Implement Phase 1 (Read-Write Locks)
3. Benchmark and document results
4. Proceed to Phase 2