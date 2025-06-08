#include "transaction/transaction.h"
#include "utils/json_helpers.h"
#include "utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <limits.h>
#include <errno.h>

/* Transaction isolation levels */
static const char* isolation_level_names[] = {
  "read_uncommitted",
  "read_committed",
  "serializable",
  NULL
};

/* Transaction states */
static const char* transaction_state_names[] = {
  "active",
  "committing",
  "committed",
  "aborting",
  "aborted",
  NULL
};

/* Transaction operation types */
static const char* transaction_operation_names[] = {
  "insert",
  "update",
  "delete",
  NULL
};

/* ISOLATION_INVALID is now defined in the enum */

#ifndef TRANSACTION_STATE_INVALID
#define TRANSACTION_STATE_INVALID (-1)
#endif

#ifndef TRANSACTION_OP_INVALID
#define TRANSACTION_OP_INVALID (-1)
#endif

/* Convert isolation level to string */
const char* isolation_level_to_string(isolation_level_t level) {
  if (level < 0 || level >= ISOLATION_SERIALIZABLE + 1) {
    return "unknown";
  }
  return isolation_level_names[level];
}

/* Parse isolation level from string */
isolation_level_t isolation_level_from_string(const char* level_str) {
  if (!level_str) {
    return ISOLATION_INVALID;
  }

  for (int i = 0; isolation_level_names[i] != NULL; i++) {
    if (strcasecmp(level_str, isolation_level_names[i]) == 0) {
      return (isolation_level_t)i;
    }
  }

  return ISOLATION_INVALID;
}

/* Convert transaction state to string */
const char* transaction_state_to_string(transaction_state_t state) {
  if (state < 0 || state >= 5) { // Using the count instead of enum constant
    return "unknown";
  }
  return transaction_state_names[state];
}

/* Parse transaction state from string */
transaction_state_t transaction_state_from_string(const char* state_str) {
  if (!state_str) {
    return TRANSACTION_STATE_INVALID;
  }

  for (int i = 0; transaction_state_names[i] != NULL; i++) {
    if (strcasecmp(state_str, transaction_state_names[i]) == 0) {
      return (transaction_state_t)i;
    }
  }

  return TRANSACTION_STATE_INVALID;
}

/* Convert transaction operation type to string */
const char* transaction_operation_to_string(operation_type_t operation) {
  if (operation < 0 || operation >= 3) { // Using the count instead of enum constant
    return "unknown";
  }
  return transaction_operation_names[operation];
}

/* Parse transaction operation type from string */
operation_type_t transaction_operation_from_string(const char* operation_str) {
  if (!operation_str) {
    return TRANSACTION_OP_INVALID;
  }

  for (int i = 0; transaction_operation_names[i] != NULL; i++) {
    if (strcasecmp(operation_str, transaction_operation_names[i]) == 0) {
      return (operation_type_t)i;
    }
  }

  return TRANSACTION_OP_INVALID;
}

/* Transaction error codes to string */
const char* transaction_error_to_string(int error_code) {
  switch (error_code) {
    case 0: return "Success";
    case -1: return "Invalid transaction";
    case -2: return "Transaction already committed";
    case -3: return "Transaction already rolled back";
    case -4: return "Transaction failed";
    case -5: return "Deadlock detected";
    case -6: return "Lock timeout";
    case -7: return "Invalid isolation level";
    case -8: return "Invalid savepoint";
    case -9: return "Savepoint not found";
    case -10: return "Document not found";
    case -11: return "Collection not found";
    case -12: return "Invalid document";
    case -13: return "Conflict with another transaction";
    default: return "Unknown error";
  }
}

/* Generate a transaction ID */
static char* generate_transaction_id() {
  /* Format: tx_<timestamp>_<random> */
  char* id = (char*)malloc(40);
  if (!id) {
    return NULL;
  }

  time_t now = time(NULL);
  unsigned int random_part = rand() % 1000000;

  snprintf(id, 40, "tx_%ld_%06u", now, random_part);

  return id;
}

/* Hash function for transaction IDs */
static unsigned int hash_transaction_id(const char* id, int table_size) {
  if (!id) return 0;

  unsigned int hash = 0;
  while (*id) {
    hash = (hash * 31) + *id++;
  }
  return hash % table_size;
}

/* Add a transaction to the hash table */
static int transaction_hash_table_add(transaction_manager_t* manager, transaction_t* transaction) {
  if (!manager || !transaction || !transaction->id || !manager->tx_hash_table) {
    return 0;
  }

  /* Calculate hash value */
  unsigned int hash = hash_transaction_id(transaction->id, manager->tx_hash_size);

  /* Create a new hash entry */
  transaction_hash_entry_t* entry = (transaction_hash_entry_t*)malloc(sizeof(transaction_hash_entry_t));
  if (!entry) {
    return 0;
  }

  /* Initialize entry */
  entry->transaction = transaction;

  /* Add to the hash chain */
  entry->next = manager->tx_hash_table[hash];
  manager->tx_hash_table[hash] = entry;

  return 1;
}

/* Remove a transaction from the hash table */
static int transaction_hash_table_remove(transaction_manager_t* manager, const char* id) {
  if (!manager || !id || !manager->tx_hash_table) {
    return 0;
  }

  /* Calculate hash value */
  unsigned int hash = hash_transaction_id(id, manager->tx_hash_size);

  /* Find the entry */
  transaction_hash_entry_t** prev = &manager->tx_hash_table[hash];
  transaction_hash_entry_t* entry = *prev;

  while (entry) {
    if (entry->transaction && entry->transaction->id &&
      strcmp(entry->transaction->id, id) == 0) {
      /* Remove the entry from the chain */
      *prev = entry->next;
      free(entry);
      return 1;
    }

    prev = &entry->next;
    entry = entry->next;
  }

  return 0;
}

/* Find a transaction in the hash table */
static transaction_t* transaction_hash_table_find(transaction_manager_t* manager, const char* id) {
  if (!manager || !id || !manager->tx_hash_table) {
    return NULL;
  }

  /* Calculate hash value */
  unsigned int hash = hash_transaction_id(id, manager->tx_hash_size);

  /* Find the entry */
  transaction_hash_entry_t* entry = manager->tx_hash_table[hash];

  while (entry) {
    if (entry->transaction && entry->transaction->id &&
      strcmp(entry->transaction->id, id) == 0) {
      return entry->transaction;
    }

    entry = entry->next;
  }

  return NULL;
}

/* Free the hash table */
static void transaction_hash_table_free(transaction_manager_t* manager) {
  if (!manager || !manager->tx_hash_table) {
    return;
  }

  /* Free all entries in the hash table */
  for (int i = 0; i < manager->tx_hash_size; i++) {
    transaction_hash_entry_t* entry = manager->tx_hash_table[i];

    while (entry) {
      transaction_hash_entry_t* next = entry->next;
      free(entry);
      entry = next;
    }
  }

  /* Free the hash table array */
  free(manager->tx_hash_table);
  manager->tx_hash_table = NULL;
}

/* Create a transaction manager */
transaction_manager_t* transaction_manager_create(database_t* db, int capacity) {
  LOG_INFO("Creating transaction manager with capacity %d", capacity);

  transaction_manager_t* manager = (transaction_manager_t*)malloc(sizeof(transaction_manager_t));
  if (!manager) {
    LOG_ERROR("Out of memory");
    return NULL;
  }

  /* Initialize fields */
  LOG_DEBUG("Initializing transaction manager fields");
  manager->active_transactions = NULL;
  manager->count = 0;
  manager->capacity = capacity > 0 ? capacity : 100; // Default capacity
  manager->db = db;
  manager->lock_manager = NULL; /* Initialize lock manager as needed */
  manager->log = NULL; /* Initialize log as needed */

  if (pthread_mutex_init(&manager->lock, NULL) != 0) {
    LOG_ERROR("initialize transaction manager mutex");
    free(manager);
    return NULL;
  }

  /* Allocate transactions array */
  LOG_DEBUG("Allocating array for %d active transactions", manager->capacity);
  manager->active_transactions = (transaction_t**)malloc(manager->capacity * sizeof(transaction_t*));
  if (!manager->active_transactions) {
    LOG_ERROR("Out of memory");
    pthread_mutex_destroy(&manager->lock);
    free(manager);
    return NULL;
  }

  /* Initialize hash table for O(1) transaction lookup */
  manager->tx_hash_size = manager->capacity * 2; /* Size hash table 2x capacity for good distribution */
  LOG_DEBUG("Initializing transaction hash table with %d buckets", manager->tx_hash_size);

  manager->tx_hash_table = (transaction_hash_entry_t**)calloc(
    manager->tx_hash_size, sizeof(transaction_hash_entry_t*));

  if (!manager->tx_hash_table) {
    LOG_ERROR("Out of memory");
    free(manager->active_transactions);
    pthread_mutex_destroy(&manager->lock);
    free(manager);
    return NULL;
  }

  LOG_INFO("Transaction manager created with capacity for %d transactions",
      manager->capacity);
  return manager;
}

/* Free a transaction manager */
void transaction_manager_free(transaction_manager_t* manager) {
  if (!manager) {
    LOG_WARNING("Attempted to free NULL transaction manager");
    return;
  }

  LOG_INFO("Freeing transaction manager with %d active transactions", manager->count);

  /* Free active transactions */
  if (manager->active_transactions) {
    LOG_DEBUG("Freeing active transactions");

    for (int i = 0; i < manager->count; i++) {
      if (manager->active_transactions[i]) {
        LOG_TRACE("Freeing transaction %s (state: %d)",
             manager->active_transactions[i]->id ?
             manager->active_transactions[i]->id : "unknown",
             manager->active_transactions[i]->state);

        if (manager->active_transactions[i]->id) {
          free(manager->active_transactions[i]->id);
        }
        if (manager->active_transactions[i]->user_id) {
          free(manager->active_transactions[i]->user_id);
        }

        /* Free operation list */
        transaction_operation_t* operation = manager->active_transactions[i]->operations;
        int op_count = 0;

        while (operation) {
          transaction_operation_t* next = operation->next;
          op_count++;

          if (operation->collection_name) {
            free(operation->collection_name);
          }

          if (operation->document_id) {
            free(operation->document_id);
          }

          if (operation->before_state) {
            json_free(operation->before_state);
          }

          if (operation->after_state) {
            json_free(operation->after_state);
          }

          free(operation);
          operation = next;
        }

        LOG_TRACE("Freed %d operations for transaction %s",
             op_count,
             manager->active_transactions[i]->id ?
             manager->active_transactions[i]->id : "unknown");

        /* Free savepoint list */
        savepoint_t* savepoint = manager->active_transactions[i]->savepoints;
        int sp_count = 0;

        while (savepoint) {
          savepoint_t* next = savepoint->next;
          sp_count++;

          if (savepoint->name) {
            free(savepoint->name);
          }

          free(savepoint);
          savepoint = next;
        }

        LOG_TRACE("Freed %d savepoints for transaction %s",
             sp_count,
             manager->active_transactions[i]->id ?
             manager->active_transactions[i]->id : "unknown");

        pthread_mutex_destroy(&manager->active_transactions[i]->lock);
        free(manager->active_transactions[i]);
      }
    }

    LOG_DEBUG("Freeing transaction array");
    free(manager->active_transactions);
  }

  /* Free hash table */
  LOG_DEBUG("Freeing transaction hash table");
  transaction_hash_table_free(manager);

  /* Free lock manager if present */
  if (manager->lock_manager) {
    LOG_DEBUG("Lock manager present but free function is commented out");
    /* lock_manager_free(manager->lock_manager); */
  }

  /* Free log if present */
  if (manager->log) {
    LOG_DEBUG("Transaction log present but free function is commented out");
    /* transaction_log_free(manager->log); */
  }

  LOG_DEBUG("Destroying transaction manager mutex");
  pthread_mutex_destroy(&manager->lock);

  LOG_INFO("Transaction manager freed");
  free(manager);
}

/* Find a transaction by ID */
transaction_t* transaction_manager_get_transaction(transaction_manager_t* manager, const char* id) {
  if (!manager || !id) {
    return NULL;
  }

  pthread_mutex_lock(&manager->lock);

  transaction_t* transaction = NULL;

  /* Use hash table for O(1) lookup if available */
  if (manager->tx_hash_table) {
    transaction = transaction_hash_table_find(manager, id);
  } else {
    /* Fallback to linear search if hash table is not available */
    for (int i = 0; i < manager->count; i++) {
      if (manager->active_transactions[i] &&
        strcmp(manager->active_transactions[i]->id, id) == 0) {
        transaction = manager->active_transactions[i];
        break;
      }
    }
  }

  pthread_mutex_unlock(&manager->lock);

  return transaction;
}

/* Convert a transaction to JSON */
json_value_t* transaction_to_json(transaction_t* transaction) {
  if (!transaction) {
    return NULL;
  }

  json_value_t* json = json_create_object();
  if (!json) {
    return NULL;
  }

  /* Add transaction fields */
  json_object_set(json, "id", json_create_string(transaction->id));
  json_object_set(json, "state", json_create_string(transaction_state_to_string(transaction->state)));
  json_object_set(json, "isolation_level", json_create_string(isolation_level_to_string(transaction->isolation_level)));
  json_object_set(json, "start_time", json_create_integer(transaction->start_time));

  if (transaction->state == TRANSACTION_COMMITTED) {
    json_object_set(json, "commit_time", json_create_integer(transaction->commit_time));
  } else if (transaction->state == TRANSACTION_ABORTED) {
    /* Using commit_time as equivalent to rollback_time */
    json_object_set(json, "rollback_time", json_create_integer(transaction->commit_time));
  }

  return json;
}

/* Start a new transaction */
transaction_t* transaction_begin(transaction_manager_t* manager, isolation_level_t isolation_level, const char* user_id) {
  if (!manager || !user_id) {
    LOG_ERROR("begin transaction: Invalid parameters (manager: %p, user_id: %s)",
         manager, user_id ? user_id : "NULL");
    return NULL;
  }

  LOG_INFO("Beginning transaction for user '%s' with isolation level %d",
      user_id, isolation_level);

  /* Validate isolation level */
  if (isolation_level < ISOLATION_READ_UNCOMMITTED || isolation_level > ISOLATION_SERIALIZABLE) {
    LOG_WARNING("Invalid isolation level %d, defaulting to READ COMMITTED", isolation_level);
    isolation_level = ISOLATION_READ_COMMITTED; /* Default to READ COMMITTED */
  }

  /* Create a new transaction */
  LOG_DEBUG("Allocating memory for transaction structure");
  transaction_t* transaction = (transaction_t*)malloc(sizeof(transaction_t));
  if (!transaction) {
    LOG_ERROR("Out of memory");
    return NULL;
  }

  /* Generate a transaction ID */
  LOG_DEBUG("Generating transaction ID");
  transaction->id = generate_transaction_id();
  if (!transaction->id) {
    LOG_ERROR("generate transaction ID");
    free(transaction);
    return NULL;
  }

  /* Copy user ID */
  LOG_DEBUG("Setting transaction user ID to '%s'", user_id);
  transaction->user_id = strdup(user_id);
  if (!transaction->user_id) {
    LOG_ERROR("Out of memory");
    free(transaction->id);
    free(transaction);
    return NULL;
  }

  /* Initialize transaction fields */
  LOG_DEBUG("Initializing transaction fields");
  transaction->state = TRANSACTION_ACTIVE;
  transaction->isolation_level = isolation_level;
  transaction->start_time = time(NULL);
  transaction->commit_time = 0;
  transaction->timeout_sec = 30; /* Default timeout */
  transaction->operations = NULL;
  transaction->operation_count = 0;
  transaction->savepoints = NULL;

  if (pthread_mutex_init(&transaction->lock, NULL) != 0) {
    LOG_ERROR("initialize transaction mutex");
    free(transaction->user_id);
    free(transaction->id);
    free(transaction);
    return NULL;
  }

  LOG_DEBUG("Transaction %s created", transaction->id);

  /* Add to manager */
  LOG_DEBUG("Adding transaction %s to manager", transaction->id);
  pthread_mutex_lock(&manager->lock);

  /* Check if we need to resize the transactions array */
  if (manager->count >= manager->capacity) {
    /* Resize the array to double the capacity */
    int new_capacity = manager->capacity * 2;
    LOG_INFO("Resizing transaction array from %d to %d elements",
        manager->capacity, new_capacity);

    transaction_t** new_array = (transaction_t**)realloc(
      manager->active_transactions, new_capacity * sizeof(transaction_t*));

    if (!new_array) {
      /* Reallocation failed */
      LOG_ERROR("resize transaction array to %d elements", new_capacity);
      pthread_mutex_unlock(&manager->lock);
      free(transaction->user_id);
      free(transaction->id);
      free(transaction);
      return NULL;
    }

    /* Resize the hash table to maintain good performance */
    int new_hash_size = new_capacity * 2;
    LOG_INFO("Resizing transaction hash table from %d to %d buckets",
        manager->tx_hash_size, new_hash_size);

    transaction_hash_entry_t** new_hash_table = (transaction_hash_entry_t**)
      calloc(new_hash_size, sizeof(transaction_hash_entry_t*));

    if (!new_hash_table) {
      /* Hash table resize failed - continue with the existing hash table */
      LOG_WARNING("Failed to resize transaction hash table, continuing with existing table");
    } else {
      /* Rebuild the hash table with the new size */
      LOG_DEBUG("Rebuilding transaction hash table with new size");
      int rehashed_entries = 0;

      for (int i = 0; i < manager->tx_hash_size; i++) {
        transaction_hash_entry_t* entry = manager->tx_hash_table[i];
        while (entry) {
          transaction_hash_entry_t* next = entry->next;
          rehashed_entries++;

          /* Calculate new hash value */
          unsigned int hash = hash_transaction_id(
            entry->transaction->id, new_hash_size);

          /* Add to new hash table */
          entry->next = new_hash_table[hash];
          new_hash_table[hash] = entry;

          entry = next;
        }
      }

      /* Replace the old hash table */
      LOG_DEBUG("Rehashed %d entries to new transaction hash table", rehashed_entries);
      free(manager->tx_hash_table);
      manager->tx_hash_table = new_hash_table;
      manager->tx_hash_size = new_hash_size;
    }

    /* Update the transaction array */
    manager->active_transactions = new_array;
    manager->capacity = new_capacity;
    LOG_INFO("Transaction array and hash table resized");
  }

  /* Add transaction to the list */
  LOG_DEBUG("Adding transaction %s as transaction #%d",
       transaction->id, manager->count);
  manager->active_transactions[manager->count++] = transaction;

  /* Add transaction to hash table for O(1) lookup */
  if (manager->tx_hash_table) {
    LOG_DEBUG("Adding transaction %s to hash table", transaction->id);
    if (!transaction_hash_table_add(manager, transaction)) {
      /* Failed to add to hash table, but continue anyway with degraded performance */
      LOG_WARNING("Failed to add transaction %s to hash table - lookups will be slower",
           transaction->id);
    }
  }

  pthread_mutex_unlock(&manager->lock);

  LOG_INFO("Transaction %s started (isolation: %d, user: %s)",
      transaction->id, transaction->isolation_level, transaction->user_id);
  return transaction;
}

/* Commit a transaction */
int transaction_commit(transaction_manager_t* manager, transaction_t* transaction) {
  if (!manager || !transaction) {
    LOG_ERROR("commit transaction: Invalid parameters (manager: %p, transaction: %p)",
         manager, transaction);
    return -1; /* Invalid transaction */
  }

  LOG_INFO("Committing transaction %s", transaction->id);
  pthread_mutex_lock(&transaction->lock);

  /* Check if transaction is already committed or rolled back */
  if (transaction->state == TRANSACTION_COMMITTED) {
    LOG_WARNING("Transaction %s is already committed", transaction->id);
    pthread_mutex_unlock(&transaction->lock);
    return -2; /* Already committed */
  }

  if (transaction->state == TRANSACTION_ABORTED) {
    LOG_WARNING("Transaction %s is already aborted, cannot commit", transaction->id);
    pthread_mutex_unlock(&transaction->lock);
    return -3; /* Already rolled back */
  }

  /* Set transaction to committing state */
  LOG_DEBUG("Changing transaction %s state to COMMITTING", transaction->id);
  transaction->state = TRANSACTION_COMMITTING;

  /* Commit all operations */
  LOG_DEBUG("Committing %d operations for transaction %s",
       transaction->operation_count, transaction->id);
  /* This would involve applying the operations to the database */
  /* For each operation in transaction->operations... */

  /* Update transaction state */
  transaction->state = TRANSACTION_COMMITTED;
  transaction->commit_time = time(NULL);

  LOG_DEBUG("Transaction %s committed at %ld", transaction->id, transaction->commit_time);

  pthread_mutex_unlock(&transaction->lock);

  /* Log the transaction if a log is available */
  if (manager->log) {
    LOG_DEBUG("Writing transaction %s commit to log", transaction->id);
    transaction_log_write_state_change(manager->log, transaction);
  } else {
    LOG_TRACE("No transaction log available, skipping log entry");
  }

  /* Remove from hash table if available - no need to keep committed transactions in fast lookup */
  if (manager->tx_hash_table) {
    LOG_DEBUG("Removing committed transaction %s from hash table", transaction->id);
    pthread_mutex_lock(&manager->lock);
    transaction_hash_table_remove(manager, transaction->id);
    pthread_mutex_unlock(&manager->lock);
  }

  LOG_INFO("Transaction %s committed", transaction->id);
  return 0; /* Success */
}

/* Roll back a transaction */
int transaction_rollback(transaction_manager_t* manager, transaction_t* transaction) {
  if (!manager || !transaction) {
    LOG_ERROR("rollback transaction: Invalid parameters (manager: %p, transaction: %p)",
         manager, transaction);
    return -1; /* Invalid transaction */
  }

  LOG_INFO("Rolling back transaction %s", transaction->id);
  pthread_mutex_lock(&transaction->lock);

  /* Check if transaction is already committed or rolled back */
  if (transaction->state == TRANSACTION_COMMITTED) {
    LOG_WARNING("Transaction %s is already committed, cannot rollback", transaction->id);
    pthread_mutex_unlock(&transaction->lock);
    return -2; /* Already committed */
  }

  if (transaction->state == TRANSACTION_ABORTED) {
    LOG_WARNING("Transaction %s is already aborted", transaction->id);
    pthread_mutex_unlock(&transaction->lock);
    return -3; /* Already rolled back */
  }

  /* Set transaction to aborting state */
  LOG_DEBUG("Changing transaction %s state to ABORTING", transaction->id);
  transaction->state = TRANSACTION_ABORTING;

  /* Roll back all operations */
  LOG_DEBUG("Rolling back %d operations for transaction %s",
       transaction->operation_count, transaction->id);
  /* This would involve undoing the operations */
  /* For each operation in transaction->operations (in reverse order)... */

  /* Update transaction state */
  transaction->state = TRANSACTION_ABORTED;
  transaction->commit_time = time(NULL); /* Using commit_time to store rollback time */

  LOG_DEBUG("Transaction %s aborted at %ld", transaction->id, transaction->commit_time);

  pthread_mutex_unlock(&transaction->lock);

  /* Log the transaction if a log is available */
  if (manager->log) {
    LOG_DEBUG("Writing transaction %s rollback to log", transaction->id);
    transaction_log_write_state_change(manager->log, transaction);
  } else {
    LOG_TRACE("No transaction log available, skipping log entry");
  }

  /* Remove from hash table if available - no need to keep aborted transactions in fast lookup */
  if (manager->tx_hash_table) {
    LOG_DEBUG("Removing aborted transaction %s from hash table", transaction->id);
    pthread_mutex_lock(&manager->lock);
    transaction_hash_table_remove(manager, transaction->id);
    pthread_mutex_unlock(&manager->lock);
  }

  LOG_INFO("Transaction %s rolled back", transaction->id);
  return 0; /* Success */
}

/* Set transaction timeout */
int transaction_set_timeout(transaction_t* transaction, int timeout_sec) {
  if (!transaction) {
    return -1; /* Invalid transaction */
  }

  if (timeout_sec < 0) {
    return -2; /* Invalid timeout */
  }

  pthread_mutex_lock(&transaction->lock);
  transaction->timeout_sec = timeout_sec;
  pthread_mutex_unlock(&transaction->lock);

  return 0; /* Success */
}

/* Create a savepoint within a transaction */
int transaction_create_savepoint(transaction_t* transaction, const char* savepoint_name) {
  if (!transaction || !savepoint_name) {
    return -1; /* Invalid transaction or name */
  }

  pthread_mutex_lock(&transaction->lock);

  /* Check if transaction is active */
  if (transaction->state != TRANSACTION_ACTIVE) {
    pthread_mutex_unlock(&transaction->lock);
    return -1; /* Transaction not active */
  }

  /* Create a new savepoint */
  savepoint_t* savepoint = (savepoint_t*)malloc(sizeof(savepoint_t));
  if (!savepoint) {
    pthread_mutex_unlock(&transaction->lock);
    return -1; /* Memory allocation failed */
  }

  /* Copy the name */
  savepoint->name = strdup(savepoint_name);
  if (!savepoint->name) {
    free(savepoint);
    pthread_mutex_unlock(&transaction->lock);
    return -1; /* Memory allocation failed */
  }

  /* Set the operation index to current number of operations */
  savepoint->operation_count = transaction->operation_count;
  savepoint->operation = transaction->operations; /* Point to current operation list */
  savepoint->next = NULL;

  /* Add to transaction */
  if (!transaction->savepoints) {
    transaction->savepoints = savepoint;
  } else {
    savepoint->next = transaction->savepoints;
    transaction->savepoints = savepoint;
  }

  pthread_mutex_unlock(&transaction->lock);

  return 0; /* Success */
}

/* Rollback to a savepoint within a transaction */
int transaction_rollback_to_savepoint(transaction_manager_t* manager, transaction_t* transaction, const char* savepoint_name) {
  if (!manager || !transaction || !savepoint_name) {
    return -1; /* Invalid transaction or name */
  }

  pthread_mutex_lock(&transaction->lock);

  /* Check if transaction is active */
  if (transaction->state != TRANSACTION_ACTIVE) {
    pthread_mutex_unlock(&transaction->lock);
    return -1; /* Transaction not active */
  }

  /* Find the savepoint */
  savepoint_t* savepoint = transaction->savepoints;
  while (savepoint) {
    if (strcmp(savepoint->name, savepoint_name) == 0) {
      break;
    }
    savepoint = savepoint->next;
  }

  if (!savepoint) {
    pthread_mutex_unlock(&transaction->lock);
    return -9; /* Savepoint not found */
  }

  /* Roll back operations after the savepoint */
  /* This would involve undoing operations between transaction->operations and savepoint->operation */

  /* Reset operation list to savepoint state */
  transaction->operations = savepoint->operation;
  transaction->operation_count = savepoint->operation_count;

  pthread_mutex_unlock(&transaction->lock);

  return 0; /* Success */
}

/* Release a savepoint within a transaction */
int transaction_release_savepoint(transaction_t* transaction, const char* savepoint_name) {
  if (!transaction || !savepoint_name) {
    return -1; /* Invalid transaction or name */
  }

  pthread_mutex_lock(&transaction->lock);

  /* Check if transaction is active */
  if (transaction->state != TRANSACTION_ACTIVE) {
    pthread_mutex_unlock(&transaction->lock);
    return -1; /* Transaction not active */
  }

  /* Find the savepoint */
  savepoint_t* savepoint = transaction->savepoints;
  savepoint_t* prev = NULL;

  while (savepoint) {
    if (strcmp(savepoint->name, savepoint_name) == 0) {
      break;
    }
    prev = savepoint;
    savepoint = savepoint->next;
  }

  if (!savepoint) {
    pthread_mutex_unlock(&transaction->lock);
    return -9; /* Savepoint not found */
  }

  /* Remove from the list */
  if (prev) {
    prev->next = savepoint->next;
  } else {
    transaction->savepoints = savepoint->next;
  }

  /* Free the savepoint */
  free(savepoint->name);
  free(savepoint);

  pthread_mutex_unlock(&transaction->lock);

  return 0; /* Success */
}

/* Check for deadlocks in the transaction manager */
int transaction_manager_check_deadlocks(transaction_manager_t* manager) {
  if (!manager) {
    return -1;
  }

  pthread_mutex_lock(&manager->lock);

  /* Deadlock detection algorithm would go here */
  /* For example, build a wait-for graph and check for cycles */

  pthread_mutex_unlock(&manager->lock);

  return 0; /* No deadlocks found */
}

/* Get transaction manager metrics */
json_value_t* transaction_manager_get_metrics(transaction_manager_t* manager) {
  if (!manager) {
    return NULL;
  }

  pthread_mutex_lock(&manager->lock);

  json_value_t* json = json_create_object();
  if (!json) {
    pthread_mutex_unlock(&manager->lock);
    return NULL;
  }

  /* Count transactions by state */
  int active_count = 0;
  int committed_count = 0;
  int aborted_count = 0;

  for (int i = 0; i < manager->count; i++) {
    if (manager->active_transactions[i]) {
      switch (manager->active_transactions[i]->state) {
        case TRANSACTION_ACTIVE:
        case TRANSACTION_COMMITTING:
        case TRANSACTION_ABORTING:
          active_count++;
          break;
        case TRANSACTION_COMMITTED:
          committed_count++;
          break;
        case TRANSACTION_ABORTED:
          aborted_count++;
          break;
      }
    }
  }

  /* Add metrics to JSON */
  json_object_set(json, "active_transactions", json_create_integer(active_count));
  json_object_set(json, "committed_transactions", json_create_integer(committed_count));
  json_object_set(json, "aborted_transactions", json_create_integer(aborted_count));
  json_object_set(json, "total_transactions", json_create_integer(manager->count));

  pthread_mutex_unlock(&manager->lock);

  return json;
}

/* Get transaction performance metrics */
json_value_t* transaction_get_performance_metrics(transaction_manager_t* manager,
                       const char* dimension, time_t start_time, time_t end_time) {
  if (!manager) {
    return NULL;
  }

  pthread_mutex_lock(&manager->lock);

  /* Create a metrics container */
  json_value_t* metrics = json_create_object();
  if (!metrics) {
    pthread_mutex_unlock(&manager->lock);
    return NULL;
  }

  /* Set dimension type */
  json_object_set(metrics, "dimension", json_create_string(dimension ? dimension : "performance"));

  /* Use time range if specified */
  if (start_time > 0) {
    json_object_set(metrics, "start_time", json_create_integer(start_time));
  }

  if (end_time > 0) {
    json_object_set(metrics, "end_time", json_create_integer(end_time));
  }

  /* Create overall metrics */
  json_value_t* overall = json_create_object();

  /* Initialize counters */
  int total_transactions = 0;
  int committed_transactions = 0;
  int aborted_transactions = 0;
  long long total_duration = 0;
  long long min_duration = LONG_MAX;
  long long max_duration = 0;

  /* Analyze transactions */
  for (int i = 0; i < manager->count; i++) {
    transaction_t* tx = manager->active_transactions[i];
    if (!tx) continue;

    /* Skip transactions outside time range if specified */
    if (start_time > 0 && tx->start_time < start_time) {
      continue;
    }

    if (end_time > 0 && tx->start_time > end_time) {
      continue;
    }

    total_transactions++;

    /* Calculate duration for completed transactions */
    if (tx->state == TRANSACTION_COMMITTED) {
      committed_transactions++;

      long long duration = tx->commit_time - tx->start_time;
      total_duration += duration;

      if (duration < min_duration) {
        min_duration = duration;
      }

      if (duration > max_duration) {
        max_duration = duration;
      }
    } else if (tx->state == TRANSACTION_ABORTED) {
      aborted_transactions++;

      /* Using commit_time as rollback time */
      long long duration = tx->commit_time - tx->start_time;
      total_duration += duration;

      if (duration < min_duration) {
        min_duration = duration;
      }

      if (duration > max_duration) {
        max_duration = duration;
      }
    }
  }

  /* Add metrics to JSON */
  json_object_set(overall, "total_transactions", json_create_integer(total_transactions));
  json_object_set(overall, "committed_transactions", json_create_integer(committed_transactions));
  json_object_set(overall, "aborted_transactions", json_create_integer(aborted_transactions));

  if (total_transactions > 0) {
    /* Add duration metrics if we have data */
    if (committed_transactions + aborted_transactions > 0) {
      /* Convert to appropriate time units */
      long long avg_duration = total_duration / (committed_transactions + aborted_transactions);

      /* Add to result */
      json_object_set(overall, "avg_duration_seconds", json_create_integer(avg_duration));

      if (min_duration != LONG_MAX) {
        json_object_set(overall, "min_duration_seconds", json_create_integer(min_duration));
      }

      if (max_duration > 0) {
        json_object_set(overall, "max_duration_seconds", json_create_integer(max_duration));
      }
    }

    /* Add commit rate */
    if (total_transactions > 0) {
      double commit_rate = (double)committed_transactions / total_transactions;
      char commit_rate_str[32];
      snprintf(commit_rate_str, sizeof(commit_rate_str), "%.2f", commit_rate);
      json_object_set(overall, "commit_rate", json_create_string(commit_rate_str));
    }
  }

  /* Add overall metrics to result */
  json_object_set(metrics, "overall", overall);

  pthread_mutex_unlock(&manager->lock);

  return metrics;
}

/* Insert a document within a transaction */
int transaction_insert_document(transaction_manager_t* manager, transaction_t* transaction,
               const char* collection, json_value_t* document) {
  if (!manager || !transaction || !collection || !document) {
    return -1;
  }

  pthread_mutex_lock(&transaction->lock);

  /* Check if transaction is active */
  if (transaction->state != TRANSACTION_ACTIVE) {
    pthread_mutex_unlock(&transaction->lock);
    return -1; /* Transaction not active */
  }

  /* Create new operation */
  transaction_operation_t* operation = (transaction_operation_t*)malloc(sizeof(transaction_operation_t));
  if (!operation) {
    pthread_mutex_unlock(&transaction->lock);
    return -1;
  }

  /* Initialize operation */
  operation->type = OPERATION_INSERT;
  operation->collection_name = strdup(collection);
  if (!operation->collection_name) {
    free(operation);
    pthread_mutex_unlock(&transaction->lock);
    return -1;
  }

  /* Get document ID, or generate one if not present */
  json_value_t* id_value = json_object_get(document, "uuid");
  if (id_value && id_value->type == JSON_STRING) {
    operation->document_id = strdup(id_value->value.string);
  } else {
    /* Generate ID */
    char* new_id = generate_transaction_id();
    operation->document_id = new_id;

    /* Add ID to document */
    json_object_set(document, "uuid", json_create_string(new_id));
  }

  if (!operation->document_id) {
    free(operation->collection_name);
    free(operation);
    pthread_mutex_unlock(&transaction->lock);
    return -1;
  }

  /* Clone document for operation */
  operation->after_state = json_clone(document);
  operation->before_state = NULL; /* No previous state for insert */

  /* Add to operation list */
  operation->next = transaction->operations;
  transaction->operations = operation;
  transaction->operation_count++;

  pthread_mutex_unlock(&transaction->lock);

  return 0; /* Success */
}

/* Update a document within a transaction */
int transaction_update_document(transaction_manager_t* manager, transaction_t* transaction,
                const char* collection, const char* id, json_value_t* document) {
  if (!manager || !transaction || !collection || !id || !document) {
    return -1;
  }

  pthread_mutex_lock(&transaction->lock);

  /* Check if transaction is active */
  if (transaction->state != TRANSACTION_ACTIVE) {
    pthread_mutex_unlock(&transaction->lock);
    return -1; /* Transaction not active */
  }

  /* Find the document in the database to get its current state */
  /* This would involve looking up the document in the database */
  /* Let's assume we have a before_document that's the current state */
  json_value_t* before_document = NULL; /* This would come from the database */

  /* Create new operation */
  transaction_operation_t* operation = (transaction_operation_t*)malloc(sizeof(transaction_operation_t));
  if (!operation) {
    pthread_mutex_unlock(&transaction->lock);
    return -1;
  }

  /* Initialize operation */
  operation->type = OPERATION_UPDATE;
  operation->collection_name = strdup(collection);
  if (!operation->collection_name) {
    free(operation);
    pthread_mutex_unlock(&transaction->lock);
    return -1;
  }

  operation->document_id = strdup(id);
  if (!operation->document_id) {
    free(operation->collection_name);
    free(operation);
    pthread_mutex_unlock(&transaction->lock);
    return -1;
  }

  /* Clone documents for operation */
  operation->after_state = json_clone(document);
  operation->before_state = before_document ? json_clone(before_document) : NULL;

  /* Add to operation list */
  operation->next = transaction->operations;
  transaction->operations = operation;
  transaction->operation_count++;

  pthread_mutex_unlock(&transaction->lock);

  return 0; /* Success */
}

/* Delete a document within a transaction */
int transaction_delete_document(transaction_manager_t* manager, transaction_t* transaction,
                const char* collection, const char* id) {
  if (!manager || !transaction || !collection || !id) {
    return -1;
  }

  pthread_mutex_lock(&transaction->lock);

  /* Check if transaction is active */
  if (transaction->state != TRANSACTION_ACTIVE) {
    pthread_mutex_unlock(&transaction->lock);
    return -1; /* Transaction not active */
  }

  /* Find the document in the database to get its current state before deletion */
  /* This would involve looking up the document in the database */
  /* Let's assume we have a before_document that's the current state */
  json_value_t* before_document = NULL; /* This would come from the database */

  /* Create new operation */
  transaction_operation_t* operation = (transaction_operation_t*)malloc(sizeof(transaction_operation_t));
  if (!operation) {
    pthread_mutex_unlock(&transaction->lock);
    return -1;
  }

  /* Initialize operation */
  operation->type = OPERATION_DELETE;
  operation->collection_name = strdup(collection);
  if (!operation->collection_name) {
    free(operation);
    pthread_mutex_unlock(&transaction->lock);
    return -1;
  }

  operation->document_id = strdup(id);
  if (!operation->document_id) {
    free(operation->collection_name);
    free(operation);
    pthread_mutex_unlock(&transaction->lock);
    return -1;
  }

  /* Store document state before deletion */
  operation->before_state = before_document ? json_clone(before_document) : NULL;
  operation->after_state = NULL; /* No after state for delete */

  /* Add to operation list */
  operation->next = transaction->operations;
  transaction->operations = operation;
  transaction->operation_count++;

  pthread_mutex_unlock(&transaction->lock);

  return 0; /* Success */
}

/* Query documents within a transaction */
json_value_t* transaction_query_documents(transaction_manager_t* manager, transaction_t* transaction,
                    const char* collection, json_value_t* query __attribute__((unused))) {
  if (!manager || !transaction || !collection) {
    return NULL;
  }

  pthread_mutex_lock(&transaction->lock);

  /* Check if transaction is active */
  if (transaction->state != TRANSACTION_ACTIVE) {
    pthread_mutex_unlock(&transaction->lock);
    return NULL;
  }

  /* This would involve querying the database with appropriate isolation level */
  /* For now, return an empty result */
  json_value_t* result = json_create_object();
  if (!result) {
    pthread_mutex_unlock(&transaction->lock);
    return NULL;
  }

  json_object_set(result, "documents", json_create_array());
  json_object_set(result, "count", json_create_integer(0));

  pthread_mutex_unlock(&transaction->lock);

  return result;
}