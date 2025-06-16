#include "utils/buffer_pool.h"
#include "transaction/transaction.h"
#include "utils/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

/* Maximum log line size */
#define MAX_LOG_LINE_SIZE 4096

/* Log entry types */
#define LOG_ENTRY_STATE_CHANGE "STATE"
#define LOG_ENTRY_OPERATION "OPERATION"
#define LOG_ENTRY_AUDIT "AUDIT"

/* 
 * Transaction Log Format:
 * Each log entry is a single line in the following format:
 * TIMESTAMP|TYPE|TRANSACTION_ID|DATA_JSON\n
 * 
 * Where:
 * - TIMESTAMP: Unix timestamp (seconds since epoch)
 * - TYPE: Either "STATE" or "OPERATION"
 * - TRANSACTION_ID: Transaction identifier
 * - DATA_JSON: JSON representation of the log entry data
 * 
 * For STATE entries, DATA_JSON contains:
 *  {"state": "active|committing|committed|aborting|aborted", "user_id": "user123"}
 * 
 * For OPERATION entries, DATA_JSON contains:
 *  {
 *   "type": "insert|update|delete",
 *   "collection": "collection_name",
 *   "document_id": "doc123", // may be null for inserts
 *   "before_state": {...},  // may be null for inserts
 *   "after_state": {...}   // may be null for deletes
 *  }
 */

/* Hash function for entry_cache */
static unsigned int hash_transaction_id(const char* id, int size) {
  if (!id) return 0;

  unsigned int hash = 0;
  while (*id) {
    hash = (hash * 31) + *id++;
  }

  return hash % size;
}

/* Free a cache entry */
static void free_cache_entry(cache_entry_t* entry) {
  if (!entry) return;

  if (entry->type) BUFFER_FREE(entry->type);
  if (entry->transaction_id) BUFFER_FREE(entry->transaction_id);
  if (entry->collection) BUFFER_FREE(entry->collection);
  if (entry->document_id) BUFFER_FREE(entry->document_id);
  if (entry->user_id) BUFFER_FREE(entry->user_id);

  BUFFER_FREE(entry);
}

/* Add an entry to the cache */
static int add_to_cache(transaction_log_t* log, const char* type, const char* transaction_id,
           time_t timestamp, transaction_state_t state, operation_type_t op_type,
           const char* collection, const char* document_id, const char* user_id) {
  if (!log || !log->entry_cache || !transaction_id) return 0;

  /* Check if cache is full */
  if (log->cache_entry_count >= log->max_cache_entries) {
    /* Cache is full - evict oldest entries (in a real implementation, would be more sophisticated) */
    time_t now = time(NULL);
    time_t cutoff = now - log->cache_expiry_seconds;

    for (int i = 0; i < log->cache_size; i++) {
      cache_entry_t** prev = &log->entry_cache[i];
      cache_entry_t* entry = *prev;

      while (entry) {
        cache_entry_t* next = entry->next;

        if (entry->timestamp < cutoff) {
          /* Entry is too old, remove it */
          *prev = next;
          free_cache_entry(entry);
          log->cache_entry_count--;
        } else {
          prev = &entry->next;
        }

        entry = next;
      }
    }
  }

  /* If still full after eviction, don't add */
  if (log->cache_entry_count >= log->max_cache_entries) return 0;

  /* Create a new cache entry */
  cache_entry_t* entry = (cache_entry_t*)BUFFER_ALLOC(sizeof(cache_entry_t));
  if (!entry) return 0;

  /* Copy data */
  entry->timestamp = timestamp;
  entry->type = BUFFER_STRDUP(type);
  entry->transaction_id = BUFFER_STRDUP(transaction_id);
  entry->state = state;
  entry->op_type = op_type;
  entry->collection = collection ? BUFFER_STRDUP(collection) : NULL;
  entry->document_id = document_id ? BUFFER_STRDUP(document_id) : NULL;
  entry->user_id = user_id ? BUFFER_STRDUP(user_id) : NULL;

  /* Check for memory allocation failures */
  if (!entry->type || !entry->transaction_id ||
    (collection && !entry->collection) ||
    (document_id && !entry->document_id) ||
    (user_id && !entry->user_id)) {
    free_cache_entry(entry);
    return 0;
  }

  /* Add to the cache */
  unsigned int hash = hash_transaction_id(transaction_id, log->cache_size);
  entry->next = log->entry_cache[hash];
  log->entry_cache[hash] = entry;
  log->cache_entry_count++;

  /* Update metrics */
  if (strcmp(type, "STATE") == 0) {
    log->total_transactions++;

    if (state == TRANSACTION_COMMITTED) {
      log->committed_transactions++;
      log->active_transactions--;
    } else if (state == TRANSACTION_ABORTED) {
      log->aborted_transactions++;
      log->active_transactions--;
    } else if (state == TRANSACTION_ACTIVE) {
      log->active_transactions++;
    }
  } else if (strcmp(type, "OPERATION") == 0) {
    log->total_operations++;
  }

  return 1;
}

/* Create a transaction log */
transaction_log_t* transaction_log_create(const char* log_file) {
  if (!log_file) {
    return NULL;
  }

  transaction_log_t* log = (transaction_log_t*)BUFFER_ALLOC(sizeof(transaction_log_t));
  if (!log) {
    return NULL;
  }

  log->log_file = BUFFER_STRDUP(log_file);
  if (!log->log_file) {
    BUFFER_FREE(log);
    return NULL;
  }

  /* Initialize with default values */
  log->audit_file = NULL;
  log->log_level = 1; /* Default to basic logging */
  log->audit_enabled = 0; /* Audit disabled by default */
  log->auto_archive = 0; /* Auto archive disabled by default */
  log->retention_days = 30; /* Default retention period */

  /* Initialize cache */
  log->cache_size = 256; /* Default cache size */
  log->max_cache_entries = 10000; /* Default max entries */
  log->cache_entry_count = 0;
  log->cache_expiry_seconds = 24 * 60 * 60; /* Default expiry: 24 hours */

  /* Allocate cache hash table */
  log->entry_cache = (cache_entry_t**)calloc(log->cache_size, sizeof(cache_entry_t*));
  if (!log->entry_cache) {
    BUFFER_FREE(log->log_file);
    BUFFER_FREE(log);
    return NULL;
  }

  /* Initialize metrics */
  log->total_transactions = 0;
  log->committed_transactions = 0;
  log->aborted_transactions = 0;
  log->active_transactions = 0;
  log->total_operations = 0;
  log->avg_duration_ms = 0;
  log->last_metrics_update = time(NULL);

  pthread_mutex_init(&log->lock, NULL);

  /* Create log directory if it doesn't exist */
  char* last_slash = strrchr(log->log_file, '/');
  if (last_slash) {
    char* dir_path = strndup(log->log_file, last_slash - log->log_file);
    if (dir_path) {
      /* Try to create all directories in the path */
      char temp[256];
      char* p = NULL;
      size_t len = strlen(dir_path);

      strncpy(temp, dir_path, sizeof(temp) - 1);
      temp[sizeof(temp) - 1] = '\0';

      if (len > 0 && temp[len - 1] != '/') {
        strncat(temp, "/", sizeof(temp) - strlen(temp) - 1);
      }

      for (p = temp + 1; *p; p++) {
        if (*p == '/') {
          *p = '\0';
          mkdir(temp, 0755);
          *p = '/';
        }
      }

      BUFFER_FREE(dir_path);
    }
  }

  /* Check if log file exists, create it if it doesn't */
  FILE* file = fopen(log->log_file, "a+");
  if (!file) {
    perror("Failed to open transaction log file");
    BUFFER_FREE(log->entry_cache);
    BUFFER_FREE(log->log_file);
    pthread_mutex_destroy(&log->lock);
    BUFFER_FREE(log);
    return NULL;
  }

  /* Add a header line if the file is new */
  if (ftell(file) == 0) {
    fprintf(file, "# JSONDB Transaction Log - Format: TIMESTAMP|TYPE|TRANSACTION_ID|DATA_JSON\n");
    fprintf(file, "# Created: %ld\n", (long)time(NULL));
  }

  fclose(file);

  /* Load metrics from log file if it's not new */
  // This would be implemented in a full solution to scan the log file
  // and update the metrics. For brevity, we'll skip this step.

  return log;
}

/* Free a transaction log */
void transaction_log_free(transaction_log_t* log) {
  if (!log) {
    return;
  }

  pthread_mutex_lock(&log->lock);

  if (log->log_file) {
    BUFFER_FREE(log->log_file);
  }

  if (log->audit_file) {
    BUFFER_FREE(log->audit_file);
  }

  /* Free cache entries */
  if (log->entry_cache) {
    for (int i = 0; i < log->cache_size; i++) {
      cache_entry_t* entry = log->entry_cache[i];
      while (entry) {
        cache_entry_t* next = entry->next;
        free_cache_entry(entry);
        entry = next;
      }
    }
    BUFFER_FREE(log->entry_cache);
  }

  pthread_mutex_unlock(&log->lock);
  pthread_mutex_destroy(&log->lock);

  BUFFER_FREE(log);
}

/* Helper function to append an entry to the log file */
static int log_append_entry(transaction_log_t* log, const char* type,
             const char* transaction_id, const char* data_json) {
  if (!log || !type || !transaction_id || !data_json) {
    return 0;
  }

  pthread_mutex_lock(&log->lock);

  FILE* file = fopen(log->log_file, "a");
  if (!file) {
    pthread_mutex_unlock(&log->lock);
    return 0;
  }

  /* Write log entry: TIMESTAMP|TYPE|TRANSACTION_ID|DATA_JSON */
  time_t now = time(NULL);
  fprintf(file, "%ld|%s|%s|%s\n", (long)now, type, transaction_id, data_json);

  /* Ensure data is flushed to disk for durability */
  fflush(file);

  /* On most Unix systems, we could use fsync for stronger durability guarantees */
  int fd = fileno(file);
  if (fd >= 0) {
    fsync(fd);
  }

  fclose(file);

  /* Extract data for cache and metrics */
  if (strcmp(type, "STATE") == 0) {
    json_value_t* data = json_parse(data_json);
    if (data && data->type == JSON_OBJECT) {
      /* Get state */
      json_value_t* state_val = json_object_get(data, "state");
      transaction_state_t state = TRANSACTION_ACTIVE; /* Default */

      if (state_val && state_val->type == JSON_STRING) {
        if (strcmp(state_val->value.string, "active") == 0) {
          state = TRANSACTION_ACTIVE;
        } else if (strcmp(state_val->value.string, "committing") == 0) {
          state = TRANSACTION_COMMITTING;
        } else if (strcmp(state_val->value.string, "committed") == 0) {
          state = TRANSACTION_COMMITTED;
        } else if (strcmp(state_val->value.string, "aborting") == 0) {
          state = TRANSACTION_ABORTING;
        } else if (strcmp(state_val->value.string, "aborted") == 0) {
          state = TRANSACTION_ABORTED;
        }
      }

      /* Get user ID */
      const char* user_id = NULL;
      json_value_t* user_id_val = json_object_get(data, "user_id");
      if (user_id_val && user_id_val->type == JSON_STRING) {
        user_id = user_id_val->value.string;
      }

      /* Add to cache */
      add_to_cache(log, type, transaction_id, now, state, OPERATION_INSERT,
            NULL, NULL, user_id);

      json_free(data);
    }
  } else if (strcmp(type, "OPERATION") == 0) {
    json_value_t* data = json_parse(data_json);
    if (data && data->type == JSON_OBJECT) {
      /* Get operation type */
      json_value_t* op_type_val = json_object_get(data, "type");
      operation_type_t op_type = OPERATION_INSERT; /* Default */

      if (op_type_val && op_type_val->type == JSON_STRING) {
        if (strcmp(op_type_val->value.string, "insert") == 0) {
          op_type = OPERATION_INSERT;
        } else if (strcmp(op_type_val->value.string, "update") == 0) {
          op_type = OPERATION_UPDATE;
        } else if (strcmp(op_type_val->value.string, "delete") == 0) {
          op_type = OPERATION_DELETE;
        }
      }

      /* Get collection */
      const char* collection = NULL;
      json_value_t* collection_val = json_object_get(data, "collection");
      if (collection_val && collection_val->type == JSON_STRING) {
        collection = collection_val->value.string;
      }

      /* Get document ID */
      const char* document_id = NULL;
      json_value_t* doc_id_val = json_object_get(data, "document_id");
      if (doc_id_val && doc_id_val->type == JSON_STRING) {
        document_id = doc_id_val->value.string;
      }

      /* Add to cache */
      add_to_cache(log, type, transaction_id, now, TRANSACTION_ACTIVE, op_type,
            collection, document_id, NULL);

      json_free(data);
    }
  }

  pthread_mutex_unlock(&log->lock);

  return 1;
}

/* Write a transaction state change to the log */
int transaction_log_write_state_change(transaction_log_t* log, transaction_t* transaction) {
  if (!log || !transaction) {
    return 0;
  }
  
  /* Create JSON for state change data */
  json_value_t* data = json_create_object();
  if (!data) {
    return 0;
  }
  
  json_object_set(data, "state", json_create_string(transaction_state_to_string(transaction->state)));
  if (transaction->user_id) {
    json_object_set(data, "user_id", json_create_string(transaction->user_id));
  }
  
  /* Isolation level for transaction start */
  if (transaction->state == TRANSACTION_ACTIVE) {
    json_object_set(data, "isolation_level", 
           json_create_string(isolation_level_to_string(transaction->isolation_level)));
  }
  
  /* Add commit time for committed transactions */
  if (transaction->state == TRANSACTION_COMMITTED && transaction->commit_time > 0) {
    json_object_set(data, "commit_time", json_create_integer(transaction->commit_time));
  }
  
  char* data_json = json_stringify(data);
  json_free(data);
  
  if (!data_json) {
    return 0;
  }
  
  int result = log_append_entry(log, LOG_ENTRY_STATE_CHANGE, transaction->id, data_json);
  
  BUFFER_FREE(data_json);
  
  return result;
}

/* Write a transaction operation to the log */
int transaction_log_write_operation(transaction_log_t* log, transaction_t* transaction, 
                 transaction_operation_t* operation) {
  if (!log || !transaction || !operation) {
    return 0;
  }
  
  /* Create JSON for operation data */
  json_value_t* data = json_create_object();
  if (!data) {
    return 0;
  }
  
  /* Set operation type */
  switch (operation->type) {
    case OPERATION_INSERT:
      json_object_set(data, "type", json_create_string("insert"));
      break;
    case OPERATION_UPDATE:
      json_object_set(data, "type", json_create_string("update"));
      break;
    case OPERATION_DELETE:
      json_object_set(data, "type", json_create_string("delete"));
      break;
    default:
      json_object_set(data, "type", json_create_string("unknown"));
      break;
  }
  
  /* Set collection name */
  if (operation->collection_name) {
    json_object_set(data, "collection", json_create_string(operation->collection_name));
  }
  
  /* Set document ID (if any) */
  if (operation->document_id) {
    json_object_set(data, "document_id", json_create_string(operation->document_id));
  }
  
  /* Set before and after states (if any) */
  if (operation->before_state) {
    json_object_set(data, "before_state", json_clone(operation->before_state));
  }
  
  if (operation->after_state) {
    json_object_set(data, "after_state", json_clone(operation->after_state));
  }
  
  char* data_json = json_stringify(data);
  json_free(data);
  
  if (!data_json) {
    return 0;
  }
  
  int result = log_append_entry(log, LOG_ENTRY_OPERATION, transaction->id, data_json);
  
  BUFFER_FREE(data_json);
  
  return result;
}

/* Helper function to parse a log line */
static int parse_log_line(char* line, time_t* timestamp, char** type, 
            char** transaction_id, char** data_json) {
  /* Skip comments and empty lines */
  if (line[0] == '#' || line[0] == '\0' || line[0] == '\n') {
    return 0;
  }
  
  /* Parse log entry: TIMESTAMP|TYPE|TRANSACTION_ID|DATA_JSON */
  char* token = strtok(line, "|");
  if (!token) return 0;
  *timestamp = atol(token);
  
  token = strtok(NULL, "|");
  if (!token) return 0;
  *type = token;
  
  token = strtok(NULL, "|");
  if (!token) return 0;
  *transaction_id = token;
  
  token = strtok(NULL, "\n");
  if (!token) return 0;
  *data_json = token;
  
  return 1;
}

/* Create a transaction operation from JSON data */
static transaction_operation_t* operation_from_json(const char* data_json) {
  json_value_t* data = json_parse(data_json);
  if (!data || data->type != JSON_OBJECT) {
    if (data) json_free(data);
    return NULL;
  }
  
  /* Get operation type */
  json_value_t* type_val = json_object_get(data, "type");
  if (!type_val || type_val->type != JSON_STRING) {
    json_free(data);
    return NULL;
  }
  
  operation_type_t op_type;
  if (strcmp(type_val->value.string, "insert") == 0) {
    op_type = OPERATION_INSERT;
  } else if (strcmp(type_val->value.string, "update") == 0) {
    op_type = OPERATION_UPDATE;
  } else if (strcmp(type_val->value.string, "delete") == 0) {
    op_type = OPERATION_DELETE;
  } else {
    json_free(data);
    return NULL;
  }
  
  /* Get collection name */
  json_value_t* collection_val = json_object_get(data, "collection");
  if (!collection_val || collection_val->type != JSON_STRING) {
    json_free(data);
    return NULL;
  }
  
  /* Get document ID (optional) */
  json_value_t* doc_id_val = json_object_get(data, "document_id");
  const char* doc_id = NULL;
  if (doc_id_val && doc_id_val->type == JSON_STRING) {
    doc_id = doc_id_val->value.string;
  }
  
  /* Get before and after states (optional) */
  json_value_t* before_state = json_object_get(data, "before_state");
  json_value_t* after_state = json_object_get(data, "after_state");
  
  /* Create operation */
  transaction_operation_t* operation = (transaction_operation_t*)BUFFER_ALLOC(sizeof(transaction_operation_t));
  if (!operation) {
    json_free(data);
    return NULL;
  }
  
  operation->type = op_type;
  operation->collection_name = BUFFER_STRDUP(collection_val->value.string);
  operation->document_id = doc_id ? BUFFER_STRDUP(doc_id) : NULL;
  operation->before_state = before_state ? json_clone(before_state) : NULL;
  operation->after_state = after_state ? json_clone(after_state) : NULL;
  operation->next = NULL;
  
  json_free(data);
  
  return operation;
}

/* Create a transaction from recovery data */
static transaction_t* create_recovery_transaction(const char* transaction_id, 
                        const char* state_json,
                        time_t timestamp) {
  json_value_t* data = json_parse(state_json);
  if (!data || data->type != JSON_OBJECT) {
    if (data) json_free(data);
    return NULL;
  }
  
  /* Get user ID */
  json_value_t* user_id_val = json_object_get(data, "user_id");
  if (!user_id_val || user_id_val->type != JSON_STRING) {
    json_free(data);
    return NULL;
  }
  
  /* Get isolation level */
  json_value_t* isolation_val = json_object_get(data, "isolation_level");
  isolation_level_t isolation = ISOLATION_READ_COMMITTED; /* Default */
  if (isolation_val && isolation_val->type == JSON_STRING) {
    isolation = isolation_level_from_string(isolation_val->value.string);
  }
  
  /* Create transaction */
  transaction_t* transaction = (transaction_t*)BUFFER_ALLOC(sizeof(transaction_t));
  if (!transaction) {
    json_free(data);
    return NULL;
  }
  
  transaction->id = BUFFER_STRDUP(transaction_id);
  transaction->state = TRANSACTION_ACTIVE; /* Default, will be updated later */
  transaction->start_time = timestamp;
  transaction->commit_time = 0;
  transaction->user_id = BUFFER_STRDUP(user_id_val->value.string);
  transaction->operations = NULL;
  transaction->operation_count = 0;
  transaction->isolation_level = isolation;
  pthread_mutex_init(&transaction->lock, NULL);
  
  json_free(data);
  
  return transaction;
}

/* Update transaction state from JSON data */
static int update_transaction_state(transaction_t* transaction, const char* state_json) {
  json_value_t* data = json_parse(state_json);
  if (!data || data->type != JSON_OBJECT) {
    if (data) json_free(data);
    return 0;
  }
  
  /* Get state */
  json_value_t* state_val = json_object_get(data, "state");
  if (!state_val || state_val->type != JSON_STRING) {
    json_free(data);
    return 0;
  }
  
  /* Update transaction state */
  if (strcmp(state_val->value.string, "active") == 0) {
    transaction->state = TRANSACTION_ACTIVE;
  } else if (strcmp(state_val->value.string, "committing") == 0) {
    transaction->state = TRANSACTION_COMMITTING;
  } else if (strcmp(state_val->value.string, "committed") == 0) {
    transaction->state = TRANSACTION_COMMITTED;
    
    /* Get commit time */
    json_value_t* commit_time_val = json_object_get(data, "commit_time");
    if (commit_time_val && commit_time_val->type == JSON_INTEGER) {
      transaction->commit_time = commit_time_val->value.integer;
    }
  } else if (strcmp(state_val->value.string, "aborting") == 0) {
    transaction->state = TRANSACTION_ABORTING;
  } else if (strcmp(state_val->value.string, "aborted") == 0) {
    transaction->state = TRANSACTION_ABORTED;
  }
  
  json_free(data);
  
  return 1;
}

/* Read operations from the log for recovery */
int transaction_log_read_operations(transaction_log_t* log, transaction_manager_t* manager) {
  if (!log || !manager) {
    return 0;
  }
  
  pthread_mutex_lock(&log->lock);
  
  FILE* file = fopen(log->log_file, "r");
  if (!file) {
    pthread_mutex_unlock(&log->lock);
    return 0;
  }
  
  /* Hash table to store transactions during recovery */
  /* In a real implementation, we would use a proper hash table */
  /* For simplicity, we use a fixed-size array of pointers */
  #define MAX_RECOVERY_TRANSACTIONS 1000
  transaction_t* transactions[MAX_RECOVERY_TRANSACTIONS] = {NULL};
  int transaction_count = 0;
  
  char line[MAX_LOG_LINE_SIZE];
  
  /* Process each line in the log file */
  while (fgets(line, sizeof(line), file)) {
    time_t timestamp;
    char *type, *transaction_id, *data_json;
    
    if (!parse_log_line(line, &timestamp, &type, &transaction_id, &data_json)) {
      continue;
    }
    
    /* Find or create transaction */
    transaction_t* transaction = NULL;
    for (int i = 0; i < transaction_count; i++) {
      if (transactions[i] && strcmp(transactions[i]->id, transaction_id) == 0) {
        transaction = transactions[i];
        break;
      }
    }
    
    /* Process based on entry type */
    if (strcmp(type, LOG_ENTRY_STATE_CHANGE) == 0) {
      if (!transaction) {
        /* Create new transaction for state entry */
        transaction = create_recovery_transaction(transaction_id, data_json, timestamp);
        if (transaction && transaction_count < MAX_RECOVERY_TRANSACTIONS) {
          transactions[transaction_count++] = transaction;
        }
      } else {
        /* Update existing transaction state */
        update_transaction_state(transaction, data_json);
      }
    } else if (strcmp(type, LOG_ENTRY_OPERATION) == 0 && transaction) {
      /* Add operation to transaction */
      transaction_operation_t* operation = operation_from_json(data_json);
      if (operation) {
        /* Add operation to the head of the list (will be in reverse order) */
        operation->next = transaction->operations;
        transaction->operations = operation;
        transaction->operation_count++;
      }
    }
  }
  
  fclose(file);
  
  /* Apply recovered transactions based on their state */
  int recovery_count = 0;
  
  for (int i = 0; i < transaction_count; i++) {
    transaction_t* transaction = transactions[i];
    if (!transaction) continue;
    
    switch (transaction->state) {
      case TRANSACTION_COMMITTED:
        /* Replay committed transaction */
        /* In a real implementation, we would apply these operations to the database */
        recovery_count++;
        break;
        
      case TRANSACTION_ACTIVE:
      case TRANSACTION_COMMITTING:
        /* Incomplete transaction - rollback */
        /* In a real implementation, we would undo any partially applied operations */
        break;
        
      case TRANSACTION_ABORTING:
      case TRANSACTION_ABORTED:
        /* Already aborted - nothing to do */
        break;
    }
    
    /* Clean up transaction memory */
    /* In a real implementation, we might need to keep some transactions around */
    transaction_operation_t* operation = transaction->operations;
    while (operation) {
      transaction_operation_t* next = operation->next;
      
      /* Free operation memory */
      if (operation->collection_name) BUFFER_FREE(operation->collection_name);
      if (operation->document_id) BUFFER_FREE(operation->document_id);
      if (operation->before_state) json_free(operation->before_state);
      if (operation->after_state) json_free(operation->after_state);
      
      BUFFER_FREE(operation);
      operation = next;
    }
    
    if (transaction->id) BUFFER_FREE(transaction->id);
    if (transaction->user_id) BUFFER_FREE(transaction->user_id);
    pthread_mutex_destroy(&transaction->lock);
    BUFFER_FREE(transaction);
  }
  
  pthread_mutex_unlock(&log->lock);
  
  return recovery_count;
}

/* Compact the transaction log by removing unnecessary entries */
int transaction_log_compact(transaction_log_t* log) {
  if (!log) {
    return 0;
  }
  
  pthread_mutex_lock(&log->lock);
  
  /* Create a temporary file for the compacted log */
  char temp_file[256];
  snprintf(temp_file, sizeof(temp_file), "%s.new", log->log_file);
  
  FILE* in_file = fopen(log->log_file, "r");
  if (!in_file) {
    pthread_mutex_unlock(&log->lock);
    return 0;
  }
  
  FILE* out_file = fopen(temp_file, "w");
  if (!out_file) {
    fclose(in_file);
    pthread_mutex_unlock(&log->lock);
    return 0;
  }
  
  /* Copy header lines */
  fprintf(out_file, "# JSONDB Transaction Log - Format: TIMESTAMP|TYPE|TRANSACTION_ID|DATA_JSON\n");
  fprintf(out_file, "# Compacted: %ld\n", (long)time(NULL));
  
  /* Simple implementation of log compaction:
   * 1. First pass: identify all committed and aborted transactions
   * 2. Second pass: keep only relevant entries
   *  - For committed transactions: keep only the final commit record
   *  - For aborted transactions: keep only the final abort record
   *  - For active transactions: keep all records
   */
  
  /* First pass: identify completed transactions */
  char line[MAX_LOG_LINE_SIZE];
  char line_copy[MAX_LOG_LINE_SIZE];
  #define MAX_COMPLETED_TRANSACTIONS 1000
  char* completed_transactions[MAX_COMPLETED_TRANSACTIONS] = {NULL};
  int completed_count = 0;
  
  while (fgets(line, sizeof(line), in_file) && completed_count < MAX_COMPLETED_TRANSACTIONS) {
    time_t timestamp;
    char *type, *transaction_id, *data_json;
    
    if (!parse_log_line(line, &timestamp, &type, &transaction_id, &data_json)) {
      continue;
    }
    
    if (strcmp(type, LOG_ENTRY_STATE_CHANGE) == 0) {
      json_value_t* data = json_parse(data_json);
      if (!data || data->type != JSON_OBJECT) {
        if (data) json_free(data);
        continue;
      }
      
      json_value_t* state_val = json_object_get(data, "state");
      if (!state_val || state_val->type != JSON_STRING) {
        json_free(data);
        continue;
      }
      
      if (strcmp(state_val->value.string, "committed") == 0 || 
        strcmp(state_val->value.string, "aborted") == 0) {
        /* Add to completed transactions if not already there */
        int found = 0;
        for (int i = 0; i < completed_count; i++) {
          if (completed_transactions[i] && 
            strcmp(completed_transactions[i], transaction_id) == 0) {
            found = 1;
            break;
          }
        }
        
        if (!found) {
          completed_transactions[completed_count++] = BUFFER_STRDUP(transaction_id);
        }
      }
      
      json_free(data);
    }
  }
  
  /* Reset file position */
  rewind(in_file);
  
  /* Second pass: write compacted log */
  while (fgets(line, sizeof(line), in_file)) {
    /* Skip comment lines */
    if (line[0] == '#') {
      continue;
    }
    
    /* Parse the line */
    time_t timestamp;
    char *type, *transaction_id, *data_json;
    
    strcpy(line_copy, line);
    if (!parse_log_line(line_copy, &timestamp, &type, &transaction_id, &data_json)) {
      continue;
    }
    
    /* Check if this transaction is completed */
    int is_completed = 0;
    for (int i = 0; i < completed_count; i++) {
      if (completed_transactions[i] && strcmp(completed_transactions[i], transaction_id) == 0) {
        is_completed = 1;
        break;
      }
    }
    
    /* Keep the entry if:
     * 1. Transaction is not completed, or
     * 2. This is a final state change (commit or abort), or
     * 3. This is an audit entry (we keep all audit entries)
     */
    if (!is_completed || 
      (strcmp(type, LOG_ENTRY_STATE_CHANGE) == 0 &&
       (strstr(data_json, "\"state\":\"committed\"") || 
       strstr(data_json, "\"state\":\"aborted\""))) ||
      strcmp(type, LOG_ENTRY_AUDIT) == 0) {
      fputs(line, out_file);
    }
  }
  
  /* Clean up */
  for (int i = 0; i < completed_count; i++) {
    if (completed_transactions[i]) {
      BUFFER_FREE(completed_transactions[i]);
    }
  }
  
  fclose(in_file);
  fclose(out_file);
  
  /* Replace the old file with the new one */
  if (rename(temp_file, log->log_file) != 0) {
    /* Failed to rename, try copying the content instead */
    in_file = fopen(temp_file, "r");
    out_file = fopen(log->log_file, "w");
    
    if (in_file && out_file) {
      while (fgets(line, sizeof(line), in_file)) {
        fputs(line, out_file);
      }
    }
    
    if (in_file) fclose(in_file);
    if (out_file) fclose(out_file);
    
    /* Remove the temporary file */
    unlink(temp_file);
  }
  
  pthread_mutex_unlock(&log->lock);
  
  return 1;
}

/* Create an advanced transaction log with audit capabilities */
transaction_log_t* transaction_log_create_advanced(const char* log_file, const char* audit_file, 
                         int log_level, int audit_enabled) {
  if (!log_file) {
    return NULL;
  }
  
  transaction_log_t* log = transaction_log_create(log_file);
  if (!log) {
    return NULL;
  }
  
  /* Set advanced parameters */
  if (audit_file) {
    log->audit_file = BUFFER_STRDUP(audit_file);
    if (!log->audit_file) {
      transaction_log_free(log);
      return NULL;
    }
    
    /* Create audit directory if it doesn't exist */
    char* last_slash = strrchr(log->audit_file, '/');
    if (last_slash) {
      char* dir_path = strndup(log->audit_file, last_slash - log->audit_file);
      if (dir_path) {
        /* Create directory structure */
        char temp[256];
        char* p = NULL;
        size_t len = strlen(dir_path);
        
        strncpy(temp, dir_path, sizeof(temp) - 1);
        temp[sizeof(temp) - 1] = '\0';
        
        if (len > 0 && temp[len - 1] != '/') {
          strncat(temp, "/", sizeof(temp) - strlen(temp) - 1);
        }
        
        for (p = temp + 1; *p; p++) {
          if (*p == '/') {
            *p = '\0';
            mkdir(temp, 0755);
            *p = '/';
          }
        }
        
        BUFFER_FREE(dir_path);
      }
    }
    
    /* Initialize audit file if it doesn't exist */
    FILE* file = fopen(log->audit_file, "a+");
    if (file) {
      if (ftell(file) == 0) {
        fprintf(file, "# JSONDB Audit Trail - Format: TIMESTAMP|TRANSACTION_ID|DATA_JSON\n");
        fprintf(file, "# Created: %ld\n", (long)time(NULL));
      }
      fclose(file);
    } else {
      /* Non-fatal error: we can still function without audit file */
      perror("Failed to open audit file");
    }
  }
  
  /* Set configuration parameters */
  log->log_level = log_level;
  log->audit_enabled = audit_enabled;
  
  return log;
}

/* Configure transaction log settings */
int transaction_log_configure(transaction_log_t* log, int log_level, int audit_enabled,
              int auto_archive, int retention_days) {
  if (!log) {
    return 0;
  }
  
  pthread_mutex_lock(&log->lock);
  
  log->log_level = log_level;
  log->audit_enabled = audit_enabled;
  log->auto_archive = auto_archive;
  log->retention_days = retention_days;
  
  pthread_mutex_unlock(&log->lock);
  
  return 1;
}

/* Write a detailed audit entry */
int transaction_log_write_audit(transaction_log_t* log, transaction_t* transaction,
               const char* client_ip, const char* application_name,
               int is_retry, int retry_count, int error_code, int duration_ms) {
  if (!log || !transaction || !log->audit_enabled) {
    return 0; /* Silently skip if audit is disabled */
  }
  
  /* Create JSON for audit data */
  json_value_t* data = json_create_object();
  if (!data) {
    return 0;
  }
  
  /* Basic transaction information */
  json_object_set(data, "state", json_create_string(transaction_state_to_string(transaction->state)));
  if (transaction->user_id) {
    json_object_set(data, "user_id", json_create_string(transaction->user_id));
  }
  
  /* Audit-specific information */
  if (client_ip) {
    json_object_set(data, "client_ip", json_create_string(client_ip));
  }
  
  if (application_name) {
    json_object_set(data, "application_name", json_create_string(application_name));
  }
  
  json_object_set(data, "is_retry", json_create_boolean(is_retry));
  json_object_set(data, "retry_count", json_create_integer(retry_count));
  
  if (error_code) {
    json_object_set(data, "error_code", json_create_integer(error_code));
  }
  
  json_object_set(data, "duration_ms", json_create_integer(duration_ms));
  json_object_set(data, "operation_count", json_create_integer(transaction->operation_count));
  json_object_set(data, "isolation_level", 
         json_create_string(isolation_level_to_string(transaction->isolation_level)));
  
  char* data_json = json_stringify(data);
  json_free(data);
  
  if (!data_json) {
    return 0;
  }
  
  /* Write to transaction log if detailed logging is enabled */
  int result = 0;
  if (log->log_level >= 2) {
    result = log_append_entry(log, LOG_ENTRY_AUDIT, transaction->id, data_json);
  }
  
  /* Write to dedicated audit file if available */
  if (log->audit_file) {
    pthread_mutex_lock(&log->lock);
    
    FILE* file = fopen(log->audit_file, "a");
    if (file) {
      /* Write audit entry: TIMESTAMP|TRANSACTION_ID|DATA_JSON */
      time_t now = time(NULL);
      fprintf(file, "%ld|%s|%s\n", (long)now, transaction->id, data_json);
      
      /* Ensure data is flushed to disk for durability */
      fflush(file);
      int fd = fileno(file);
      if (fd >= 0) {
        fsync(fd);
      }
      
      fclose(file);
      result = 1;
    }
    
    pthread_mutex_unlock(&log->lock);
  }
  
  BUFFER_FREE(data_json);
  
  return result;
}

/* Archive logs older than retention_days */
int transaction_log_archive(transaction_log_t* log, const char* archive_dir) {
  if (!log || !archive_dir) {
    return 0;
  }
  
  pthread_mutex_lock(&log->lock);
  
  /* Ensure archive directory exists */
  struct stat st = {0};
  if (stat(archive_dir, &st) == -1) {
    /* Try to create the directory */
    if (mkdir(archive_dir, 0755) != 0) {
      pthread_mutex_unlock(&log->lock);
      return 0;
    }
  } else if (!S_ISDIR(st.st_mode)) {
    /* Path exists but is not a directory */
    pthread_mutex_unlock(&log->lock);
    return 0;
  }
  
  /* Create archive filename with timestamp */
  time_t now = time(NULL);
  struct tm* tm_info = localtime(&now);
  
  char archive_filename[512];
  strftime(archive_filename, sizeof(archive_filename), "%Y%m%d_%H%M%S", tm_info);
  
  char* log_basename = strrchr(log->log_file, '/');
  log_basename = log_basename ? log_basename + 1 : log->log_file;
  
  char archive_path[768];
  snprintf(archive_path, sizeof(archive_path), "%s/%s_%s", 
       archive_dir, archive_filename, log_basename);
  
  /* Copy the log file to the archive */
  FILE* src = fopen(log->log_file, "r");
  if (!src) {
    pthread_mutex_unlock(&log->lock);
    return 0;
  }
  
  FILE* dst = fopen(archive_path, "w");
  if (!dst) {
    fclose(src);
    pthread_mutex_unlock(&log->lock);
    return 0;
  }
  
  /* Copy contents */
  char buffer[4096];
  size_t bytes;
  while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
    fwrite(buffer, 1, bytes, dst);
  }
  
  fclose(src);
  fclose(dst);
  
  /* Clear the original log file, but keep the header */
  FILE* file = fopen(log->log_file, "w");
  if (file) {
    fprintf(file, "# JSONDB Transaction Log - Format: TIMESTAMP|TYPE|TRANSACTION_ID|DATA_JSON\n");
    fprintf(file, "# Archived: %ld - Previous logs moved to %s\n", (long)now, archive_path);
    fclose(file);
  }
  
  /* Also archive audit file if it exists */
  if (log->audit_file) {
    log_basename = strrchr(log->audit_file, '/');
    log_basename = log_basename ? log_basename + 1 : log->audit_file;
    
    snprintf(archive_path, sizeof(archive_path), "%s/%s_%s", 
         archive_dir, archive_filename, log_basename);
    
    src = fopen(log->audit_file, "r");
    if (src) {
      dst = fopen(archive_path, "w");
      if (dst) {
        while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
          fwrite(buffer, 1, bytes, dst);
        }
        fclose(dst);
      }
      fclose(src);
      
      /* Clear the original audit file, but keep the header */
      file = fopen(log->audit_file, "w");
      if (file) {
        fprintf(file, "# JSONDB Audit Trail - Format: TIMESTAMP|TRANSACTION_ID|DATA_JSON\n");
        fprintf(file, "# Archived: %ld - Previous logs moved to %s\n", (long)now, archive_path);
        fclose(file);
      }
    }
  }
  
  pthread_mutex_unlock(&log->lock);
  
  return 1;
}

/* Get JSON report of transaction logs for a time period */
json_value_t* transaction_log_get_report(transaction_log_t* log, time_t start_time, time_t end_time) {
  if (!log) {
    return NULL;
  }
  
  pthread_mutex_lock(&log->lock);
  
  json_value_t* report = json_create_object();
  if (!report) {
    pthread_mutex_unlock(&log->lock);
    return NULL;
  }
  
  /* Set time range */
  json_object_set(report, "start_time", json_create_integer(start_time));
  json_object_set(report, "end_time", json_create_integer(end_time));
  
  /* Counters for different transaction states and operations */
  int active_count = 0;
  int committed_count = 0;
  int aborted_count = 0;
  int insert_count = 0;
  int update_count = 0;
  int delete_count = 0;
  int total_transactions = 0;
  int total_operations = 0;
  int error_count = 0;
  
  /* Collect transaction data within the time range */
  json_value_t* transactions = json_create_array();
  if (!transactions) {
    json_free(report);
    pthread_mutex_unlock(&log->lock);
    return NULL;
  }
  
  /* Set of transaction IDs seen in this report */
  #define MAX_REPORT_TRANSACTIONS 10000
  char* seen_transactions[MAX_REPORT_TRANSACTIONS] = {NULL};
  int seen_count = 0;
  
  /* Process transaction log */
  FILE* file = fopen(log->log_file, "r");
  if (file) {
    char line[MAX_LOG_LINE_SIZE];
    
    while (fgets(line, sizeof(line), file)) {
      time_t timestamp;
      char *type, *transaction_id, *data_json;
      
      if (!parse_log_line(line, &timestamp, &type, &transaction_id, &data_json)) {
        continue;
      }
      
      /* Skip entries outside time range */
      if (timestamp < start_time || timestamp > end_time) {
        continue;
      }
      
      /* Process by type */
      if (strcmp(type, LOG_ENTRY_STATE_CHANGE) == 0) {
        json_value_t* data = json_parse(data_json);
        if (!data || data->type != JSON_OBJECT) {
          if (data) json_free(data);
          continue;
        }
        
        json_value_t* state_val = json_object_get(data, "state");
        if (!state_val || state_val->type != JSON_STRING) {
          json_free(data);
          continue;
        }
        
        /* Count by state */
        if (strcmp(state_val->value.string, "active") == 0) {
          active_count++;
        } else if (strcmp(state_val->value.string, "committed") == 0) {
          committed_count++;
        } else if (strcmp(state_val->value.string, "aborted") == 0) {
          aborted_count++;
        }
        
        /* Count unique transactions */
        int is_new = 1;
        for (int i = 0; i < seen_count; i++) {
          if (seen_transactions[i] && strcmp(seen_transactions[i], transaction_id) == 0) {
            is_new = 0;
            break;
          }
        }
        
        if (is_new && seen_count < MAX_REPORT_TRANSACTIONS) {
          seen_transactions[seen_count++] = BUFFER_STRDUP(transaction_id);
          total_transactions++;
          
          /* Add transaction summary to report if it's a terminal state */
          if (strcmp(state_val->value.string, "committed") == 0 || 
            strcmp(state_val->value.string, "aborted") == 0) {
            
            json_value_t* trans_summary = json_create_object();
            if (trans_summary) {
              json_object_set(trans_summary, "id", json_create_string(transaction_id));
              json_object_set(trans_summary, "timestamp", json_create_integer(timestamp));
              json_object_set(trans_summary, "state", json_create_string(state_val->value.string));
              
              json_value_t* user_id_val = json_object_get(data, "user_id");
              if (user_id_val && user_id_val->type == JSON_STRING) {
                json_object_set(trans_summary, "user_id", json_create_string(user_id_val->value.string));
              }
              
              json_array_append(transactions, trans_summary);
            }
          }
        }
        
        json_free(data);
      }
      else if (strcmp(type, LOG_ENTRY_OPERATION) == 0) {
        /* Count operations */
        total_operations++;
        
        json_value_t* data = json_parse(data_json);
        if (!data || data->type != JSON_OBJECT) {
          if (data) json_free(data);
          continue;
        }
        
        json_value_t* op_type_val = json_object_get(data, "type");
        if (!op_type_val || op_type_val->type != JSON_STRING) {
          json_free(data);
          continue;
        }
        
        /* Count by operation type */
        if (strcmp(op_type_val->value.string, "insert") == 0) {
          insert_count++;
        } else if (strcmp(op_type_val->value.string, "update") == 0) {
          update_count++;
        } else if (strcmp(op_type_val->value.string, "delete") == 0) {
          delete_count++;
        }
        
        json_free(data);
      }
      else if (strcmp(type, LOG_ENTRY_AUDIT) == 0) {
        /* Process audit entries */
        json_value_t* data = json_parse(data_json);
        if (!data || data->type != JSON_OBJECT) {
          if (data) json_free(data);
          continue;
        }
        
        /* Count errors */
        json_value_t* error_code_val = json_object_get(data, "error_code");
        if (error_code_val && error_code_val->type == JSON_INTEGER && error_code_val->value.integer != 0) {
          error_count++;
        }
        
        json_free(data);
      }
    }
    
    fclose(file);
  }
  
  /* Clean up seen transactions */
  for (int i = 0; i < seen_count; i++) {
    if (seen_transactions[i]) {
      BUFFER_FREE(seen_transactions[i]);
    }
  }
  
  /* Add statistics to report */
  json_value_t* stats = json_create_object();
  if (stats) {
    json_object_set(stats, "total_transactions", json_create_integer(total_transactions));
    json_object_set(stats, "active_transactions", json_create_integer(active_count));
    json_object_set(stats, "committed_transactions", json_create_integer(committed_count));
    json_object_set(stats, "aborted_transactions", json_create_integer(aborted_count));
    json_object_set(stats, "total_operations", json_create_integer(total_operations));
    json_object_set(stats, "insert_operations", json_create_integer(insert_count));
    json_object_set(stats, "update_operations", json_create_integer(update_count));
    json_object_set(stats, "delete_operations", json_create_integer(delete_count));
    json_object_set(stats, "errors", json_create_integer(error_count));
    
    json_object_set(report, "stats", stats);
  }
  
  /* Add transaction list to report */
  json_object_set(report, "transactions", transactions);
  
  pthread_mutex_unlock(&log->lock);
  
  return report;
}

/* Get transaction history for a specific document */
json_value_t* transaction_log_get_document_history(transaction_log_t* log, 
                        const char* collection, const char* document_id) {
  if (!log || !collection || !document_id) {
    return NULL;
  }
  
  pthread_mutex_lock(&log->lock);
  
  json_value_t* history = json_create_object();
  if (!history) {
    pthread_mutex_unlock(&log->lock);
    return NULL;
  }
  
  /* Set document information */
  json_object_set(history, "collection", json_create_string(collection));
  json_object_set(history, "document_id", json_create_string(document_id));
  
  /* Array of document changes */
  json_value_t* changes = json_create_array();
  if (!changes) {
    json_free(history);
    pthread_mutex_unlock(&log->lock);
    return NULL;
  }
  
  /* Process transaction log */
  FILE* file = fopen(log->log_file, "r");
  if (file) {
    char line[MAX_LOG_LINE_SIZE];
    
    while (fgets(line, sizeof(line), file)) {
      time_t timestamp;
      char *type, *transaction_id, *data_json;
      
      if (!parse_log_line(line, &timestamp, &type, &transaction_id, &data_json)) {
        continue;
      }
      
      /* We're only interested in operations */
      if (strcmp(type, LOG_ENTRY_OPERATION) != 0) {
        continue;
      }
      
      json_value_t* data = json_parse(data_json);
      if (!data || data->type != JSON_OBJECT) {
        if (data) json_free(data);
        continue;
      }
      
      /* Check if this operation is for our target document */
      json_value_t* op_collection = json_object_get(data, "collection");
      json_value_t* op_document_id = json_object_get(data, "document_id");
      
      if (!op_collection || op_collection->type != JSON_STRING ||
        !op_document_id || op_document_id->type != JSON_STRING) {
        json_free(data);
        continue;
      }
      
      if (strcmp(op_collection->value.string, collection) != 0 ||
        strcmp(op_document_id->value.string, document_id) != 0) {
        json_free(data);
        continue;
      }
      
      /* This operation affected our document - add it to history */
      json_value_t* change = json_create_object();
      if (change) {
        json_object_set(change, "timestamp", json_create_integer(timestamp));
        json_object_set(change, "transaction_id", json_create_string(transaction_id));
        
        json_value_t* op_type = json_object_get(data, "type");
        if (op_type && op_type->type == JSON_STRING) {
          json_object_set(change, "operation", json_create_string(op_type->value.string));
        }
        
        json_value_t* before_state = json_object_get(data, "before_state");
        if (before_state) {
          json_object_set(change, "before_state", json_clone(before_state));
        }
        
        json_value_t* after_state = json_object_get(data, "after_state");
        if (after_state) {
          json_object_set(change, "after_state", json_clone(after_state));
        }
        
        /* Add to changes array */
        json_array_append(changes, change);
      }
      
      json_free(data);
    }
    
    fclose(file);
  }
  
  /* Add changes to history */
  json_object_set(history, "changes", changes);
  json_object_set(history, "change_count", json_create_integer(json_array_size(changes)));
  
  pthread_mutex_unlock(&log->lock);
  
  return history;
}

/* Get transaction log statistics */
json_value_t* transaction_log_get_stats(transaction_log_t* log) {
  if (!log) {
    return NULL;
  }
  
  pthread_mutex_lock(&log->lock);
  
  json_value_t* stats = json_create_object();
  if (!stats) {
    pthread_mutex_unlock(&log->lock);
    return NULL;
  }
  
  /* Get log file size and other stats */
  struct stat file_stat;
  if (stat(log->log_file, &file_stat) == 0) {
    json_object_set(stats, "file_size", json_create_integer(file_stat.st_size));
    json_object_set(stats, "last_modified", json_create_integer(file_stat.st_mtime));
  }
  
  /* Add audit file stats if it exists */
  if (log->audit_file) {
    if (stat(log->audit_file, &file_stat) == 0) {
      json_object_set(stats, "audit_file_size", json_create_integer(file_stat.st_size));
      json_object_set(stats, "audit_last_modified", json_create_integer(file_stat.st_mtime));
    }
  }
  
  /* Add configuration information */
  json_object_set(stats, "log_level", json_create_integer(log->log_level));
  json_object_set(stats, "audit_enabled", json_create_boolean(log->audit_enabled));
  json_object_set(stats, "auto_archive", json_create_boolean(log->auto_archive));
  json_object_set(stats, "retention_days", json_create_integer(log->retention_days));
  
  /* Count entries */
  FILE* file = fopen(log->log_file, "r");
  if (file) {
    int total_entries = 0;
    int state_entries = 0;
    int operation_entries = 0;
    int audit_entries = 0;
    
    char line[MAX_LOG_LINE_SIZE];
    while (fgets(line, sizeof(line), file)) {
      time_t timestamp;
      char *type, *transaction_id, *data_json;
      
      if (!parse_log_line(line, &timestamp, &type, &transaction_id, &data_json)) {
        continue;
      }
      
      total_entries++;
      
      if (strcmp(type, LOG_ENTRY_STATE_CHANGE) == 0) {
        state_entries++;
      } else if (strcmp(type, LOG_ENTRY_OPERATION) == 0) {
        operation_entries++;
      } else if (strcmp(type, LOG_ENTRY_AUDIT) == 0) {
        audit_entries++;
      }
    }
    
    json_object_set(stats, "total_entries", json_create_integer(total_entries));
    json_object_set(stats, "state_entries", json_create_integer(state_entries));
    json_object_set(stats, "operation_entries", json_create_integer(operation_entries));
    json_object_set(stats, "audit_entries", json_create_integer(audit_entries));
    
    fclose(file);
  }
  
  pthread_mutex_unlock(&log->lock);
  
  return stats;
}