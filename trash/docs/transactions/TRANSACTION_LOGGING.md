# Transaction Logging System

The QJSDB transaction logging system provides durability and recovery capabilities for the database's transaction system. This document explains the design, implementation, and usage of the transaction logging system.

## Overview

The transaction logging system records all transaction operations and state changes to a persistent log file. This allows the database to recover from crashes or unexpected shutdowns by replaying committed transactions and rolling back incomplete transactions.

## Log Format

Each log entry is a single line in the following format:

```
TIMESTAMP|TYPE|TRANSACTION_ID|DATA_JSON
```

Where:
- `TIMESTAMP`: Unix timestamp (seconds since epoch)
- `TYPE`: Either "STATE" or "OPERATION"
- `TRANSACTION_ID`: Transaction identifier
- `DATA_JSON`: JSON representation of the log entry data

### STATE Entry Format

STATE entries record transaction state changes:

```json
{
  "state": "active|committing|committed|aborting|aborted",
  "user_id": "user123",
  "isolation_level": "read_uncommitted|read_committed|serializable",
  "commit_time": 1714589876
}
```

The `isolation_level` is included only in entries with state "active".
The `commit_time` is included only in entries with state "committed".

### OPERATION Entry Format

OPERATION entries record document operations within transactions:

```json
{
  "type": "insert|update|delete",
  "collection": "collection_name",
  "document_id": "doc123", // may be null for inserts
  "before_state": {...},   // may be null for inserts
  "after_state": {...}     // may be null for deletes
}
```

## Recovery Process

During database startup, the transaction manager reads the transaction log file and performs the following steps:

1. Parse each entry in the log file
2. Group entries by transaction ID
3. For each transaction:
   - If the final state is "committed", apply all operations
   - If the final state is "aborted", do nothing
   - If the final state is "active", "committing", or "aborting", roll back any applied operations

The recovery process ensures that transactions follow the ACID principle of durability - once a transaction is committed, its effects will persist even in the event of a system failure.

## Configuration

The transaction log file location can be configured through the `JSONDB_TRANSACTION_LOG_PATH` environment variable. If not set, the default location is `data/transactions.log`.

Example:

```bash
export JSONDB_TRANSACTION_LOG_PATH=/var/lib/jsondb/transactions.log
```

## Log Maintenance

Over time, the transaction log can grow large. The system provides a log compaction feature that reduces the log size by:

1. Removing entries for fully committed or aborted transactions
2. Keeping only the most recent valid state for each active transaction
3. Reorganizing the log to optimize recovery speed

Log compaction is automatically triggered when:
- The log file exceeds a configured size threshold
- The database shuts down cleanly
- An admin API endpoint is called to request compaction

## Error Handling

The transaction logging system is designed to be resilient to errors:

- If writing to the log fails, the corresponding transaction operation will be aborted
- Malformed log entries are skipped during recovery
- Partial writes are detected and handled appropriately

## Performance Considerations

The transaction logging system can impact performance, particularly for write-heavy workloads. Some performance considerations:

1. **Write Amplification**: Each database write operation will result in at least one log entry.
2. **Synchronous Writes**: Log entries are synchronously written to disk to ensure durability.
3. **Log File Location**: For best performance, place the log file on a fast storage device (SSD/NVMe).

To optimize performance:
- Consider batching multiple operations in a single transaction
- When durability is less critical, configure the log to use asynchronous writes
- Schedule regular log compaction during low-usage periods

## API Reference

### Transaction Manager Integration

The transaction manager automatically initializes the transaction log:

```c
/* Create a transaction manager with logging */
transaction_manager_t* manager = transaction_manager_create(db, capacity);
```

### Log Operations

Log operations happen automatically when performing transaction operations:

```c
/* Begin a transaction - automatically logs the start state */
transaction_t* transaction = transaction_begin(manager, isolation_level, user_id);

/* Perform operations - each is automatically logged */
transaction_insert_document(manager, transaction, collection, document);
transaction_update_document(manager, transaction, collection, id, document);
transaction_delete_document(manager, transaction, collection, id);

/* Commit or rollback - automatically logs the end state */
transaction_commit(manager, transaction);
transaction_rollback(manager, transaction);
```

### Manual Log Management

For administrative purposes, the following functions are available:

```c
/* Get transaction log statistics */
json_value_t* stats = transaction_log_get_stats(manager->log);

/* Manually compact the log file */
transaction_log_compact(manager->log);
```

## Best Practices

1. **Ensure Directory Permissions**: The database process must have write permissions to the log file directory.
2. **Regular Backups**: While the transaction log provides recovery, regular database backups are still recommended.
3. **Monitor Log Size**: Set up monitoring for log file size to prevent disk space issues.
4. **Transaction Size**: Keep transactions small and focused for better performance and recovery speed.
5. **Testing Recovery**: Regularly test the recovery process to ensure it works as expected in your environment.

## Limitations

- The current implementation does not support distributed transactions across multiple database nodes.
- Very large transactions may impact recovery time.
- Log compaction is not yet optimized for extremely high-throughput scenarios.

## Future Enhancements

Planned enhancements to the transaction logging system include:

1. **Streaming log compaction** that can run concurrently with normal operations
2. **Log archiving** for long-term storage of historical transactions
3. **Point-in-time recovery** to restore the database to a specific moment in time
4. **Replication** to mirror transaction logs to standby servers for high availability