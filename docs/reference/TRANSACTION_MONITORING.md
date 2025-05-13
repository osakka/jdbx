# Transaction Monitoring and Metrics

This document describes the transaction monitoring features and metrics API of the JSON database.

## Overview

The transaction monitoring system provides detailed information about active transactions and their current state, as well as aggregated metrics to help administrators understand the overall transaction health of the database.

## Transaction Status API

### Getting Basic Status for a Single Transaction

```http
GET /api/transactions/:id
```

**Response:**

```json
{
  "transaction_id": "txn-12345",
  "status": "active",
  "isolation_level": "read_committed",
  "start_time": 1651234567,
  "operation_count": 5
}
```

### Getting Detailed Status for a Transaction

```http
GET /api/transactions/:id/status
```

**Response:**

```json
{
  "transaction_id": "txn-12345",
  "user_id": "admin",
  "state": "active",
  "isolation_level": "read_committed",
  "start_time": 1651234567,
  "operation_count": 5,
  "timeout_sec": 60,
  "elapsed_seconds": 32,
  "savepoints": [
    {
      "name": "after_user_creation",
      "operation_count": 2
    },
    {
      "name": "after_preferences",
      "operation_count": 4
    }
  ],
  "recent_operations": [
    {
      "type": "insert",
      "collection": "users",
      "document_id": "user123"
    },
    {
      "type": "insert",
      "collection": "preferences",
      "document_id": "pref123"
    },
    {
      "type": "update",
      "collection": "users",
      "document_id": "user123"
    }
  ]
}
```

## Transaction Metrics API

The metrics API provides aggregated statistics about all transactions in the system. This is useful for monitoring overall database health and identifying potential issues.

```http
GET /api/transactions/metrics
```

**Response:**

```json
{
  "active_transactions": 10,
  "max_capacity": 100,
  "capacity_usage_percent": 10.0,
  "transactions_by_state": {
    "active": 8,
    "committing": 2,
    "committed": 0,
    "aborting": 0,
    "aborted": 0
  },
  "transactions_by_isolation": {
    "read_uncommitted": 2,
    "read_committed": 6,
    "serializable": 2
  },
  "operations": {
    "total": 156,
    "insert": 55,
    "update": 89,
    "delete": 12
  },
  "duration": {
    "oldest_active_seconds": 120,
    "newest_active_seconds": 5,
    "average_active_seconds": 42.5
  },
  "locks": {
    "total_resources": 25,
    "shared_locks": 15,
    "exclusive_locks": 8,
    "waiting_requests": 2,
    "deadlock_detection": true,
    "timeout_ms": 30000
  }
}
```

## Metrics Description

### General Statistics

- `active_transactions`: Current number of active transactions
- `max_capacity`: Maximum allowed concurrent transactions
- `capacity_usage_percent`: Percentage of transaction capacity currently in use

### Transactions by State

- `active`: Number of transactions in the active state
- `committing`: Number of transactions currently committing
- `committed`: Number of recently committed transactions still in memory
- `aborting`: Number of transactions currently aborting
- `aborted`: Number of recently aborted transactions still in memory

### Transactions by Isolation Level

- `read_uncommitted`: Transactions with READ UNCOMMITTED isolation
- `read_committed`: Transactions with READ COMMITTED isolation
- `serializable`: Transactions with SERIALIZABLE isolation

### Operations

- `total`: Total number of operations across all active transactions
- `insert`: Number of insert operations
- `update`: Number of update operations
- `delete`: Number of delete operations

### Duration

- `oldest_active_seconds`: Duration of the oldest active transaction
- `newest_active_seconds`: Duration of the newest active transaction
- `average_active_seconds`: Average duration of all active transactions

### Locks

- `total_resources`: Number of resources (documents or collections) with active locks
- `shared_locks`: Number of shared (read) locks
- `exclusive_locks`: Number of exclusive (write) locks
- `waiting_requests`: Number of lock requests waiting to be granted
- `deadlock_detection`: Whether deadlock detection is enabled
- `timeout_ms`: Lock acquisition timeout in milliseconds

## Use Cases

### Monitoring Transaction Health

Regular polling of the metrics API allows you to monitor the overall health of the transaction system, including:

- Transaction throughput
- Average transaction duration
- Lock contention levels
- Resource usage

### Debugging Long-Running Transactions

When a transaction is taking longer than expected, you can use the status API to:

- Check which operations have been performed
- See how long the transaction has been running
- Check if the transaction has created savepoints
- Identify potential lock contention issues

### Capacity Planning

Monitoring transaction metrics over time helps with capacity planning by understanding:

- Peak transaction loads
- Transaction patterns by time of day or day of week
- Lock usage patterns
- Common operation types

## Integration with Monitoring Systems

The metrics API is designed to be easily integrated with monitoring and alerting systems. Consider setting up alerts for:

- High transaction capacity usage (> 80%)
- Long-running transactions (> 5 minutes)
- High lock wait counts (indicating contention)
- Deadlocks detected

## Best Practices

1. **Poll metrics at regular intervals** (every 30-60 seconds) to track trends.
2. **Set up alerts** for abnormal conditions like high transaction counts or long durations.
3. **Monitor lock metrics closely** as they often indicate performance issues.
4. **Check transaction status** when investigating performance problems.
5. **Track metrics over time** to establish normal baselines for your workload.