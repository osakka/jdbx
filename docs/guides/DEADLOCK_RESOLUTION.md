# Deadlock Detection and Resolution

This document describes the deadlock detection and automatic resolution mechanisms implemented in the JSON database.

## Overview

Deadlocks occur when two or more transactions are each waiting for a lock held by another, resulting in a circular dependency that prevents any of the transactions from proceeding. The database implements an automatic deadlock detection and resolution system to identify and resolve these situations.

## Key Features

- Wait-for graph construction for deadlock detection
- Cycle detection algorithm to identify deadlocks
- Automatic victim selection for deadlock resolution
- Manual deadlock checking API
- Support for complex deadlock scenarios with multiple cycles

## How Deadlock Detection Works

The deadlock detection system works by constructing a wait-for graph of transactions and their lock dependencies:

1. **Graph Construction**: Build a directed graph where nodes represent transactions and edges represent waiting relationships (transaction A is waiting for a lock held by transaction B).

2. **Cycle Detection**: Use depth-first search (DFS) to detect cycles in the wait-for graph, which indicate deadlocks.

3. **Victim Selection**: When a deadlock is detected, select a "victim" transaction to abort, breaking the deadlock cycle.

4. **Resolution**: Abort the victim transaction, allowing other transactions to proceed.

5. **Recursive Resolution**: For complex deadlock scenarios with multiple cycles, recursively resolve each deadlock until the system is deadlock-free.

## Victim Selection Policy

When choosing a transaction to abort, the system uses the following criteria:

1. **Transaction Age**: Select the youngest transaction (most recently started) as the victim, minimizing wasted work.

2. **Transaction State**: Only active transactions are considered as potential victims.

3. **Cycle Participation**: Only transactions that are part of the deadlock cycle can be selected as victims.

## API Usage

The database provides an API endpoint to manually check for and resolve deadlocks:

```http
POST /api/transactions/check-deadlocks
```

**Response:**

```json
{
  "deadlocks_detected": 1,
  "status": "success",
  "message": "Deadlocks detected and resolved"
}
```

## Automatic Deadlock Detection

In addition to manual checking, the database periodically checks for deadlocks in the following situations:

1. **Lock Timeouts**: When a lock request times out, the system checks for deadlocks to determine if the timeout was due to a deadlock.

2. **Transaction Operations**: During high-contention operations, the system may check for deadlocks to proactively resolve conflicts.

## Configuring Deadlock Detection

Deadlock detection can be configured at the system level:

- **Enabled/Disabled**: Deadlock detection can be enabled or disabled globally.
- **Timeout**: Configure the lock request timeout period, after which deadlock detection is triggered.

## Example Deadlock Scenario

Consider the following sequence of events:

1. Transaction A acquires an exclusive lock on document X.
2. Transaction B acquires an exclusive lock on document Y.
3. Transaction A requests an exclusive lock on document Y (must wait for B).
4. Transaction B requests an exclusive lock on document X (must wait for A).

This creates a deadlock because:
- A is waiting for B to release Y.
- B is waiting for A to release X.

The deadlock detector will:
1. Construct the wait-for graph showing A→B→A.
2. Detect the cycle in the graph.
3. Select the younger of transactions A and B as the victim.
4. Abort the victim transaction, releasing its locks.
5. Allow the remaining transaction to continue.

## Best Practices

1. **Keep Transactions Short**: Long-running transactions increase the likelihood of deadlocks.

2. **Consistent Lock Order**: When possible, acquire locks in a consistent order across transactions to prevent deadlocks.

3. **Use Appropriate Isolation Levels**: Higher isolation levels (like SERIALIZABLE) acquire more locks and increase deadlock probability. Use the lowest isolation level that meets your consistency requirements.

4. **Monitor Deadlock Frequency**: Regular deadlocks may indicate design issues in your application's transaction patterns.

5. **Implement Retry Logic**: Applications should be prepared to retry transactions that are aborted due to deadlocks.

## Deadlock Prevention vs. Detection

The database uses deadlock detection and resolution rather than deadlock prevention because:

1. **Performance**: Prevention techniques can be overly restrictive and reduce concurrency.

2. **Flexibility**: Detection allows more flexible transaction patterns while still addressing deadlocks when they occur.

3. **Simplicity**: Detection and resolution can be more straightforward to implement efficiently.

## Implementation Details

The deadlock detection algorithm uses a depth-first search to find cycles in the wait-for graph:

```c
int detect_cycle(wait_for_node_t* nodes, int node_count, int start_idx) {
    /* Mark the current node as being visited */
    nodes[start_idx].state = DL_IN_PROGRESS;
    
    /* Visit all adjacent vertices */
    for (each waiting edge) {
        if (adjacent node is already in the recursion stack) {
            return true; /* Deadlock detected */
        }
        
        if (adjacent node not yet visited) {
            if (detect_cycle(nodes, node_count, adjacent_node_idx)) {
                return true;
            }
        }
    }
    
    /* Mark the node as fully visited */
    nodes[start_idx].state = DL_VISITED;
    
    return false;
}
```

## Performance Considerations

Deadlock detection has the following performance characteristics:

- **Time Complexity**: O(T + E) where T is the number of transactions and E is the number of waiting relationships.
- **Space Complexity**: O(T) for storing the wait-for graph.
- **Execution Frequency**: Only performed on lock timeouts or manual checks, not on every lock request.

This makes deadlock detection efficient even in systems with many concurrent transactions.