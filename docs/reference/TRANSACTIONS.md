# Transaction System Design

This document outlines the design for the transaction system in the JSON Database Server.

## Overview

The transaction system allows for atomic operations across multiple documents and collections. It ensures that either all operations within a transaction succeed or none of them do, maintaining data consistency even in the face of errors or system failures.

## Transaction Model

We'll implement a simplified ACID transaction model:

- **Atomicity**: All operations in a transaction either complete successfully or have no effect
- **Consistency**: The database remains in a valid state before and after the transaction
- **Isolation**: Concurrent transactions do not interfere with each other
- **Durability**: Once committed, transaction changes are permanent

## Components

### 1. Transaction Manager

The transaction manager is responsible for:
- Creating and tracking transactions
- Managing transaction state
- Coordinating commit and rollback operations
- Ensuring isolation between transactions

### 2. Transaction Log

The transaction log records:
- All operations performed within a transaction
- Before and after states of modified documents
- Transaction metadata (ID, timestamp, user, etc.)

This log is used for recovery in case of system failure and for rolling back transactions.

### 3. Lock Manager

The lock manager handles:
- Document-level locks to prevent concurrent modifications
- Collection-level locks for schema changes
- Deadlock detection and resolution

## Data Structures

### Transaction

```c
typedef enum {
    TRANSACTION_ACTIVE,
    TRANSACTION_COMMITTING,
    TRANSACTION_COMMITTED,
    TRANSACTION_ABORTING,
    TRANSACTION_ABORTED
} transaction_state_t;

typedef struct transaction_operation {
    operation_type_t type;           /* Type of operation (insert, update, delete) */
    char* collection_name;           /* Collection name */
    char* document_id;               /* Document ID (if applicable) */
    json_value_t* before_state;      /* Document state before operation */
    json_value_t* after_state;       /* Document state after operation */
    struct transaction_operation* next; /* Next operation in list */
} transaction_operation_t;

typedef struct transaction {
    char* id;                        /* Transaction ID */
    transaction_state_t state;       /* Current state */
    time_t start_time;               /* Start timestamp */
    time_t commit_time;              /* Commit timestamp */
    char* user_id;                   /* User who initiated the transaction */
    transaction_operation_t* operations; /* List of operations */
    int operation_count;             /* Number of operations */
    isolation_level_t isolation_level; /* Isolation level */
    pthread_mutex_t lock;            /* Transaction lock */
} transaction_t;
```

### Transaction Manager

```c
typedef struct {
    transaction_t** active_transactions; /* Array of active transactions */
    int capacity;                    /* Maximum number of concurrent transactions */
    int count;                       /* Current number of active transactions */
    pthread_mutex_t lock;            /* Manager lock */
    transaction_log_t* log;          /* Transaction log */
    lock_manager_t* lock_manager;    /* Lock manager */
} transaction_manager_t;
```

## Isolation Levels

We'll support three isolation levels:

1. **Read Uncommitted**: Allows transactions to see uncommitted changes from other transactions
2. **Read Committed**: Only allows transactions to see committed changes from other transactions
3. **Serializable**: Provides complete isolation between transactions

## API

### Transaction Management

```c
/* Start a new transaction */
transaction_t* transaction_begin(transaction_manager_t* manager, isolation_level_t isolation_level);

/* Commit a transaction */
int transaction_commit(transaction_manager_t* manager, transaction_t* transaction);

/* Rollback a transaction */
int transaction_rollback(transaction_manager_t* manager, transaction_t* transaction);
```

### Document Operations within Transactions

```c
/* Insert a document within a transaction */
int transaction_insert_document(transaction_t* transaction, const char* collection, json_value_t* document);

/* Update a document within a transaction */
int transaction_update_document(transaction_t* transaction, const char* collection, const char* id, json_value_t* document);

/* Delete a document within a transaction */
int transaction_delete_document(transaction_t* transaction, const char* collection, const char* id);

/* Query documents within a transaction */
json_value_t* transaction_query_documents(transaction_t* transaction, const char* collection, json_value_t* query);
```

## Transaction Workflow

1. **Begin Transaction**:
   - Create a new transaction with a unique ID
   - Set the isolation level
   - Initialize the transaction state to ACTIVE

2. **Perform Operations**:
   - For each operation, acquire necessary locks based on isolation level
   - Record the operation in the transaction operations list
   - Record before and after states for rollback purposes
   - Apply the operation to the database

3. **Commit Transaction**:
   - Change transaction state to COMMITTING
   - Ensure all operations were successful
   - Write transaction to the log
   - Make changes permanent
   - Release all locks
   - Change transaction state to COMMITTED

4. **Rollback Transaction**:
   - Change transaction state to ABORTING
   - Reverse all operations in reverse order
   - Release all locks
   - Change transaction state to ABORTED

## Recovery Process

1. On system startup, check for incomplete transactions in the log
2. For each transaction in COMMITTING state, complete the commit
3. For each transaction in ACTIVE or ABORTING state, perform rollback

## REST API

We'll expose the transaction functionality through the following REST endpoints:

### Begin Transaction

- **URL**: `/api/transactions`
- **Method**: `POST`
- **Auth Required**: Yes
- **Request Body**:
  ```json
  {
    "isolation_level": "serializable"
  }
  ```
- **Success Response**: `201 Created`
  ```json
  {
    "transaction_id": "string",
    "status": "active",
    "start_time": "string"
  }
  ```

### Commit Transaction

- **URL**: `/api/transactions/:id/commit`
- **Method**: `POST`
- **Auth Required**: Yes
- **URL Parameters**: `id=[string]` - Transaction ID
- **Success Response**: `200 OK`
  ```json
  {
    "transaction_id": "string",
    "status": "committed",
    "commit_time": "string"
  }
  ```

### Rollback Transaction

- **URL**: `/api/transactions/:id/rollback`
- **Method**: `POST`
- **Auth Required**: Yes
- **URL Parameters**: `id=[string]` - Transaction ID
- **Success Response**: `200 OK`
  ```json
  {
    "transaction_id": "string",
    "status": "aborted",
    "abort_time": "string"
  }
  ```

### Document Operations in Transaction

- **URL**: `/api/transactions/:id/collections/:collection/documents`
- **Method**: `POST` (insert), `PUT` (update), `DELETE` (delete), `GET` (query)
- **Auth Required**: Yes
- **URL Parameters**:
  - `id=[string]` - Transaction ID
  - `collection=[string]` - Collection name
- **Request Body**: Same as regular document operations
- **Success Response**: Same as regular document operations

## Limitations and Future Improvements

1. Initial implementation will focus on basic ACID guarantees rather than performance
2. Distributed transactions across multiple database nodes will be a future enhancement
3. Performance optimizations like row-level locking will be added in future versions

## Implementation Phases

1. **Phase 1**: Basic transaction support with document-level locking
2. **Phase 2**: Add transaction log and recovery support
3. **Phase 3**: Implement different isolation levels
4. **Phase 4**: Optimize performance with row-level locking
5. **Phase 5**: Distributed transactions