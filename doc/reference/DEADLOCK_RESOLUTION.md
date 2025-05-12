# Deadlock Detection and Resolution

This document describes the enhanced deadlock detection and automatic resolution mechanisms implemented in the JSON database.

## Overview

Deadlocks occur when two or more transactions are each waiting for a lock held by another, resulting in a circular dependency that prevents any of the transactions from proceeding. The database implements an advanced deadlock detection and resolution system to identify and resolve these situations efficiently.

## Key Features

- Optimized wait-for graph construction for deadlock detection
- Enhanced cycle detection algorithm with full cycle tracking
- Multi-factor victim selection for intelligent deadlock resolution
- Comprehensive deadlock statistics and metrics
- Detailed logging for deadlock analysis
- Robust handling of complex deadlock scenarios with multiple cycles
- Manual and automatic deadlock checking

## How Deadlock Detection Works

The deadlock detection system works by constructing a wait-for graph of transactions and their lock dependencies:

1. **Graph Construction**: Build a directed graph where nodes represent transactions and edges represent waiting relationships (transaction A is waiting for a lock held by transaction B).

2. **Cycle Detection**: Use an enhanced depth-first search (DFS) algorithm to detect cycles in the wait-for graph, which indicate deadlocks. Our implementation tracks full cycles to provide complete information about deadlock patterns.

3. **Victim Selection**: When a deadlock is detected, select a "victim" transaction to abort using our multi-factor scoring algorithm.

4. **Resolution**: Abort the victim transaction, releasing its locks and recording detailed statistics.

5. **Iterative Resolution**: For complex deadlock scenarios with multiple cycles, iteratively resolve each deadlock until the system is deadlock-free, with safeguards to prevent infinite resolution loops.

## Multi-Factor Victim Selection

Our enhanced victim selection algorithm considers multiple factors, not just transaction age:

1. **Transaction Age** (40% weight): Newer transactions are more likely to be selected as victims, minimizing wasted work.

2. **Operation Count** (25% weight): Transactions with fewer operations are preferred as victims to minimize the cost of rollback.

3. **Waiting Count** (20% weight): Transactions blocking more others receive higher scores, as aborting them unblocks more work.

4. **Isolation Level** (15% weight): Higher isolation levels (especially SERIALIZABLE) increase deadlock probability and are more likely to be selected.

Each factor is normalized and weighted to produce a comprehensive score that helps the system make optimal victim selection decisions, balancing efficiency and fairness.

## API Usage

The database provides API endpoints to manually check for and resolve deadlocks, and to retrieve deadlock statistics:

```http
POST /api/transactions/check-deadlocks
```

**Response:**

```json
{
  "deadlocks_detected": 1,
  "deadlocks_resolved": 1,
  "status": "success",
  "message": "Deadlocks detected and resolved",
  "victim_transaction_id": "tx-12345"
}
```

To retrieve deadlock statistics:

```http
GET /api/transactions/deadlock-stats
```

**Response:**

```json
{
  "deadlock_stats": {
    "total_deadlocks": 42,
    "total_aborted_transactions": 42,
    "complex_deadlocks": 5,
    "last_deadlock_time": "2025-05-12 15:30:45",
    "last_victim_id": "tx-12345",
    "last_cycle_length": 3,
    "by_isolation_level": {
      "read_uncommitted": 2,
      "read_committed": 15,
      "serializable": 25
    }
  }
}
```

## Deadlock Statistics

The system maintains detailed statistics about deadlocks:

- Total deadlocks detected
- Total transactions aborted due to deadlocks
- Complex deadlock scenarios encountered
- Timestamp of the last deadlock
- ID of the last victim transaction
- Length of the last deadlock cycle
- Breakdown of deadlocks by isolation level

These statistics help administrators understand deadlock patterns and optimize the system's configuration and application design.

## Automatic Deadlock Detection

In addition to manual checking, the database periodically checks for deadlocks in the following situations:

1. **Lock Timeouts**: When a lock request times out, the system checks for deadlocks to determine if the timeout was due to a deadlock.

2. **Transaction Operations**: During high-contention operations, the system may check for deadlocks to proactively resolve conflicts.

## Configuring Deadlock Detection

Deadlock detection can be configured at the system level:

- **Enabled/Disabled**: Deadlock detection can be enabled or disabled globally.
- **Timeout**: Configure the lock request timeout period, after which deadlock detection is triggered.
- **Statistics Reset**: Reset deadlock statistics to begin a new monitoring period.

## Example Deadlock Scenario

Consider the following sequence of events:

1. Transaction A acquires an exclusive lock on document X.
2. Transaction B acquires an exclusive lock on document Y.
3. Transaction A requests an exclusive lock on document Y (must wait for B).
4. Transaction B requests an exclusive lock on document X (must wait for A).

This creates a deadlock because:
- A is waiting for B to release Y.
- B is waiting for A to release X.

Our enhanced deadlock detector will:
1. Construct the wait-for graph showing A→B→A.
2. Detect and log the full cycle in the graph.
3. Score both transactions using our multi-factor algorithm.
4. Select the transaction with the highest score as the victim.
5. Abort the victim transaction, releasing its locks.
6. Update deadlock statistics.
7. Allow the remaining transaction to continue.

## Complex Deadlock Handling

The system can handle complex deadlock scenarios involving multiple transactions and multiple cycles:

1. **Multiple Cycles**: The system detects and resolves multiple deadlock cycles iteratively.
2. **Safeguards**: A maximum iteration limit prevents infinite resolution loops.
3. **Statistics**: Complex deadlocks are tracked separately for monitoring.

## Best Practices

1. **Keep Transactions Short**: Long-running transactions increase the likelihood of deadlocks.

2. **Consistent Lock Order**: When possible, acquire locks in a consistent order across transactions to prevent deadlocks.

3. **Use Appropriate Isolation Levels**: Higher isolation levels (like SERIALIZABLE) acquire more locks and increase deadlock probability. Use the lowest isolation level that meets your consistency requirements.

4. **Monitor Deadlock Statistics**: Regular monitoring of deadlock patterns may indicate design issues in your application's transaction patterns.

5. **Implement Retry Logic**: Applications should be prepared to retry transactions that are aborted due to deadlocks.

6. **Analyze Deadlock Logs**: The enhanced logging system provides detailed information to help identify and resolve recurring deadlock patterns.

## Deadlock Prevention vs. Detection

The database uses deadlock detection and resolution rather than deadlock prevention because:

1. **Performance**: Prevention techniques can be overly restrictive and reduce concurrency.

2. **Flexibility**: Detection allows more flexible transaction patterns while still addressing deadlocks when they occur.

3. **Simplicity**: Detection and resolution can be more straightforward to implement efficiently.

## Implementation Details

The enhanced deadlock detection algorithm uses an improved depth-first search with cycle tracking:

```c
int detect_cycle_enhanced(wait_for_node_t* nodes, int node_count, int start_idx,
                        deadlock_cycle_t* cycle, int depth, int max_depth) {
    /* Avoid excessive recursion */
    if (depth > max_depth) {
        return 0;
    }

    /* Mark the current node as being visited */
    nodes[start_idx].state = DL_IN_PROGRESS;

    /* Add this transaction to the cycle if we're tracking it */
    if (cycle) {
        add_to_deadlock_cycle(cycle, nodes[start_idx].transaction);
    }

    /* Visit all adjacent vertices */
    for (int i = 0; i < nodes[start_idx].waiting_count; i++) {
        /* Find the node index for the transaction we're waiting for */
        int wait_idx = find_node_for_transaction(nodes, node_count,
                                              nodes[start_idx].waiting_for[i]);

        if (wait_idx >= 0) {
            /* If adjacent vertex is already in the recursion stack, we found a cycle */
            if (nodes[wait_idx].state == DL_IN_PROGRESS) {
                if (cycle) {
                    /* Complete the cycle for accurate tracking */
                    add_to_deadlock_cycle(cycle, nodes[wait_idx].transaction);
                }
                return 1;
            }

            /* If not visited yet, recursively check for cycles */
            if (nodes[wait_idx].state == DL_NOT_VISITED) {
                if (detect_cycle_enhanced(nodes, node_count, wait_idx,
                                       cycle, depth + 1, max_depth)) {
                    return 1;
                }
            }
        }
    }

    /* If we get here and we're tracking cycles, remove this transaction */
    if (cycle && cycle->cycle_length > 0) {
        cycle->cycle_length--;
    }

    /* Mark the node as fully visited */
    nodes[start_idx].state = DL_VISITED;

    return 0;
}
```

## Performance Considerations

Our enhanced deadlock detection has the following performance characteristics:

- **Time Complexity**: O(T + E) where T is the number of transactions and E is the number of waiting relationships.
- **Space Complexity**: O(T) for storing the wait-for graph, plus O(C) for cycle tracking where C is the maximum cycle length.
- **Execution Frequency**: Only performed on lock timeouts or manual checks, not on every lock request.
- **Iterative Resolution**: For complex deadlocks, we limit iterations to prevent performance issues.

These optimizations make deadlock detection efficient even in systems with many concurrent transactions.

## Deadlock Metrics and Monitoring

The system provides detailed metrics about deadlock patterns, helping administrators identify and address problematic transaction patterns:

1. **Frequency Tracking**: Monitor how often deadlocks occur over time.
2. **Isolation Level Analysis**: Identify which isolation levels are most prone to deadlocks.
3. **Complex Deadlock Monitoring**: Track the occurrence of multi-cycle deadlocks.
4. **Victim Selection Patterns**: Understand which transactions are most often selected as victims.

Regular monitoring of these metrics can help optimize application design and database configuration.