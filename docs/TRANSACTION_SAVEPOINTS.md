# Transaction Savepoints

This document describes the transaction savepoint mechanism implemented in the JSON database.

## Overview

Savepoints provide a way to create named points within a transaction that you can later roll back to without aborting the entire transaction. This allows for more granular control over transaction operations and enables partial rollbacks.

## Key Features

- Create named savepoints within active transactions
- Roll back to a specific savepoint, undoing all operations after the savepoint
- Release savepoints that are no longer needed
- Support for multiple savepoints within a single transaction

## API Usage

### Creating a Savepoint

```http
POST /api/transactions/:id/savepoint
Content-Type: application/json

{
  "savepoint_name": "my_savepoint"
}
```

**Response:**

```json
{
  "transaction_id": "txn-12345",
  "savepoint_name": "my_savepoint",
  "status": "success",
  "message": "Savepoint created successfully"
}
```

### Rolling Back to a Savepoint

```http
POST /api/transactions/:id/savepoint/rollback
Content-Type: application/json

{
  "savepoint_name": "my_savepoint"
}
```

**Response:**

```json
{
  "transaction_id": "txn-12345",
  "savepoint_name": "my_savepoint",
  "status": "success",
  "message": "Successfully rolled back to savepoint"
}
```

### Releasing a Savepoint

```http
DELETE /api/transactions/:id/savepoint/release
Content-Type: application/json

{
  "savepoint_name": "my_savepoint"
}
```

**Response:**

```json
{
  "transaction_id": "txn-12345",
  "savepoint_name": "my_savepoint",
  "status": "success",
  "message": "Savepoint released successfully"
}
```

## Example Usage Scenarios

### Complex Multi-step Operations

Savepoints are particularly useful for transactions that involve multiple related but independent operations. For example, when importing a large dataset, you might create a savepoint after each batch is processed, allowing you to roll back to the last successful batch if an error occurs.

```javascript
// Start a transaction
const txId = await db.beginTransaction({ isolation_level: "serializable" });

try {
  // Process first batch
  await db.insertMany(txId, "users", batch1);
  await db.createSavepoint(txId, "batch1_complete");
  
  // Process second batch
  await db.insertMany(txId, "users", batch2);
  await db.createSavepoint(txId, "batch2_complete");
  
  // Process third batch
  try {
    await db.insertMany(txId, "users", batch3);
  } catch (error) {
    // If batch 3 fails, roll back to batch 2
    await db.rollbackToSavepoint(txId, "batch2_complete");
    console.log("Batch 3 failed, rolled back to batch 2");
  }
  
  // Commit the transaction
  await db.commitTransaction(txId);
} catch (error) {
  // If any other error occurs, roll back the entire transaction
  await db.rollbackTransaction(txId);
}
```

### Error Handling in Complex Operations

Savepoints enable more robust error handling by allowing parts of a transaction to be retried without starting over from scratch.

```javascript
const txId = await db.beginTransaction();

try {
  // Step 1: Create user
  const userId = await db.insert(txId, "users", { name: "John", email: "john@example.com" });
  await db.createSavepoint(txId, "user_created");
  
  // Step 2: Create user preferences
  try {
    await db.insert(txId, "preferences", { user_id: userId, theme: "dark" });
  } catch (error) {
    // If preferences creation fails, roll back to user creation
    await db.rollbackToSavepoint(txId, "user_created");
    
    // Try with default preferences instead
    await db.insert(txId, "preferences", { user_id: userId, theme: "default" });
  }
  
  // Commit the transaction
  await db.commitTransaction(txId);
} catch (error) {
  await db.rollbackTransaction(txId);
}
```

## Implementation Details

Savepoints are implemented by maintaining a linked list of savepoint structures within each transaction. Each savepoint stores:

1. A name for the savepoint
2. A pointer to the operation that was current when the savepoint was created
3. The operation count at the time the savepoint was created

When rolling back to a savepoint, the system undoes each operation after the savepoint in reverse order, then frees those operations from the transaction's operation list.

## Limitations

- Savepoints are only valid within the transaction that created them
- Savepoints are not persisted to disk and will be lost if the server crashes
- Creating a savepoint with the same name as an existing savepoint will update the existing savepoint
- Releasing a savepoint does not invalidate savepoints created after it, but rolling back will

## Best Practices

1. **Create savepoints at logical boundaries** in your transaction operations, particularly before operations that might fail.
2. **Use descriptive names** for savepoints to make your code more readable and maintainable.
3. **Release savepoints** that are no longer needed to free memory.
4. **Don't create too many savepoints** within a single transaction, as this consumes memory and can impact performance.
5. **Consider your isolation level** when creating savepoints, as higher isolation levels may lock resources for longer periods.