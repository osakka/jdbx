#ifndef ADAPTIVE_INDEXER_H
#define ADAPTIVE_INDEXER_H

#include "database/database.h"
#include "database/query_tracker.h"
#include <pthread.h>
#include <time.h>

/**
 * @file adaptive_indexer.h
 * @brief Adaptive index management for automatic performance optimization
 * 
 * This system monitors query patterns from the query tracker and automatically
 * creates, maintains, and removes indexes for optimal performance.
 */

/* Adaptive indexer configuration */
#define ADAPTIVE_INDEX_CHECK_INTERVAL 60    /* Check for new indexes every 60 seconds */
#define ADAPTIVE_INDEX_MIN_DOCUMENTS 10     /* Only index collections with 10+ documents */
#define ADAPTIVE_INDEX_MAX_INDEXES_PER_COLLECTION 10  /* Limit indexes per collection */

/* Index creation status */
typedef enum index_creation_status {
    INDEX_CREATION_SUCCESS,
    INDEX_CREATION_EXISTS,
    INDEX_CREATION_ERROR,
    INDEX_CREATION_SKIPPED_LOW_DOCS,
    INDEX_CREATION_SKIPPED_TOO_MANY
} index_creation_status_t;

/* Adaptive index information */
typedef struct adaptive_index_info {
    char* collection_name;           /* Collection name */
    char* field_path;               /* Field path being indexed */
    char* index_name;               /* Generated index name */
    time_t created_at;              /* When the index was created */
    uint64_t queries_before_index;  /* Number of queries before index creation */
    double avg_time_before_ms;      /* Average query time before indexing */
    double avg_time_after_ms;       /* Average query time after indexing */
    int is_effective;               /* Whether the index improved performance */
    struct adaptive_index_info* next; /* Next in list */
} adaptive_index_info_t;

/* Adaptive indexer structure */
typedef struct adaptive_indexer {
    database_t* database;           /* Database reference */
    adaptive_index_info_t* indexes; /* List of created adaptive indexes */
    pthread_t indexer_thread;      /* Background indexer thread */
    pthread_mutex_t indexes_lock;   /* Lock for indexes list */
    int should_stop;                /* Flag to stop the indexer thread */
    time_t last_check;              /* Last time we checked for new indexes */
    uint64_t indexes_created;       /* Total indexes created */
    uint64_t indexes_skipped;       /* Total indexes skipped */
} adaptive_indexer_t;

/* === Adaptive Indexer Functions === */

/**
 * Initialize the adaptive indexer
 */
int adaptive_indexer_init(database_t* database);

/**
 * Cleanup the adaptive indexer
 */
void adaptive_indexer_cleanup(void);

/**
 * Start the background indexer thread
 */
int adaptive_indexer_start(void);

/**
 * Stop the background indexer thread
 */
void adaptive_indexer_stop(void);

/**
 * Force an immediate check for new indexes to create
 */
void adaptive_indexer_force_check(void);

/**
 * Get statistics about adaptive indexing
 */
json_value_t* adaptive_indexer_get_stats(void);

/**
 * Check if a field should be indexed based on query patterns
 */
int should_create_index(const char* collection_name, const char* field_path, 
                       uint64_t query_count, double avg_time_ms);

/**
 * Create an adaptive index for a specific field
 */
index_creation_status_t create_adaptive_index(const char* collection_name, 
                                             const char* field_path,
                                             const char* reason);

/**
 * Generate an appropriate index name for a field
 */
char* generate_adaptive_index_name(const char* collection_name, const char* field_path);

/**
 * Check collection document count for indexing threshold
 */
size_t get_collection_document_count(database_t* db, const char* collection_name);

/**
 * Background thread function for adaptive indexing
 */
void* adaptive_indexer_thread_func(void* arg);

/**
 * Process query tracker candidates and create indexes
 */
void process_index_candidates(void);

/**
 * Evaluate index effectiveness after creation
 */
void evaluate_index_effectiveness(adaptive_index_info_t* index_info);

/* === Index Management Helpers === */

/**
 * Check if an index already exists for a field
 */
int index_exists_for_field(database_t* db, const char* collection_name, const char* field_path);

/**
 * Get index count for a collection
 */
size_t get_collection_index_count(database_t* db, const char* collection_name);

/**
 * Log index creation activity
 */
void log_index_creation(const char* collection_name, const char* field_path, 
                       index_creation_status_t status, const char* reason);

#endif /* ADAPTIVE_INDEXER_H */