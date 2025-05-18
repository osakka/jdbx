# Database Locking Architecture

This document describes the locking architecture implemented in the JSONdb server to ensure thread safety and improve concurrency.

## Overview

JSONdb uses a hierarchical locking system that combines read-write locks with mutexes to achieve optimal concurrency while ensuring data consistency. The system is designed to allow multiple concurrent read operations while serializing write operations as needed.

## Key Components

### 1. Database-Level Locks

The `database_t` structure includes:

- `pthread_rwlock_t rwlock`: Read-write lock for database access
  - Multiple threads can acquire read locks simultaneously
  - Write operations require exclusive access
- `pthread_mutex_t mutex`: Mutex for operations requiring exclusive access to specific fields
  - Used primarily for atomic updates to fields like `is_modified`

### 2. Collection-Level Locks

Each `db_collection_t` includes:

- `pthread_rwlock_t rwlock`: Read-write lock for collection access
  - Allows concurrent reads of documents within the same collection
  - Serializes write operations to a collection

### 3. Index-Level Locks

Document indexes use read-write locks to allow:

- Concurrent lookups in the same index
- Exclusive access for index modification

### 4. Lock Manager for Document-Level Locking

For fine-grained document access control:

- Supports shared (read) and exclusive (write) locks
- Provides deadlock detection
- Uses timeouts with a backoff strategy

## Locking Strategy

### Read Operations

For operations that only read data (e.g., `db_get_document`):

1. Acquire read lock on the database (`pthread_rwlock_rdlock(&db->rwlock)`)
2. Acquire read lock on the collection (`pthread_rwlock_rdlock(&coll->rwlock)`)
3. Perform read operation
4. Release locks in reverse order

```c
/* Example: Reading a document */
pthread_rwlock_rdlock(&db->rwlock);
// Get collection...
pthread_rwlock_rdlock(&coll->rwlock);
// Read document...
pthread_rwlock_unlock(&coll->rwlock);
pthread_rwlock_unlock(&db->rwlock);
```

### Write Operations

For operations that modify data (e.g., `db_insert_document`):

1. Acquire write lock on the database (`pthread_rwlock_wrlock(&db->rwlock)`)
2. Acquire write lock on the collection (`pthread_rwlock_wrlock(&coll->rwlock)`)
3. Perform write operation
4. Update `is_modified` flag using the mutex
5. Release locks in reverse order

```c
/* Example: Inserting a document */
pthread_rwlock_wrlock(&db->rwlock);
// Get collection...
pthread_rwlock_wrlock(&coll->rwlock);
// Write document...
pthread_mutex_lock(&db->mutex);
db->is_modified = 1;
pthread_mutex_unlock(&db->mutex);
pthread_rwlock_unlock(&coll->rwlock);
pthread_rwlock_unlock(&db->rwlock);
```

### Special Cases

- **Database Initialization**: A write lock is acquired during initialization since we're modifying the database structure.
- **Database Save**: A read lock is sufficient since we're only reading the collections to serialize them.
- **Cache Operations**: Special care is taken to avoid deadlocks between cache and database operations.

## Benefits

The new locking architecture provides the following benefits:

1. **Increased Concurrency**: Multiple readers can access the database simultaneously
2. **Reduced Contention**: Read operations do not block other read operations
3. **Fine-Grained Control**: Locks at different levels (database, collection, document) allow for more precise locking
4. **Clear Ownership**: Explicit lock acquisition and release patterns clarify resource ownership
5. **Prevention of Priority Inversion**: By using appropriate locking primitives

## Performance Considerations

- Read-write locks have slightly higher overhead than mutexes for exclusive access
- The improved concurrency typically outweighs the additional overhead
- For very frequent but small modifications to shared state (like counters), separate mutexes are used

## Best Practices

When implementing operations using the locking system:

1. Acquire locks in a consistent order (database → collection → document)
2. Release locks in reverse order
3. Minimize the duration locks are held
4. Use read locks when only reading data
5. Prefer read-write locks for most scenarios
6. Use mutexes only for simple atomic updates to specific fields

## Future Work

Potential improvements to the locking system:

1. Implementing intent locks for better concurrency
2. Adding support for lock upgrading (read → write)
3. Introducing timeout-based lock acquisition with exponential backoff
4. Implementing lock-free data structures for certain operations

## Related Components

- **Transaction System**: Uses the lock manager for transaction isolation
- **Cache System**: Coordinates with the locking system to ensure cache coherence
- **Query Engine**: Utilizes read locks for efficient querying

---

This architecture ensures thread safety while maximizing performance through appropriate concurrency control for the JSONdb database system.