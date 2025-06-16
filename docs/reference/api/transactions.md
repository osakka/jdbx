# JDBX Transaction System Reference

**Version**: 6.2.0  
**Last Updated**: January 2025

This comprehensive guide documents the JDBX transaction system, including ACID compliance, isolation levels, and advanced features.

## Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Transaction Model](#transaction-model)
4. [API Reference](#api-reference)
5. [Isolation Levels](#isolation-levels)
6. [Advanced Features](#advanced-features)
7. [Performance](#performance)
8. [Best Practices](#best-practices)

## Overview

The JDBX transaction system provides ACID-compliant transactions for maintaining data consistency across multiple operations. It supports concurrent transactions, automatic retry mechanisms, and comprehensive logging.

### Key Features

- **ACID Compliance**: Full atomicity, consistency, isolation, and durability
- **Multiple Isolation Levels**: Read Uncommitted, Read Committed, and Serializable
- **Automatic Retry**: Configurable retry for transient failures
- **Savepoints**: Nested transaction support with savepoints
- **Audit Trail**: Complete transaction history and logging
- **Visualization**: Transaction flow and dependency visualization
- **Performance Monitoring**: Detailed transaction metrics

## Architecture

### Core Components

1. **Transaction Manager**
   - Creates and tracks transactions
   - Manages transaction state lifecycle
   - Coordinates commit and rollback operations
   - Ensures isolation between concurrent transactions

2. **Transaction Log**
   - Records all operations within transactions
   - Maintains before/after states of documents
   - Provides recovery capabilities
   - Enables audit trail functionality

3. **Lock Manager**
   - Implements document-level locking
   - Provides collection-level locks for schema changes
   - Performs deadlock detection and resolution
   - Manages lock timeouts and escalation

### Data Structures

```c
typedef enum {
    TRANSACTION_ACTIVE,
    TRANSACTION_COMMITTING,
    TRANSACTION_COMMITTED,
    TRANSACTION_ABORTING,
    TRANSACTION_ABORTED
} transaction_state_t;

typedef struct transaction {
    char* id;                        /* Unique transaction ID */
    transaction_state_t state;       /* Current state */
    time_t start_time;              /* Start timestamp */
    time_t commit_time;             /* Commit timestamp */
    char* user_id;                  /* User who initiated */
    transaction_operation_t* operations; /* Operation list */
    int operation_count;            /* Number of operations */
    isolation_level_t isolation_level; /* Isolation level */
    pthread_mutex_t lock;           /* Transaction lock */
} transaction_t;
```

## Transaction Model

### ACID Properties

1. **Atomicity**
   - All operations within a transaction succeed or fail together
   - No partial updates are visible to other transactions
   - Automatic rollback on failure

2. **Consistency**
   - Database integrity constraints are maintained
   - Business rules are enforced
   - Schema validation is applied

3. **Isolation**
   - Concurrent transactions don't interfere
   - Multiple isolation levels available
   - Deadlock detection and resolution

4. **Durability**
   - Committed changes persist across restarts
   - Write-ahead logging ensures recovery
   - Binary persistence format for reliability

## API Reference

### Starting a Transaction

```http
POST /api/transactions
Authorization: Bearer <token>
Content-Type: application/json

{
  "isolation_level": "read_committed",
  "timeout": 30000
}
```

**Response**:
```json
{
  "status": "success",
  "data": {
    "transaction_id": "txn_1234567890",
    "state": "active",
    "isolation_level": "read_committed",
    "started_at": "2025-01-30T10:00:00Z"
  }
}
```

### Executing Operations

All standard CRUD operations can be executed within a transaction by including the transaction ID:

```http
POST /api/collections/users/documents
Authorization: Bearer <token>
X-Transaction-ID: txn_1234567890
Content-Type: application/json

{
  "username": "john_doe",
  "email": "john@example.com"
}
```

### Committing a Transaction

```http
POST /api/transactions/{transaction_id}/commit
Authorization: Bearer <token>
```

**Response**:
```json
{
  "status": "success",
  "data": {
    "transaction_id": "txn_1234567890",
    "state": "committed",
    "operations_count": 5,
    "committed_at": "2025-01-30T10:01:00Z"
  }
}
```

### Rolling Back a Transaction

```http
POST /api/transactions/{transaction_id}/rollback
Authorization: Bearer <token>
```

### Getting Transaction Status

```http
GET /api/transactions/{transaction_id}
Authorization: Bearer <token>
```

### Listing Active Transactions

```http
GET /api/transactions?state=active
Authorization: Bearer <token>
```

## Isolation Levels

### Read Uncommitted

- **Description**: Lowest isolation level
- **Behavior**: Can read uncommitted changes from other transactions
- **Use Case**: Read-heavy workloads where consistency is less critical
- **Performance**: Highest performance, lowest consistency

```json
{"isolation_level": "read_uncommitted"}
```

### Read Committed (Default)

- **Description**: Balanced isolation level
- **Behavior**: Only reads committed changes from other transactions
- **Use Case**: Most general-purpose applications
- **Performance**: Good performance with strong consistency

```json
{"isolation_level": "read_committed"}
```

### Serializable

- **Description**: Highest isolation level
- **Behavior**: Complete isolation between transactions
- **Use Case**: Financial or critical operations requiring absolute consistency
- **Performance**: Lower performance, highest consistency

```json
{"isolation_level": "serializable"}
```

## Advanced Features

### Savepoints

Create nested transaction points for partial rollback:

```http
POST /api/transactions/{transaction_id}/savepoints
Content-Type: application/json

{
  "name": "before_bulk_update"
}
```

Rollback to savepoint:

```http
POST /api/transactions/{transaction_id}/rollback-to-savepoint
Content-Type: application/json

{
  "savepoint": "before_bulk_update"
}
```

### Automatic Retry

Configure automatic retry for transient failures:

```json
{
  "retry": {
    "enabled": true,
    "max_attempts": 3,
    "backoff_ms": 100,
    "exponential_backoff": true
  }
}
```

### Transaction Monitoring

Monitor transaction performance and status:

```http
GET /api/transactions/metrics
```

**Response**:
```json
{
  "active_transactions": 5,
  "avg_transaction_duration_ms": 250,
  "transactions_per_second": 100,
  "rollback_rate": 0.02,
  "deadlock_count": 0
}
```

### Audit Trail

Query transaction history:

```http
GET /api/transactions/audit?user_id=user123&from=2025-01-01
```

### Visualization

Get transaction dependency graph:

```http
GET /api/transactions/visualization
```

Returns a graph structure showing transaction dependencies and lock relationships.

## Performance

### Optimization Strategies

1. **Lock Granularity**
   - Document-level locks for fine-grained control
   - Collection locks only for schema operations
   - Lock escalation for bulk operations

2. **Deadlock Prevention**
   - Consistent lock ordering
   - Timeout-based deadlock detection
   - Automatic retry with backoff

3. **Write-Ahead Logging**
   - Asynchronous log writes
   - Group commit optimization
   - Configurable sync intervals

### Performance Metrics

| Metric | Value |
|--------|-------|
| Transaction Overhead | ~5-10ms |
| Commit Latency | ~20-50ms |
| Rollback Speed | ~10-30ms |
| Max Concurrent Transactions | 1000 |
| Lock Acquisition Time | <1ms |

## Best Practices

### Transaction Design

1. **Keep Transactions Short**
   - Minimize lock hold time
   - Reduce deadlock probability
   - Improve concurrency

2. **Use Appropriate Isolation**
   - Default to Read Committed
   - Use Serializable only when necessary
   - Consider Read Uncommitted for analytics

3. **Handle Failures Gracefully**
   - Implement retry logic
   - Use savepoints for complex operations
   - Log transaction failures

### Error Handling

```javascript
async function executeWithTransaction(operations) {
  const txn = await startTransaction();
  
  try {
    for (const op of operations) {
      await executeOperation(op, txn.id);
    }
    await commitTransaction(txn.id);
  } catch (error) {
    await rollbackTransaction(txn.id);
    
    if (error.code === 'DEADLOCK' && retries < 3) {
      // Retry with exponential backoff
      await sleep(100 * Math.pow(2, retries));
      return executeWithTransaction(operations);
    }
    
    throw error;
  }
}
```

### Monitoring

1. **Track Key Metrics**
   - Transaction duration
   - Rollback rate
   - Deadlock frequency
   - Lock wait times

2. **Set Alerts**
   - High rollback rate (>5%)
   - Long-running transactions (>30s)
   - Deadlock spikes
   - Transaction queue depth

3. **Regular Analysis**
   - Review transaction logs
   - Identify optimization opportunities
   - Monitor isolation level usage

## Troubleshooting

### Common Issues

#### Deadlocks

**Symptoms**: Transactions timing out, error code `DEADLOCK`

**Solutions**:
1. Review lock ordering in application
2. Reduce transaction scope
3. Implement retry logic
4. Consider lower isolation level

#### Long-Running Transactions

**Symptoms**: Slow performance, lock contention

**Solutions**:
1. Break into smaller transactions
2. Use savepoints for checkpointing
3. Review operation efficiency
4. Consider batch processing

#### High Rollback Rate

**Symptoms**: Many failed transactions

**Solutions**:
1. Review application logic
2. Check for constraint violations
3. Monitor for conflicts
4. Implement proper error handling

## Configuration

### Transaction Settings

```json
{
  "transactions": {
    "max_concurrent": 1000,
    "default_timeout_ms": 30000,
    "default_isolation": "read_committed",
    "enable_deadlock_detection": true,
    "deadlock_timeout_ms": 5000,
    "log_slow_transactions": true,
    "slow_transaction_threshold_ms": 10000
  }
}
```

### Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `JDBX_TXN_MAX_CONCURRENT` | Max concurrent transactions | `1000` |
| `JDBX_TXN_DEFAULT_TIMEOUT` | Default timeout (ms) | `30000` |
| `JDBX_TXN_LOG_SLOW` | Log slow transactions | `true` |
| `JDBX_TXN_SLOW_THRESHOLD` | Slow transaction threshold (ms) | `10000` |

## References

- [Transaction Implementation](src/components/transaction/transaction.c)
- [Lock Manager](src/components/database/lock_manager.c)
- [Transaction Log](src/components/transaction/transaction_log.c)
- [API Documentation](docs/api/api-rest.md#transactions)