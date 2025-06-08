#ifndef INDEX_METRICS_H
#define INDEX_METRICS_H

#include <stdint.h>
#include <time.h>
#include "utils/json.h"
#include "utils/metrics.h"

/**
 * @file index_metrics.h
 * @brief Performance metrics tracking for adaptive indexes
 * 
 * This system provides comprehensive metrics for monitoring index performance,
 * including query improvements, storage overhead, and maintenance costs.
 */

/* Index performance metrics structure */
typedef struct index_performance_metrics {
    /* Index identification */
    char collection_name[256];
    char field_path[256];
    char index_name[256];
    
    /* Query performance metrics */
    uint64_t queries_using_index;        /* Number of queries that used this index */
    uint64_t queries_without_index;      /* Historical queries before index creation */
    double avg_query_time_with_index_ms; /* Average query time with index */
    double avg_query_time_without_index_ms; /* Historical avg without index */
    double query_speedup_factor;         /* How much faster queries are with index */
    
    /* Index usage patterns */
    uint64_t index_lookups;              /* Total number of index lookups */
    uint64_t index_hits;                 /* Successful index hits */
    uint64_t index_misses;               /* Index lookups that returned no results */
    double hit_rate;                     /* Percentage of successful hits */
    
    /* Storage metrics */
    uint64_t index_size_bytes;           /* Current size of the index */
    uint64_t indexed_documents;          /* Number of documents in the index */
    double bytes_per_document;           /* Average storage per document */
    
    /* Maintenance metrics */
    uint64_t insert_operations;          /* Number of insert operations */
    uint64_t update_operations;          /* Number of update operations */
    uint64_t delete_operations;          /* Number of delete operations */
    double avg_insert_time_ms;           /* Average time to insert into index */
    double avg_update_time_ms;           /* Average time to update index */
    double avg_delete_time_ms;           /* Average time to delete from index */
    uint64_t maintenance_errors;         /* Number of maintenance failures */
    
    /* Effectiveness metrics */
    time_t created_at;                   /* When the index was created */
    time_t last_used_at;                 /* Last time the index was used */
    uint64_t days_since_last_use;        /* Days since last query used this index */
    double effectiveness_score;          /* Overall effectiveness score (0-100) */
    int is_recommended_for_removal;      /* Whether this index should be removed */
    
    /* Cost-benefit analysis */
    double query_time_saved_ms;          /* Total query time saved by this index */
    double maintenance_time_spent_ms;    /* Total time spent maintaining index */
    double net_benefit_ms;               /* Net time saved (query - maintenance) */
    double roi_percentage;               /* Return on investment percentage */
} index_performance_metrics_t;

/* Index metrics collection state */
typedef struct index_metrics_collector {
    index_performance_metrics_t* metrics_array;
    size_t metrics_count;
    size_t metrics_capacity;
    pthread_mutex_t metrics_lock;
    
    /* Global statistics */
    uint64_t total_indexes_tracked;
    uint64_t total_queries_optimized;
    double total_time_saved_ms;
    double total_storage_used_mb;
    
    /* Metric collection intervals */
    time_t last_collection_time;
    int collection_interval_seconds;
    
    /* Metrics persistence */
    char* metrics_file_path;
    int auto_persist_enabled;
} index_metrics_collector_t;

/* === Index Metrics Functions === */

/**
 * Initialize the index metrics collection system
 * @return 0 on success, -1 on error
 */
int index_metrics_init(void);

/**
 * Get the global metrics collector instance
 * @return Pointer to the global metrics collector or NULL if not initialized
 */
index_metrics_collector_t* index_metrics_get_collector(void);

/**
 * Cleanup the index metrics collection system
 */
void index_metrics_cleanup(void);

/**
 * Start tracking metrics for a new index
 * @param collection_name The collection name
 * @param field_path The indexed field path
 * @param index_name The index name
 * @return 0 on success, -1 on error
 */
int index_metrics_start_tracking(const char* collection_name,
                                const char* field_path,
                                const char* index_name);

/**
 * Stop tracking metrics for an index
 * @param collection_name The collection name
 * @param field_path The indexed field path
 * @return 0 on success, -1 on error
 */
int index_metrics_stop_tracking(const char* collection_name,
                               const char* field_path);

/**
 * Record a query that used an index
 * @param collection_name The collection name
 * @param field_path The indexed field path
 * @param query_time_ms The query execution time in milliseconds
 * @param documents_returned Number of documents returned
 * @return 0 on success, -1 on error
 */
int index_metrics_record_query(const char* collection_name,
                              const char* field_path,
                              double query_time_ms,
                              uint64_t documents_returned);

/**
 * Record an index maintenance operation
 * @param collection_name The collection name
 * @param field_path The indexed field path
 * @param operation_type Type of operation (insert/update/delete)
 * @param operation_time_ms Time taken for the operation
 * @param success Whether the operation succeeded
 * @return 0 on success, -1 on error
 */
int index_metrics_record_maintenance(const char* collection_name,
                                   const char* field_path,
                                   const char* operation_type,
                                   double operation_time_ms,
                                   int success);

/**
 * Update storage metrics for an index
 * @param collection_name The collection name
 * @param field_path The indexed field path
 * @param index_size_bytes Current size of the index
 * @param document_count Number of documents indexed
 * @return 0 on success, -1 on error
 */
int index_metrics_update_storage(const char* collection_name,
                               const char* field_path,
                               uint64_t index_size_bytes,
                               uint64_t document_count);

/**
 * Get performance metrics for a specific index
 * @param collection_name The collection name
 * @param field_path The indexed field path
 * @return Pointer to metrics structure or NULL if not found
 */
index_performance_metrics_t* index_metrics_get(const char* collection_name,
                                              const char* field_path);

/**
 * Get all index metrics as a JSON object
 * @return JSON object containing all metrics
 */
json_value_t* index_metrics_get_all_json(void);

/**
 * Get metrics summary for a collection
 * @param collection_name The collection name
 * @return JSON object with collection-level metrics
 */
json_value_t* index_metrics_get_collection_summary(const char* collection_name);

/**
 * Calculate effectiveness score for an index
 * @param metrics The index metrics
 * @return Effectiveness score (0-100)
 */
double index_metrics_calculate_effectiveness(const index_performance_metrics_t* metrics);

/**
 * Get indexes recommended for removal
 * @param threshold_days Days of inactivity threshold
 * @param min_effectiveness Minimum effectiveness score
 * @return JSON array of indexes to remove
 */
json_value_t* index_metrics_get_removal_candidates(uint64_t threshold_days,
                                                  double min_effectiveness);

/**
 * Export metrics to file
 * @param file_path Path to export file
 * @return 0 on success, -1 on error
 */
int index_metrics_export(const char* file_path);

/**
 * Import metrics from file
 * @param file_path Path to import file
 * @return 0 on success, -1 on error
 */
int index_metrics_import(const char* file_path);

/**
 * Enable automatic metrics persistence
 * @param file_path Path to metrics file
 * @param interval_seconds Save interval in seconds
 * @return 0 on success, -1 on error
 */
int index_metrics_enable_persistence(const char* file_path, int interval_seconds);

/**
 * Get index effectiveness report
 * @return JSON object with detailed effectiveness analysis
 */
json_value_t* index_metrics_effectiveness_report(void);

/**
 * Reset metrics for an index
 * @param collection_name The collection name
 * @param field_path The indexed field path
 * @return 0 on success, -1 on error
 */
int index_metrics_reset(const char* collection_name, const char* field_path);

/**
 * Register metrics with the global metrics registry
 * @return 0 on success, -1 on error
 */
int index_metrics_register_prometheus_metrics(void);

/* === Helper Functions === */

/**
 * Find metrics entry for an index
 * @param collection_name The collection name
 * @param field_path The indexed field path
 * @return Pointer to metrics or NULL if not found
 */
index_performance_metrics_t* index_metrics_find(const char* collection_name,
                                               const char* field_path);

/**
 * Format bytes to human-readable string
 * @param bytes Number of bytes
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 */
void index_metrics_format_bytes(uint64_t bytes, char* buffer, size_t buffer_size);

/**
 * Calculate ROI percentage for an index
 * @param time_saved_ms Total query time saved
 * @param time_spent_ms Total maintenance time spent
 * @return ROI percentage
 */
double index_metrics_calculate_roi(double time_saved_ms, double time_spent_ms);

/**
 * Remove metrics for a specific index
 * @param metrics The metrics system
 * @param collection_name The collection name
 * @param field_path The indexed field path
 * @return 0 on success, -1 on error
 */
int index_metrics_remove(struct index_metrics_collector* metrics,
                        const char* collection_name,
                        const char* field_path);

#endif /* INDEX_METRICS_H */