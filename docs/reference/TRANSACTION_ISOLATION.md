# Transaction Isolation Levels and Locking

This document describes the transaction isolation levels and locking mechanisms implemented in the JSON database.

## Overview

The database supports ACID (Atomicity, Consistency, Isolation, Durability) transactions to ensure data integrity. Transaction isolation levels control how concurrent transactions interact with each other, particularly around visibility of uncommitted changes and prevention of various concurrency phenomena.

## Isolation Levels

The database supports three standard isolation levels:

### READ UNCOMMITTED

The lowest isolation level. Transactions can see uncommitted changes made by other transactions.

- No locks acquired for reads
- Exclusive locks acquired for writes
- Locks held until transaction commits or aborts

**Potential Issues:**
- Dirty Reads: A transaction can read uncommitted changes from another transaction
- Non-repeatable Reads: A transaction can get different results from the same query if executed multiple times
- Phantom Reads: A transaction can see different sets of rows that satisfy a condition if a query is executed multiple times

**Use Cases:**
- When highest performance is needed and occasional dirty reads are acceptable
- For read-heavy workloads where data consistency is less critical
- When working with statistical or approximate data

### READ COMMITTED

The default isolation level. Transactions can only see committed changes from other transactions.

- Shared locks acquired for reads, but released immediately after reading
- Exclusive locks acquired for writes, held until transaction commits or aborts
- Prevents dirty reads by only seeing committed data

**Potential Issues:**
- Non-repeatable Reads: A transaction can get different results from the same query if executed multiple times
- Phantom Reads: A transaction can see different sets of rows that satisfy a condition if a query is executed multiple times

**Use Cases:**
- General-purpose database operations
- When a good balance between performance and data consistency is required
- Most application workloads

### SERIALIZABLE

The highest isolation level. Transactions are completely isolated from each other, as if they were executed serially.

- Shared locks acquired for reads, held until transaction commits or aborts
- Exclusive locks acquired for writes, held until transaction commits or aborts
- Prevents all concurrency phenomena by providing complete isolation

**Potential Issues:**
- Lowest concurrency and potential for deadlocks
- May lead to performance issues with high contention workloads

**Use Cases:**
- Financial transactions
- Critical data operations where consistency is crucial
- When the application requires the highest level of data integrity

## Locking Mechanisms

The database uses a two-phase locking protocol to implement isolation levels:

### Lock Types

- **Shared (S) Locks**: Multiple transactions can hold shared locks on the same resource simultaneously. Used for read operations.
- **Exclusive (X) Locks**: Only one transaction can hold an exclusive lock on a resource. Used for write operations (insert, update, delete).

### Lock Granularity

- **Document-level Locks**: Locks can be acquired on individual documents for fine-grained concurrency control.
- **Collection-level Locks**: Locks can be acquired on entire collections when needed (e.g., for operations that affect multiple documents).

### Lock Management

The lock manager handles lock acquisition and release with the following features:

- Hash-based lock tracking for efficiency
- Waiting queues for pending lock requests
- Timeouts to prevent indefinite waiting
- Deadlock detection to identify and resolve deadlocks

### Lock Duration

- In **READ UNCOMMITTED** and **READ COMMITTED**, read locks are released immediately after reading.
- In **SERIALIZABLE**, read locks are held until the transaction commits or aborts.
- Write locks are always held until the transaction commits or aborts, regardless of isolation level.

## Usage in API

When beginning a transaction, you can specify the isolation level:

```
POST /api/transactions
{
  "isolation_level": "read_committed"  // Options: "read_uncommitted", "read_committed", "serializable"
}
```

If not specified, the default isolation level is READ COMMITTED.

## Example Scenarios

### Scenario 1: READ UNCOMMITTED

Transaction A updates a document while Transaction B reads it before A commits:
- Transaction A gets an exclusive lock, updates the document
- Transaction B reads the document without acquiring a lock
- Transaction B sees the uncommitted changes from Transaction A

### Scenario 2: READ COMMITTED

Transaction A updates a document while Transaction B reads it before A commits:
- Transaction A gets an exclusive lock, updates the document
- Transaction B tries to get a shared lock but must wait until A commits or aborts
- After A commits, B acquires the shared lock, reads the document, and releases the lock
- Transaction B only sees committed data

### Scenario 3: SERIALIZABLE

Transaction A queries a collection while Transaction B inserts a new document:
- Transaction A gets a shared lock on the collection
- Transaction B tries to get an exclusive lock but must wait until A commits or aborts
- Transaction A holds the shared lock until it commits
- Transaction B cannot insert until Transaction A completes

## Deadlock Prevention

To prevent deadlocks, the lock manager implements:

1. Lock timeout mechanism - requests will time out after 30 seconds by default
2. Deadlock detection - cycles in the lock waiting graph are detected and resolved
3. Lock request ordering - requests are granted in FIFO order when possible

## Conclusion

The transaction isolation levels and locking mechanisms ensure data integrity while allowing for different levels of concurrency based on application requirements. Choose the isolation level that balances performance and consistency needs for your specific use case.