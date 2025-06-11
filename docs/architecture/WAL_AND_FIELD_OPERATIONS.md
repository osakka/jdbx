# Write-Ahead Logging (WAL) and Field-Level Operations

**Date**: June 11, 2025  
**Status**: Production Ready  
**Version**: 1.0.0  
**Last Updated**: June 11, 2025

## Overview

JSONdb implements Write-Ahead Logging (WAL) for durability and field-level operations for granular document manipulation. This document details both systems and their integration with the overall architecture.

## Write-Ahead Logging (WAL)

### Purpose

WAL provides ACID properties for JSONdb operations:

1. **Atomicity**: Operations complete fully or not at all
2. **Consistency**: Database remains consistent after crashes
3. **Isolation**: Concurrent operations don't interfere
4. **Durability**: Committed changes survive system failures

### WAL Architecture

#### Log Structure

The WAL is implemented as a circular buffer with the following structure:

```c
typedef struct {
    uint32_t magic;              // Magic number: 0x57414C4C ("WALL")
    uint32_t version;            // WAL format version
    uint64_t sequence;           // Current sequence number
    uint64_t checkpoint_lsn;     // Last checkpoint sequence
    uint64_t size;              // Total WAL size
    uint32_t checksum;          // Header checksum
} wal_header_t;

typedef struct {
    uint64_t lsn;               // Log sequence number
    uint64_t transaction_id;    // Transaction identifier
    uint32_t type;              // Operation type
    uint32_t page_number;       // Affected page
    uint32_t data_size;         // Size of data payload
    uint8_t data[];            // Operation data
} wal_entry_t;
```

#### Operation Types

WAL supports the following operation types:

1. **PAGE_WRITE**: Full page write
2. **PAGE_UPDATE**: Partial page update
3. **BTREE_INSERT**: B-tree key insertion
4. **BTREE_DELETE**: B-tree key deletion
5. **BTREE_UPDATE**: B-tree value update
6. **TRANSACTION_BEGIN**: Transaction start marker
7. **TRANSACTION_COMMIT**: Transaction completion
8. **TRANSACTION_ROLLBACK**: Transaction cancellation

### WAL Operations

#### Write Process

1. **Pre-Write**: Generate WAL entry before modifying data
2. **Log Write**: Write entry to WAL with fsync
3. **Data Write**: Apply changes to data pages
4. **Completion**: Mark operation complete in WAL

```c
// Example WAL write operation
int wal_write_page(wal_t* wal, uint32_t page_num, void* data, size_t size) {
    wal_entry_t* entry = create_wal_entry(WAL_PAGE_WRITE, page_num, data, size);
    
    // Write to WAL first
    if (wal_append(wal, entry) != 0) {
        return -1;
    }
    
    // Then write to data file
    if (write_page(page_num, data) != 0) {
        // Rollback WAL entry on failure
        wal_rollback_last(wal);
        return -1;
    }
    
    return 0;
}
```

#### Recovery Process

On startup, JSONdb performs WAL recovery:

1. **Read WAL Header**: Validate WAL integrity
2. **Find Checkpoint**: Locate last valid checkpoint
3. **Replay Operations**: Apply all operations after checkpoint
4. **Create Checkpoint**: Write new checkpoint after recovery

### WAL Configuration

WAL behavior is configurable through environment variables:

```bash
# WAL size (default: 10MB)
JSONDB_JDBX_WAL_SIZE=10485760

# Checkpoint interval (default: 1000 operations)
JSONDB_WAL_CHECKPOINT_INTERVAL=1000

# WAL sync mode (default: fsync)
JSONDB_WAL_SYNC_MODE=fsync  # Options: fsync, fdatasync, none
```

### Performance Considerations

1. **Group Commit**: Batch multiple operations before fsync
2. **Parallel Recovery**: Use multiple threads for WAL replay
3. **Compression**: Optional compression for WAL entries
4. **Archiving**: Move old WAL segments to archive storage

## Field-Level Operations

### Overview

Field-level operations allow granular manipulation of document fields without loading entire documents, improving performance for large documents.

### Supported Operations

#### Field Read

Read specific fields from a document:

```c
// Read single field
json_value_t* db_get_field(database_t* db, const char* collection, 
                           const char* doc_id, const char* field_path);

// Read multiple fields
json_value_t* db_get_fields(database_t* db, const char* collection,
                            const char* doc_id, const char** field_paths, 
                            size_t field_count);
```

#### Field Update

Update specific fields without affecting others:

```c
// Update single field
int db_update_field(database_t* db, const char* collection,
                    const char* doc_id, const char* field_path,
                    json_value_t* value);

// Update multiple fields atomically
int db_update_fields(database_t* db, const char* collection,
                     const char* doc_id, field_update_t* updates,
                     size_t update_count);
```

#### Field Delete

Remove fields from documents:

```c
// Delete single field
int db_delete_field(database_t* db, const char* collection,
                    const char* doc_id, const char* field_path);

// Delete multiple fields
int db_delete_fields(database_t* db, const char* collection,
                     const char* doc_id, const char** field_paths,
                     size_t field_count);
```

### Field Path Syntax

Field paths support nested object and array access:

```javascript
// Object field access
"user.profile.email"

// Array index access
"items[0].price"

// Mixed access
"order.items[2].product.name"

// Wildcard for all array elements
"items[*].quantity"
```

### Implementation Details

#### Storage Format

Field-level operations use a delta-based storage format:

```c
typedef struct {
    uint32_t version;           // Document version
    uint32_t base_size;         // Size of base document
    uint32_t delta_count;       // Number of deltas
    uint8_t base_data[];        // Base document data
    field_delta_t deltas[];     // Field-level changes
} field_document_t;

typedef struct {
    uint32_t operation;         // ADD, UPDATE, DELETE
    uint32_t field_path_len;    // Length of field path
    uint32_t value_size;        // Size of new value
    char field_path[];          // Field path string
    uint8_t value[];           // New value (if applicable)
} field_delta_t;
```

#### Merge Strategy

Deltas are periodically merged into base documents:

1. **Threshold-Based**: Merge after N deltas
2. **Time-Based**: Merge after time interval
3. **Size-Based**: Merge when deltas exceed percentage of base

### RBAC Integration

Field-level operations respect RBAC permissions:

```json
{
  "role": "data_analyst",
  "permissions": [
    {
      "resource": "sales.orders",
      "actions": ["read"],
      "fields": {
        "allow": ["order_id", "total", "customer.name"],
        "deny": ["customer.ssn", "payment.card_number"]
      }
    }
  ]
}
```

### API Examples

#### REST API

```bash
# Read specific fields
GET /api/libraries/ecommerce/collections/products/documents/prod-123?fields=name,price,stock

# Update specific fields
PATCH /api/libraries/ecommerce/collections/products/documents/prod-123
{
  "fields": {
    "price": 29.99,
    "stock": 150
  }
}

# Delete fields
DELETE /api/libraries/ecommerce/collections/products/documents/prod-123/fields
{
  "fields": ["deprecated_field", "old_category"]
}
```

#### JavaScript API

```javascript
// Read fields
const fields = await db.collection('products').document('prod-123')
  .getFields(['name', 'price', 'category.name']);

// Update fields
await db.collection('products').document('prod-123')
  .updateFields({
    'price': 29.99,
    'category.featured': true,
    'tags[0]': 'bestseller'
  });

// Delete fields
await db.collection('products').document('prod-123')
  .deleteFields(['old_field', 'deprecated.section']);
```

## WAL and Field Operations Integration

### Atomic Field Updates

Field operations are logged in WAL for atomicity:

```c
int db_update_field_atomic(database_t* db, const char* collection,
                          const char* doc_id, const char* field_path,
                          json_value_t* value) {
    // Begin WAL transaction
    uint64_t txn_id = wal_begin_transaction(db->wal);
    
    // Log field update in WAL
    wal_log_field_update(db->wal, txn_id, collection, doc_id, 
                        field_path, value);
    
    // Apply update
    int result = apply_field_update(db, collection, doc_id, 
                                   field_path, value);
    
    if (result == 0) {
        // Commit on success
        wal_commit_transaction(db->wal, txn_id);
    } else {
        // Rollback on failure
        wal_rollback_transaction(db->wal, txn_id);
    }
    
    return result;
}
```

### Recovery Handling

Field operations are recovered from WAL:

1. **Identify Field Operations**: Scan WAL for field-level changes
2. **Reconstruct State**: Apply field deltas in order
3. **Merge Deltas**: Compact deltas during recovery
4. **Verify Integrity**: Validate document structure after recovery

## Performance Optimization

### Field Operation Batching

Batch multiple field operations for efficiency:

```c
// Batch multiple field updates
field_batch_t* batch = field_batch_create();
field_batch_add_update(batch, "field1", value1);
field_batch_add_update(batch, "field2", value2);
field_batch_add_delete(batch, "field3");

// Execute as single WAL transaction
db_execute_field_batch(db, collection, doc_id, batch);
```

### Lazy Field Loading

Load fields only when accessed:

```c
// Document proxy with lazy loading
document_proxy_t* proxy = db_get_document_proxy(db, collection, doc_id);

// Fields loaded on demand
json_value_t* name = proxy_get_field(proxy, "name");  // Loads only name
json_value_t* price = proxy_get_field(proxy, "price"); // Loads only price
```

### Field Indexing

Create indexes on specific fields:

```sql
-- Conceptual index on nested field
CREATE INDEX idx_product_category ON products(category.name);
CREATE INDEX idx_user_email ON users(profile.email);
```

## Monitoring and Diagnostics

### WAL Metrics

Monitor WAL performance:

```json
{
  "wal_metrics": {
    "size_bytes": 10485760,
    "entries_count": 15234,
    "checkpoint_lsn": 15000,
    "write_throughput": "5.2 MB/s",
    "recovery_time_ms": 234,
    "compression_ratio": 0.65
  }
}
```

### Field Operation Metrics

Track field-level operation performance:

```json
{
  "field_metrics": {
    "field_reads_per_sec": 1250,
    "field_updates_per_sec": 450,
    "avg_field_size_bytes": 256,
    "delta_merge_count": 125,
    "cache_hit_ratio": 0.92
  }
}
```

## Best Practices

### WAL Management

1. **Size Appropriately**: Set WAL size based on write volume
2. **Regular Checkpoints**: Balance durability vs performance
3. **Monitor Growth**: Alert on excessive WAL growth
4. **Archive Old Logs**: Move old WAL segments to cold storage

### Field Operations

1. **Batch When Possible**: Group related field updates
2. **Use Projections**: Request only needed fields
3. **Index Key Fields**: Create indexes on frequently accessed fields
4. **Monitor Delta Growth**: Merge deltas before they impact performance

## Future Enhancements

### Planned WAL Features

1. **Parallel WAL**: Multiple WAL streams for better concurrency
2. **WAL Compression**: Built-in compression algorithms
3. **Remote WAL**: Stream WAL to remote storage for DR
4. **Point-in-Time Recovery**: Restore to any point using WAL

### Planned Field Features

1. **Field Triggers**: Execute functions on field changes
2. **Computed Fields**: Fields calculated from other fields
3. **Field Versioning**: Track history of field changes
4. **Field Encryption**: Encrypt specific sensitive fields

## Conclusion

The combination of Write-Ahead Logging and field-level operations provides JSONdb with enterprise-grade durability and performance. WAL ensures data consistency across failures while field-level operations enable efficient manipulation of large documents. Together, they form the foundation for JSONdb's reliability and scalability in production environments.