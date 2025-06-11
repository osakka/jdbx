#ifndef LIBRARY_METRICS_H
#define LIBRARY_METRICS_H

#include "database/database.h"
#include "utils/json.h"

/**
 * Initialize metrics collection for a library
 * @param db Database instance
 * @param library_name Library name (NULL for default)
 * @return 1 on success, 0 on failure
 */
int library_metrics_init(database_t* db, const char* library_name);

/**
 * Record a library-specific metric
 * @param db Database instance
 * @param library_name Library name (NULL for default)
 * @param metric_type Type of metric (operations, performance, etc.)
 * @param metric_data JSON object with metric data
 * @return 1 on success, 0 on failure
 */
int library_metrics_record(database_t* db, const char* library_name, 
                          const char* metric_type, json_value_t* metric_data);

/**
 * Query library metrics
 * @param db Database instance
 * @param library_name Library name (NULL for default)
 * @param metric_type Type of metric to query (NULL for all)
 * @param limit Maximum number of results (0 for no limit)
 * @return JSON array of metric documents
 */
json_value_t* library_metrics_query(database_t* db, const char* library_name,
                                   const char* metric_type, int limit);

/**
 * Get aggregated library metrics (latest of each type)
 * @param db Database instance
 * @param library_name Library name (NULL for default)
 * @return JSON object with aggregated metrics
 */
json_value_t* library_metrics_aggregate(database_t* db, const char* library_name);

/**
 * Clean up old library metrics
 * @param db Database instance
 * @param library_name Library name (NULL for default)
 * @param retention_days Number of days to retain metrics
 * @return Number of metrics deleted
 */
int library_metrics_cleanup(database_t* db, const char* library_name, int retention_days);

#endif /* LIBRARY_METRICS_H */