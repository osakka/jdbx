#ifndef DATABASE_H
#define DATABASE_H

#include "utils/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <regex.h>
#include <time.h>

/* Persistence configuration constants */
#define PERSISTENCE_BUFFER_OPS_THRESHOLD 50      /* Save after 50 operations */
#define PERSISTENCE_BUFFER_SIZE_THRESHOLD (1024 * 1024)  /* Save after 1MB of changes */
#define PERSISTENCE_PERIODIC_SAVE_INTERVAL 30    /* Save every 30 seconds */

/* Schema validation rule types */
typedef enum {
    SCHEMA_TYPE_CHECK,      /* Field must be of specific type */
    SCHEMA_REQUIRED,        /* Field is required */
    SCHEMA_MIN_LENGTH,      /* Minimum string length */
    SCHEMA_MAX_LENGTH,      /* Maximum string length */
    SCHEMA_MIN_VALUE,       /* Minimum numeric value */
    SCHEMA_MAX_VALUE,       /* Maximum numeric value */
    SCHEMA_PATTERN,         /* Regular expression pattern */
    SCHEMA_ENUM,            /* Value must be in enum list */
    SCHEMA_NESTED           /* Nested schema for objects */
} schema_rule_type_t;

/* Schema validation rule */
typedef struct schema_rule {
    schema_rule_type_t type;
    char* field_path;          /* JSON path to the field (dot notation) */
    union {
        int type_value;        /* For SCHEMA_TYPE_CHECK */
        int required;          /* For SCHEMA_REQUIRED */
        int min_length;        /* For SCHEMA_MIN_LENGTH */
        int max_length;        /* For SCHEMA_MAX_LENGTH */
        double min_value;      /* For SCHEMA_MIN_VALUE */
        double max_value;      /* For SCHEMA_MAX_VALUE */
        char* pattern;         /* For SCHEMA_PATTERN */
        json_value_t* enum_values; /* For SCHEMA_ENUM (array) */
        struct schema* nested; /* For SCHEMA_NESTED */
    } params;
    regex_t regex;             /* Compiled regex for SCHEMA_PATTERN */
    int regex_compiled;        /* Flag indicating if regex is compiled */
    struct schema_rule* next;  /* Next rule in linked list */
} schema_rule_t;

/* Schema definition */
typedef struct schema {
    char* name;                /* Schema name */
    char* description;         /* Schema description */
    schema_rule_t* rules;      /* Linked list of rules */
} schema_t;

/* Schema validation result */
typedef struct {
    int is_valid;              /* 1 if valid, 0 if invalid */
    char* error_field;         /* Field that failed validation */
    char* error_message;       /* Error message */
} schema_validation_result_t;

/* Index types */
typedef enum {
    INDEX_TYPE_UNIQUE,         /* Unique index (no duplicates) */
    INDEX_TYPE_NON_UNIQUE,     /* Non-unique index (allows duplicates) */
    INDEX_TYPE_TEXT,           /* Text index for text search */
    INDEX_TYPE_GEO             /* Geospatial index */
} index_type_t;

/* Index entry for a single document */
typedef struct index_entry {
    char* document_id;         /* Document ID */
    char* key_value;           /* Indexed field value (as string) */
    struct index_entry* next;  /* Next entry in linked list */
} index_entry_t;

/* Index structure */
typedef struct index {
    char* name;                /* Index name */
    char* field_path;          /* JSON path to the indexed field */
    index_type_t type;         /* Index type */
    index_entry_t** buckets;   /* Hash buckets (for hash-based indexes) */
    size_t num_buckets;        /* Number of hash buckets */
    size_t entries;            /* Number of index entries */
    pthread_rwlock_t lock;     /* Read-write lock for thread safety */
    struct index* next;        /* Next index in linked list */
} index_t;

/* Database collection */
typedef struct {
    char* name;
    json_value_t* documents;   /* JSON array of documents */
    pthread_mutex_t lock;      /* Collection lock for thread safety */
    schema_t* schema;          /* Optional schema for validation */
    index_t* indexes;          /* List of indexes for the collection */
} db_collection_t;

/* Forward declaration for cache */
typedef struct cache cache_t;

/* Forward declaration for transaction_manager */
typedef struct transaction_manager transaction_manager_t;

/* Persistence thread structure */
typedef struct {
    pthread_t thread;              /* Persistence thread handle */
    pthread_mutex_t mutex;         /* Mutex for thread synchronization */
    pthread_cond_t condition;      /* Condition variable for notifications */
    int shutdown;                  /* Shutdown flag */
    int operations_count;          /* Buffer: number of pending operations */
    size_t data_size_estimate;     /* Buffer: estimated data size in bytes */
    time_t last_save_time;         /* Timestamp of last save operation */
    int save_in_progress;          /* Flag indicating save is in progress */
    int last_save_failed;          /* Flag indicating if last save failed */
    char last_error_message[256];  /* Last error message */
    time_t last_error_time;        /* Timestamp of last error */
} persistence_thread_t;

/* Database structure */
typedef struct database {
    char* path;                    /* Path to database file */
    json_value_t* collections;     /* JSON object of collections */
    pthread_rwlock_t rwlock;       /* Read-write lock for concurrent access */
    int is_modified;               /* Flag to track if database is modified */
    cache_t* cache;                /* Document cache */
    int cache_enabled;             /* Flag indicating if caching is enabled */
    transaction_manager_t* transaction_manager; /* Transaction manager */
    persistence_thread_t* persistence; /* Persistence thread management */
    int is_bootstrap_mode;         /* Flag for bootstrap initialization mode */
} database_t;

/* Database function prototypes */
database_t* db_init(const char* path);
void db_close(database_t* db);
int db_save(database_t* db);

/* Persistence thread management */
int db_start_persistence_thread(database_t* db);
void db_stop_persistence_thread(database_t* db);
void db_notify_data_change(database_t* db, size_t estimated_size);
int db_notify_data_change_sync(database_t* db, size_t estimated_size);
int db_force_save(database_t* db);
int db_check_persistence_errors(database_t* db, char* error_buffer, size_t buffer_size);
int db_load(database_t* db);

/* Cache management */
int db_enable_cache(database_t* db, int capacity, int ttl);
int db_disable_cache(database_t* db);
int db_configure_cache(database_t* db, int capacity, int ttl, const char* type, double max_memory_mb);
json_value_t* db_get_cache_stats(database_t* db);
int db_clear_cache(database_t* db);

/* Collection operations */
int db_create_collection(database_t* db, const char* name);
int db_drop_collection(database_t* db, const char* name);
db_collection_t* db_get_collection(database_t* db, const char* name);
json_value_t* db_list_collections(database_t* db);
json_value_t* db_list_collections_with_info(database_t* db);

/* Collection operations */
int db_collection_exists(database_t* db, const char* collection_name);
int db_create_collection(database_t* db, const char* collection_name);

/* Document operations */
json_value_t* db_insert_document(database_t* db, const char* collection, json_value_t* document);
json_value_t* db_get_document(database_t* db, const char* collection, const char* id);
json_value_t* db_update_document(database_t* db, const char* collection, const char* id, json_value_t* document);
int db_delete_document(database_t* db, const char* collection, const char* id);
json_value_t* db_query_documents(database_t* db, const char* collection, json_value_t* query);

/* Schema operations */
schema_t* db_create_schema(const char* name, const char* description);
void db_free_schema(schema_t* schema);
int db_add_schema_rule(schema_t* schema, schema_rule_t* rule);
schema_rule_t* db_create_schema_rule(schema_rule_type_t type, const char* field_path);
void db_free_schema_rule(schema_rule_t* rule);
int db_attach_schema(database_t* db, const char* collection, schema_t* schema);
int db_detach_schema(database_t* db, const char* collection);
schema_t* db_get_schema(database_t* db, const char* collection);
json_value_t* db_schema_to_json(schema_t* schema);
schema_t* db_schema_from_json(json_value_t* json);
schema_validation_result_t db_validate_document(schema_t* schema, json_value_t* document);
int db_validate_collection(database_t* db, const char* collection);

/* JSON Schema management */
int db_store_json_schema(database_t* db, const char* collection_name, json_value_t* schema);
json_value_t* db_get_json_schema(database_t* db, const char* collection_name);
int db_delete_json_schema(database_t* db, const char* collection_name);
json_value_t* db_list_json_schemas(database_t* db);
int db_validate_json_schema(json_value_t* schema, json_value_t* document, char** error_msg);

/* Index operations */
index_t* db_create_index(database_t* db, const char* collection, const char* name, 
                        const char* field_path, index_type_t type);
int db_drop_index(database_t* db, const char* collection, const char* name);
index_t* db_get_index(database_t* db, const char* collection, const char* name);
json_value_t* db_list_indexes(database_t* db, const char* collection);
int db_rebuild_index(database_t* db, const char* collection, const char* name);
int db_rebuild_all_indexes(database_t* db, const char* collection);
json_value_t* db_query_by_index(database_t* db, const char* collection, const char* field_path, 
                               const char* value, int limit, int skip);
void db_update_indexes_for_document(database_t* db, const char* collection, 
                                   const char* document_id, json_value_t* document);
void db_remove_document_from_indexes(database_t* db, const char* collection, 
                                    const char* document_id);
json_value_t* db_index_to_json(index_t* index);
json_value_t* db_index_stats(database_t* db, const char* collection, const char* name);

/**
 * Rebuild all document indices for improved performance
 * 
 * @param db Database instance
 * @return 1 on success, 0 on failure
 */
int db_rebuild_indices(database_t* db);

/* JSON Schema validation */
int json_schema_validate(json_value_t* schema, json_value_t* value, char** error_msg);

#endif /* DATABASE_H */