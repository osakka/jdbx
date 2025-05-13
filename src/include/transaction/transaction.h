#ifndef TRANSACTION_H
#define TRANSACTION_H

#include "database/database.h"
#include "utils/json.h"
#include "core/server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

/* Forward declaration for api_context_t to break circular dependency */
typedef struct api_context api_context_t;

/* Forward declarations */
typedef struct transaction transaction_t;
typedef struct transaction_manager transaction_manager_t;
typedef struct transaction_operation transaction_operation_t;
typedef struct transaction_log transaction_log_t;
typedef struct lock_manager lock_manager_t;

/* Transaction state */
typedef enum {
    TRANSACTION_ACTIVE,      /* Transaction is active and can perform operations */
    TRANSACTION_COMMITTING,  /* Transaction is in the process of committing */
    TRANSACTION_COMMITTED,   /* Transaction has been committed successfully */
    TRANSACTION_ABORTING,    /* Transaction is in the process of aborting */
    TRANSACTION_ABORTED      /* Transaction has been aborted */
} transaction_state_t;

/* Isolation level */
typedef enum {
    ISOLATION_INVALID = -1,     /* Invalid isolation level */
    ISOLATION_READ_UNCOMMITTED, /* Lowest isolation level, can see uncommitted changes */
    ISOLATION_READ_COMMITTED,   /* Can only see committed changes */
    ISOLATION_SERIALIZABLE      /* Highest isolation level, complete transaction isolation */
} isolation_level_t;

/* Operation type */
typedef enum {
    OPERATION_INSERT,  /* Insert a new document */
    OPERATION_UPDATE,  /* Update an existing document */
    OPERATION_DELETE   /* Delete an existing document */
} operation_type_t;

/* Transaction operation */
struct transaction_operation {
    operation_type_t type;           /* Type of operation */
    char* collection_name;           /* Collection name */
    char* document_id;               /* Document ID (if applicable) */
    json_value_t* before_state;      /* Document state before operation */
    json_value_t* after_state;       /* Document state after operation */
    transaction_operation_t* next;   /* Next operation in the list */
};

/* Savepoint */
typedef struct savepoint savepoint_t;
struct savepoint {
    char* name;                      /* Savepoint name */
    transaction_operation_t* operation; /* Pointer to the operation at this savepoint */
    int operation_count;             /* Number of operations at this savepoint */
    savepoint_t* next;               /* Next savepoint in the list */
};

/* Transaction */
struct transaction {
    char* id;                        /* Transaction ID */
    transaction_state_t state;       /* Current state */
    time_t start_time;               /* Start timestamp */
    time_t commit_time;              /* Commit timestamp (0 if not committed) */
    char* user_id;                   /* User who initiated the transaction */
    transaction_operation_t* operations; /* List of operations */
    int operation_count;             /* Number of operations */
    isolation_level_t isolation_level; /* Isolation level */
    int timeout_sec;                 /* Transaction timeout in seconds */
    savepoint_t* savepoints;         /* List of savepoints */
    pthread_mutex_t lock;            /* Transaction lock */
};

/* Transaction hash table entry */
typedef struct transaction_hash_entry transaction_hash_entry_t;
struct transaction_hash_entry {
    transaction_t* transaction;        /* Pointer to transaction */
    transaction_hash_entry_t* next;    /* Next entry in hash chain */
};

/* Transaction manager */
struct transaction_manager {
    transaction_t** active_transactions; /* Array of active transactions */
    int capacity;                    /* Maximum number of concurrent transactions */
    int count;                       /* Current number of active transactions */
    pthread_mutex_t lock;            /* Manager lock */
    transaction_log_t* log;          /* Transaction log */
    lock_manager_t* lock_manager;    /* Lock manager */
    database_t* db;                  /* Reference to the database */
    transaction_hash_entry_t** tx_hash_table; /* Hash table for O(1) transaction lookup */
    int tx_hash_size;                /* Size of hash table */
};

/* Transaction log entry */
typedef struct {
    char* transaction_id;            /* Transaction ID */
    transaction_state_t state;       /* Transaction state */
    time_t timestamp;                /* Entry timestamp */
    operation_type_t operation_type; /* Operation type */
    char* collection_name;           /* Collection name */
    char* document_id;               /* Document ID */
    char* before_state;              /* JSON string of before state */
    char* after_state;               /* JSON string of after state */
    char* user_id;                   /* User who performed the operation */
    char* client_ip;                 /* Client IP address */
    char* application_name;          /* Application name */
    isolation_level_t isolation_level; /* Isolation level */
    char* savepoint_name;            /* Savepoint name, if applicable */
    int is_retry;                    /* Whether this is a retry attempt */
    int retry_count;                 /* Number of retry attempts */
    int error_code;                  /* Error code, if applicable */
    int duration_ms;                 /* Operation duration in milliseconds */
} transaction_log_entry_t;

/* Transaction log entry cache */
typedef struct cache_entry {
    time_t timestamp;                /* Entry timestamp */
    char* type;                      /* Entry type (STATE or OPERATION) */
    char* transaction_id;            /* Transaction ID */
    transaction_state_t state;       /* Transaction state (for STATE entries) */
    operation_type_t op_type;        /* Operation type (for OPERATION entries) */
    char* collection;                /* Collection name (for OPERATION entries) */
    char* document_id;               /* Document ID (for OPERATION entries) */
    char* user_id;                   /* User ID */
    struct cache_entry* next;        /* Next entry in cache */
} cache_entry_t;

/* Transaction log */
struct transaction_log {
    char* log_file;                  /* Path to log file */
    char* audit_file;                /* Path to audit file */
    int log_level;                   /* Logging level (0=off, 1=basic, 2=detailed) */
    int audit_enabled;               /* Whether audit logging is enabled */
    int auto_archive;                /* Whether to automatically archive old logs */
    int retention_days;              /* Number of days to retain logs */
    pthread_mutex_t lock;            /* Log lock */

    /* Cache for recent log entries */
    cache_entry_t** entry_cache;     /* Hash table for recent log entries */
    int cache_size;                  /* Size of the cache hash table */
    int max_cache_entries;           /* Maximum number of entries in cache */
    int cache_entry_count;           /* Current number of entries in cache */
    time_t cache_expiry_seconds;     /* How long entries stay in cache */

    /* In-memory metrics for fast access */
    int64_t total_transactions;      /* Total transactions seen */
    int64_t committed_transactions;  /* Total committed transactions */
    int64_t aborted_transactions;    /* Total aborted transactions */
    int64_t active_transactions;     /* Current active transactions */
    int64_t total_operations;        /* Total operations processed */
    int64_t avg_duration_ms;         /* Average transaction duration in ms */
    time_t last_metrics_update;      /* When metrics were last updated */
};

/* Lock manager */
struct lock_manager {
    /* Lock tracking structures would go here */
    pthread_mutex_t lock;            /* Lock manager lock */
};

/* Create a transaction manager */
transaction_manager_t* transaction_manager_create(database_t* db, int capacity);

/* Free a transaction manager */
void transaction_manager_free(transaction_manager_t* manager);

/* Begin a new transaction */
transaction_t* transaction_begin(transaction_manager_t* manager, isolation_level_t isolation_level, const char* user_id);

/* Commit a transaction */
int transaction_commit(transaction_manager_t* manager, transaction_t* transaction);

/* Rollback a transaction */
int transaction_rollback(transaction_manager_t* manager, transaction_t* transaction);

/* Insert a document within a transaction */
int transaction_insert_document(transaction_manager_t* manager, transaction_t* transaction, 
                              const char* collection, json_value_t* document);

/* Update a document within a transaction */
int transaction_update_document(transaction_manager_t* manager, transaction_t* transaction, 
                               const char* collection, const char* id, json_value_t* document);

/* Delete a document within a transaction */
int transaction_delete_document(transaction_manager_t* manager, transaction_t* transaction, 
                               const char* collection, const char* id);

/* Query documents within a transaction */
json_value_t* transaction_query_documents(transaction_manager_t* manager, transaction_t* transaction, 
                                        const char* collection, json_value_t* query);

/* Get transaction by ID */
transaction_t* transaction_manager_get_transaction(transaction_manager_t* manager, const char* transaction_id);

/* Serialize transaction to JSON */
json_value_t* transaction_to_json(transaction_t* transaction);

/* Get transaction state as string */
const char* transaction_state_to_string(transaction_state_t state);

/* Get isolation level as string */
const char* isolation_level_to_string(isolation_level_t level);

/* Parse isolation level from string */
isolation_level_t isolation_level_from_string(const char* level_str);

/* Set transaction timeout in seconds */
int transaction_set_timeout(transaction_t* transaction, int timeout_sec);

/* Check for timed out transactions and abort them */
int transaction_manager_check_timeouts(transaction_manager_t* manager);

/* Create a savepoint within a transaction */
int transaction_create_savepoint(transaction_t* transaction, const char* savepoint_name);

/* Rollback to a savepoint within a transaction */
int transaction_rollback_to_savepoint(transaction_manager_t* manager, transaction_t* transaction, const char* savepoint_name);

/* Release a savepoint within a transaction */
int transaction_release_savepoint(transaction_t* transaction, const char* savepoint_name);

/* Get transaction status as JSON */
json_value_t* transaction_get_status(transaction_t* transaction);

/* Get transaction metrics as JSON */
json_value_t* transaction_manager_get_metrics(transaction_manager_t* manager);

/* Check for and resolve deadlocks */
int transaction_manager_check_deadlocks(transaction_manager_t* manager);

/* Create a transaction log */
transaction_log_t* transaction_log_create(const char* log_file);

/* Create an advanced transaction log with audit capabilities */
transaction_log_t* transaction_log_create_advanced(const char* log_file, const char* audit_file, 
                                                 int log_level, int audit_enabled);

/* Free a transaction log */
void transaction_log_free(transaction_log_t* log);

/* Write a transaction operation to the log */
int transaction_log_write_operation(transaction_log_t* log, transaction_t* transaction, 
                                  transaction_operation_t* operation);

/* Write a transaction state change to the log */
int transaction_log_write_state_change(transaction_log_t* log, transaction_t* transaction);

/* Write a detailed audit entry */
int transaction_log_write_audit(transaction_log_t* log, transaction_t* transaction,
                              const char* client_ip, const char* application_name,
                              int is_retry, int retry_count, int error_code, int duration_ms);

/* Read operations from the log for recovery */
int transaction_log_read_operations(transaction_log_t* log, transaction_manager_t* manager);

/* Archive logs older than retention_days */
int transaction_log_archive(transaction_log_t* log, const char* archive_dir);

/* Configure transaction log settings */
int transaction_log_configure(transaction_log_t* log, int log_level, int audit_enabled,
                            int auto_archive, int retention_days);

/* Get JSON report of transaction logs for a time period */
json_value_t* transaction_log_get_report(transaction_log_t* log, time_t start_time, time_t end_time);

/* Get transaction history for a specific document */
json_value_t* transaction_log_get_document_history(transaction_log_t* log, 
                                                const char* collection, const char* document_id);

/* Create a lock manager */
lock_manager_t* lock_manager_create(void);

/* Free a lock manager */
void lock_manager_free(lock_manager_t* manager);

/* Acquire a lock on a document */
int lock_manager_lock_document(lock_manager_t* manager, const char* collection, 
                             const char* document_id, transaction_t* transaction);

/* Release a lock on a document */
int lock_manager_unlock_document(lock_manager_t* manager, const char* collection, 
                               const char* document_id, transaction_t* transaction);

/* Check for deadlocks */
int lock_manager_check_deadlocks(lock_manager_t* manager);

/* Get lock manager statistics */
json_value_t* lock_manager_get_stats(lock_manager_t* manager);

/* Reset deadlock statistics */
int lock_manager_reset_deadlock_stats(lock_manager_t* manager);

/* Acquire a shared lock on a document */
int lock_manager_lock_document_shared(lock_manager_t* manager, const char* collection,
                                   const char* document_id, transaction_t* transaction);

/* API handler for beginning a transaction */
http_response_t* api_handle_transaction_begin(api_context_t* ctx, http_request_t* request);

/* API handler for committing a transaction */
http_response_t* api_handle_transaction_commit(api_context_t* ctx, http_request_t* request);

/* API handler for rolling back a transaction */
http_response_t* api_handle_transaction_rollback(api_context_t* ctx, http_request_t* request);

/* API handler for document operations within a transaction */
http_response_t* api_handle_transaction_document_operation(api_context_t* ctx, http_request_t* request);

/* API handler for setting transaction isolation level */
http_response_t* api_handle_transaction_set_isolation(api_context_t* ctx, http_request_t* request);

/* API handler for setting transaction timeout */
http_response_t* api_handle_transaction_set_timeout(api_context_t* ctx, http_request_t* request);

/* API handler for creating a savepoint */
http_response_t* api_handle_transaction_create_savepoint(api_context_t* ctx, http_request_t* request);

/* API handler for rolling back to a savepoint */
http_response_t* api_handle_transaction_rollback_to_savepoint(api_context_t* ctx, http_request_t* request);

/* API handler for releasing a savepoint */
http_response_t* api_handle_transaction_release_savepoint(api_context_t* ctx, http_request_t* request);

/* API handler for getting transaction status */
http_response_t* api_handle_transaction_status(api_context_t* ctx, http_request_t* request);

/* API handler for getting transaction metrics */
http_response_t* api_handle_transaction_metrics(api_context_t* ctx, http_request_t* request);

/* API handler for checking deadlocks */
http_response_t* api_handle_transaction_check_deadlocks(api_context_t* ctx, http_request_t* request);

/* Visualization data format types */
typedef enum {
    VISUALIZATION_FORMAT_DEFAULT,       /* Default list of transactions/operations */
    VISUALIZATION_FORMAT_TIMELINE,      /* Time-series optimized format */
    VISUALIZATION_FORMAT_LIFECYCLE,     /* Transaction lifecycle data */
    VISUALIZATION_FORMAT_HEATMAP,       /* Resource contention heatmap data */
    VISUALIZATION_FORMAT_DISTRIBUTION,  /* Performance distribution statistics */
    VISUALIZATION_FORMAT_DEPENDENCY,    /* Transaction dependency graph */
    VISUALIZATION_FORMAT_SANKEY         /* Sankey diagram flow data */
} visualization_format_t;

/* Get transaction history data for visualization */
json_value_t* transaction_get_history_data(transaction_manager_t* manager, time_t start_time, time_t end_time,
                                       const char* user_id, int limit);

/* Get transaction history data with enhanced visualization formats */
json_value_t* transaction_get_history_data_enhanced(transaction_manager_t* manager, time_t start_time, time_t end_time,
                                               const char* user_id, int limit, visualization_format_t format);

/* Get transaction performance metrics for visualization */
json_value_t* transaction_get_performance_metrics(transaction_manager_t* manager,
                                              const char* dimension, time_t start_time, time_t end_time);

/* Get transaction relationship data for graph visualization */
json_value_t* transaction_get_relationship_data(transaction_manager_t* manager, time_t start_time, time_t end_time,
                                           const char* collection, const char* document_id);

/* Graph export formats */
typedef enum {
    EXPORT_FORMAT_JSON,    /* Default JSON format */
    EXPORT_FORMAT_DOT,     /* GraphViz DOT format */
    EXPORT_FORMAT_GRAPHML, /* GraphML XML format */
    EXPORT_FORMAT_CYTOSCAPE, /* Cytoscape JSON format */
    EXPORT_FORMAT_D3       /* D3.js compatible JSON format */
} graph_export_format_t;

/* Export transaction graph in various formats */
char* transaction_graph_export(json_value_t* graph_data, graph_export_format_t format);

/* API handler for transaction graph export */
http_response_t* api_handle_visualization_transaction_export(api_context_t* ctx, http_request_t* request);

#endif /* TRANSACTION_H */