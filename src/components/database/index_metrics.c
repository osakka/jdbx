#include "database/index_metrics.h"
#include "utils/logger.h"
#include "utils/json_helpers.h"
#include "utils/buffer_pool.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
#include <stdio.h>

/* Global metrics collector */
static index_metrics_collector_t* g_metrics_collector = NULL;
static int g_metrics_initialized = 0;

/* Prometheus-style metrics for integration */
static metric_t* g_index_queries_total = NULL;
static metric_t* g_index_query_duration_seconds = NULL;
static metric_t* g_index_maintenance_duration_seconds = NULL;
static metric_t* g_index_size_bytes_gauge = NULL;
static metric_t* g_index_effectiveness_gauge = NULL;

/* === Helper Functions === */

/**
 * Get current timestamp in milliseconds
 */
static double get_timestamp_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)(tv.tv_sec * 1000) + (double)(tv.tv_usec / 1000.0);
}

/**
 * Calculate days between two timestamps
 */
static uint64_t days_between(time_t start, time_t end) {
    double diff = difftime(end, start);
    return (uint64_t)(diff / (24 * 60 * 60));
}

/**
 * Initialize a new metrics entry
 */
static void init_metrics_entry(index_performance_metrics_t* metrics,
                              const char* collection_name,
                              const char* field_path,
                              const char* index_name) {
    memset(metrics, 0, sizeof(index_performance_metrics_t));
    strncpy(metrics->collection_name, collection_name, sizeof(metrics->collection_name) - 1);
    strncpy(metrics->field_path, field_path, sizeof(metrics->field_path) - 1);
    strncpy(metrics->index_name, index_name, sizeof(metrics->index_name) - 1);
    metrics->created_at = time(NULL);
    metrics->last_used_at = metrics->created_at;
    metrics->effectiveness_score = 50.0; /* Start with neutral score */
}

/* === Public Functions === */

int index_metrics_init(void) {
    if (g_metrics_initialized) {
        return 0;
    }
    
    /* Allocate collector */
    g_metrics_collector = calloc(1, sizeof(index_metrics_collector_t));
    if (!g_metrics_collector) {
        LOG_ERROR("Failed to allocate index metrics collector");
        return -1;
    }
    
    /* Initialize collector */
    if (pthread_mutex_init(&g_metrics_collector->metrics_lock, NULL) != 0) {
        LOG_ERROR("Failed to initialize metrics lock");
        free(g_metrics_collector);
        g_metrics_collector = NULL;
        return -1;
    }
    
    g_metrics_collector->metrics_capacity = 100; /* Initial capacity */
    g_metrics_collector->metrics_array = calloc(g_metrics_collector->metrics_capacity, 
                                               sizeof(index_performance_metrics_t));
    if (!g_metrics_collector->metrics_array) {
        LOG_ERROR("Failed to allocate metrics array");
        pthread_mutex_destroy(&g_metrics_collector->metrics_lock);
        free(g_metrics_collector);
        g_metrics_collector = NULL;
        return -1;
    }
    
    g_metrics_collector->collection_interval_seconds = 60; /* Collect every minute */
    g_metrics_collector->last_collection_time = time(NULL);
    
    /* Register Prometheus metrics */
    index_metrics_register_prometheus_metrics();
    
    g_metrics_initialized = 1;
    LOG_INFO("Index metrics system initialized");
    
    return 0;
}

void index_metrics_cleanup(void) {
    if (!g_metrics_initialized || !g_metrics_collector) {
        return;
    }
    
    /* Save metrics if persistence is enabled */
    if (g_metrics_collector->auto_persist_enabled && g_metrics_collector->metrics_file_path) {
        index_metrics_export(g_metrics_collector->metrics_file_path);
    }
    
    /* Cleanup */
    pthread_mutex_destroy(&g_metrics_collector->metrics_lock);
    free(g_metrics_collector->metrics_array);
    free(g_metrics_collector->metrics_file_path);
    free(g_metrics_collector);
    g_metrics_collector = NULL;
    
    g_metrics_initialized = 0;
    LOG_INFO("Index metrics system cleaned up");
}

int index_metrics_start_tracking(const char* collection_name,
                                const char* field_path,
                                const char* index_name) {
    if (!g_metrics_initialized || !collection_name || !field_path || !index_name) {
        return -1;
    }
    
    pthread_mutex_lock(&g_metrics_collector->metrics_lock);
    
    /* Check if already tracking */
    for (size_t i = 0; i < g_metrics_collector->metrics_count; i++) {
        if (strcmp(g_metrics_collector->metrics_array[i].collection_name, collection_name) == 0 &&
            strcmp(g_metrics_collector->metrics_array[i].field_path, field_path) == 0) {
            pthread_mutex_unlock(&g_metrics_collector->metrics_lock);
            LOG_DEBUG("Already tracking metrics for index %s.%s", collection_name, field_path);
            return 0;
        }
    }
    
    /* Expand array if needed */
    if (g_metrics_collector->metrics_count >= g_metrics_collector->metrics_capacity) {
        size_t new_capacity = g_metrics_collector->metrics_capacity * 2;
        index_performance_metrics_t* new_array = realloc(g_metrics_collector->metrics_array,
                                                        new_capacity * sizeof(index_performance_metrics_t));
        if (!new_array) {
            pthread_mutex_unlock(&g_metrics_collector->metrics_lock);
            LOG_ERROR("Failed to expand metrics array");
            return -1;
        }
        g_metrics_collector->metrics_array = new_array;
        g_metrics_collector->metrics_capacity = new_capacity;
    }
    
    /* Add new metrics entry */
    index_performance_metrics_t* metrics = &g_metrics_collector->metrics_array[g_metrics_collector->metrics_count];
    init_metrics_entry(metrics, collection_name, field_path, index_name);
    g_metrics_collector->metrics_count++;
    g_metrics_collector->total_indexes_tracked++;
    
    pthread_mutex_unlock(&g_metrics_collector->metrics_lock);
    
    LOG_INFO("Started tracking metrics for index %s on %s.%s", index_name, collection_name, field_path);
    return 0;
}

int index_metrics_record_query(const char* collection_name,
                              const char* field_path,
                              double query_time_ms,
                              uint64_t documents_returned) {
    if (!g_metrics_initialized || !collection_name || !field_path) {
        return -1;
    }
    
    pthread_mutex_lock(&g_metrics_collector->metrics_lock);
    
    /* Find metrics entry */
    index_performance_metrics_t* metrics = NULL;
    for (size_t i = 0; i < g_metrics_collector->metrics_count; i++) {
        if (strcmp(g_metrics_collector->metrics_array[i].collection_name, collection_name) == 0 &&
            strcmp(g_metrics_collector->metrics_array[i].field_path, field_path) == 0) {
            metrics = &g_metrics_collector->metrics_array[i];
            break;
        }
    }
    
    if (!metrics) {
        pthread_mutex_unlock(&g_metrics_collector->metrics_lock);
        LOG_WARNING("No metrics tracking for index %s.%s", collection_name, field_path);
        return -1;
    }
    
    /* Update query metrics */
    metrics->queries_using_index++;
    metrics->index_lookups++;
    
    if (documents_returned > 0) {
        metrics->index_hits++;
    } else {
        metrics->index_misses++;
    }
    
    /* Update average query time */
    double total_query_time = metrics->avg_query_time_with_index_ms * (metrics->queries_using_index - 1);
    metrics->avg_query_time_with_index_ms = (total_query_time + query_time_ms) / metrics->queries_using_index;
    
    /* Update hit rate */
    if (metrics->index_lookups > 0) {
        metrics->hit_rate = (double)metrics->index_hits / metrics->index_lookups * 100.0;
    }
    
    /* Update last used timestamp */
    metrics->last_used_at = time(NULL);
    metrics->days_since_last_use = 0;
    
    /* Calculate query time saved (if we have historical data) */
    if (metrics->avg_query_time_without_index_ms > 0) {
        double time_saved = metrics->avg_query_time_without_index_ms - query_time_ms;
        if (time_saved > 0) {
            metrics->query_time_saved_ms += time_saved;
        }
    }
    
    /* Update global stats */
    g_metrics_collector->total_queries_optimized++;
    
    pthread_mutex_unlock(&g_metrics_collector->metrics_lock);
    
    /* Update Prometheus metrics */
    if (g_index_queries_total) {
        metrics_counter_inc(g_index_queries_total, 1);
    }
    if (g_index_query_duration_seconds) {
        metrics_histogram_observe(g_index_query_duration_seconds, query_time_ms / 1000.0);
    }
    
    LOG_TRACE("Recorded query for index %s.%s: %.2fms, %lu docs", 
              collection_name, field_path, query_time_ms, documents_returned);
    
    return 0;
}

int index_metrics_record_maintenance(const char* collection_name,
                                   const char* field_path,
                                   const char* operation_type,
                                   double operation_time_ms,
                                   int success) {
    if (!g_metrics_initialized || !collection_name || !field_path || !operation_type) {
        return -1;
    }
    
    pthread_mutex_lock(&g_metrics_collector->metrics_lock);
    
    /* Find metrics entry */
    index_performance_metrics_t* metrics = NULL;
    for (size_t i = 0; i < g_metrics_collector->metrics_count; i++) {
        if (strcmp(g_metrics_collector->metrics_array[i].collection_name, collection_name) == 0 &&
            strcmp(g_metrics_collector->metrics_array[i].field_path, field_path) == 0) {
            metrics = &g_metrics_collector->metrics_array[i];
            break;
        }
    }
    
    if (!metrics) {
        pthread_mutex_unlock(&g_metrics_collector->metrics_lock);
        return -1;
    }
    
    /* Update operation counts and times */
    if (strcmp(operation_type, "insert") == 0) {
        metrics->insert_operations++;
        double total_time = metrics->avg_insert_time_ms * (metrics->insert_operations - 1);
        metrics->avg_insert_time_ms = (total_time + operation_time_ms) / metrics->insert_operations;
    } else if (strcmp(operation_type, "update") == 0) {
        metrics->update_operations++;
        double total_time = metrics->avg_update_time_ms * (metrics->update_operations - 1);
        metrics->avg_update_time_ms = (total_time + operation_time_ms) / metrics->update_operations;
    } else if (strcmp(operation_type, "delete") == 0) {
        metrics->delete_operations++;
        double total_time = metrics->avg_delete_time_ms * (metrics->delete_operations - 1);
        metrics->avg_delete_time_ms = (total_time + operation_time_ms) / metrics->delete_operations;
    }
    
    /* Track failures */
    if (!success) {
        metrics->maintenance_errors++;
    }
    
    /* Update total maintenance time */
    metrics->maintenance_time_spent_ms += operation_time_ms;
    
    /* Recalculate net benefit and ROI */
    metrics->net_benefit_ms = metrics->query_time_saved_ms - metrics->maintenance_time_spent_ms;
    if (metrics->maintenance_time_spent_ms > 0) {
        metrics->roi_percentage = (metrics->query_time_saved_ms / metrics->maintenance_time_spent_ms - 1) * 100;
    }
    
    pthread_mutex_unlock(&g_metrics_collector->metrics_lock);
    
    /* Update Prometheus metrics */
    if (g_index_maintenance_duration_seconds) {
        metrics_histogram_observe(g_index_maintenance_duration_seconds, operation_time_ms / 1000.0);
    }
    
    return 0;
}

int index_metrics_update_storage(const char* collection_name,
                               const char* field_path,
                               uint64_t index_size_bytes,
                               uint64_t document_count) {
    if (!g_metrics_initialized || !collection_name || !field_path) {
        return -1;
    }
    
    pthread_mutex_lock(&g_metrics_collector->metrics_lock);
    
    /* Find metrics entry */
    index_performance_metrics_t* metrics = NULL;
    for (size_t i = 0; i < g_metrics_collector->metrics_count; i++) {
        if (strcmp(g_metrics_collector->metrics_array[i].collection_name, collection_name) == 0 &&
            strcmp(g_metrics_collector->metrics_array[i].field_path, field_path) == 0) {
            metrics = &g_metrics_collector->metrics_array[i];
            break;
        }
    }
    
    if (!metrics) {
        pthread_mutex_unlock(&g_metrics_collector->metrics_lock);
        return -1;
    }
    
    /* Update storage metrics */
    uint64_t old_size = metrics->index_size_bytes;
    metrics->index_size_bytes = index_size_bytes;
    metrics->indexed_documents = document_count;
    
    if (document_count > 0) {
        metrics->bytes_per_document = (double)index_size_bytes / document_count;
    }
    
    /* Update global storage */
    g_metrics_collector->total_storage_used_mb += (index_size_bytes - old_size) / (1024.0 * 1024.0);
    
    pthread_mutex_unlock(&g_metrics_collector->metrics_lock);
    
    /* Update Prometheus metrics */
    if (g_index_size_bytes_gauge) {
        metrics_gauge_set(g_index_size_bytes_gauge, (double)index_size_bytes);
    }
    
    return 0;
}

double index_metrics_calculate_effectiveness(const index_performance_metrics_t* metrics) {
    if (!metrics) {
        return 0.0;
    }
    
    double score = 0.0;
    double weight_total = 0.0;
    
    /* Query performance improvement (40% weight) */
    if (metrics->avg_query_time_without_index_ms > 0) {
        double speedup = metrics->avg_query_time_without_index_ms / 
                        (metrics->avg_query_time_with_index_ms > 0 ? metrics->avg_query_time_with_index_ms : 0.1);
        score += fmin(speedup * 10, 40); /* Cap at 40 points */
        weight_total += 40;
    }
    
    /* Hit rate (20% weight) */
    score += metrics->hit_rate * 0.2;
    weight_total += 20;
    
    /* Usage frequency (20% weight) */
    if (metrics->days_since_last_use == 0) {
        score += 20;
    } else if (metrics->days_since_last_use < 7) {
        score += 15;
    } else if (metrics->days_since_last_use < 30) {
        score += 10;
    } else if (metrics->days_since_last_use < 90) {
        score += 5;
    }
    weight_total += 20;
    
    /* ROI (20% weight) */
    if (metrics->roi_percentage > 0) {
        score += fmin(metrics->roi_percentage / 10, 20); /* Cap at 20 points */
    }
    weight_total += 20;
    
    /* Normalize to 0-100 */
    if (weight_total > 0) {
        score = (score / weight_total) * 100;
    }
    
    return fmin(fmax(score, 0), 100); /* Ensure 0-100 range */
}

json_value_t* index_metrics_get_all_json(void) {
    if (!g_metrics_initialized || !g_metrics_collector) {
        return json_create_object();
    }
    
    pthread_mutex_lock(&g_metrics_collector->metrics_lock);
    
    json_value_t* result = json_create_object();
    json_value_t* indexes = json_create_array();
    
    /* Add each index's metrics */
    for (size_t i = 0; i < g_metrics_collector->metrics_count; i++) {
        index_performance_metrics_t* metrics = &g_metrics_collector->metrics_array[i];
        
        /* Update derived fields */
        metrics->days_since_last_use = days_between(metrics->last_used_at, time(NULL));
        metrics->effectiveness_score = index_metrics_calculate_effectiveness(metrics);
        metrics->query_speedup_factor = metrics->avg_query_time_without_index_ms > 0 ?
            metrics->avg_query_time_without_index_ms / 
            (metrics->avg_query_time_with_index_ms > 0 ? metrics->avg_query_time_with_index_ms : 1) : 1;
        
        /* Create metrics object */
        json_value_t* index_obj = json_create_object();
        json_object_set(index_obj, "collection", json_create_string(metrics->collection_name));
        json_object_set(index_obj, "field", json_create_string(metrics->field_path));
        json_object_set(index_obj, "index_name", json_create_string(metrics->index_name));
        
        /* Query performance */
        json_value_t* query_perf = json_create_object();
        json_object_set(query_perf, "queries_using_index", json_create_number(metrics->queries_using_index));
        json_object_set(query_perf, "avg_query_time_ms", json_create_number(metrics->avg_query_time_with_index_ms));
        json_object_set(query_perf, "speedup_factor", json_create_number(metrics->query_speedup_factor));
        json_object_set(query_perf, "hit_rate", json_create_number(metrics->hit_rate));
        json_object_set(index_obj, "query_performance", query_perf);
        
        /* Storage metrics */
        json_value_t* storage = json_create_object();
        json_object_set(storage, "size_bytes", json_create_number(metrics->index_size_bytes));
        json_object_set(storage, "size_mb", json_create_number(metrics->index_size_bytes / (1024.0 * 1024.0)));
        json_object_set(storage, "documents", json_create_number(metrics->indexed_documents));
        json_object_set(storage, "bytes_per_doc", json_create_number(metrics->bytes_per_document));
        json_object_set(index_obj, "storage", storage);
        
        /* Maintenance metrics */
        json_value_t* maintenance = json_create_object();
        json_object_set(maintenance, "inserts", json_create_number(metrics->insert_operations));
        json_object_set(maintenance, "updates", json_create_number(metrics->update_operations));
        json_object_set(maintenance, "deletes", json_create_number(metrics->delete_operations));
        json_object_set(maintenance, "errors", json_create_number(metrics->maintenance_errors));
        json_object_set(maintenance, "avg_time_ms", json_create_number(
            (metrics->avg_insert_time_ms + metrics->avg_update_time_ms + metrics->avg_delete_time_ms) / 3));
        json_object_set(index_obj, "maintenance", maintenance);
        
        /* Effectiveness */
        json_value_t* effectiveness = json_create_object();
        json_object_set(effectiveness, "score", json_create_number(metrics->effectiveness_score));
        json_object_set(effectiveness, "days_since_use", json_create_number(metrics->days_since_last_use));
        json_object_set(effectiveness, "roi_percentage", json_create_number(metrics->roi_percentage));
        json_object_set(effectiveness, "time_saved_ms", json_create_number(metrics->query_time_saved_ms));
        json_object_set(effectiveness, "recommended_for_removal", 
                       json_create_boolean(metrics->effectiveness_score < 30 || metrics->days_since_last_use > 90));
        json_object_set(index_obj, "effectiveness", effectiveness);
        
        json_array_append(indexes, index_obj);
    }
    
    json_object_set(result, "indexes", indexes);
    
    /* Add global statistics */
    json_value_t* global_stats = json_create_object();
    json_object_set(global_stats, "total_indexes", json_create_number(g_metrics_collector->total_indexes_tracked));
    json_object_set(global_stats, "total_queries_optimized", json_create_number(g_metrics_collector->total_queries_optimized));
    json_object_set(global_stats, "total_time_saved_ms", json_create_number(g_metrics_collector->total_time_saved_ms));
    json_object_set(global_stats, "total_storage_mb", json_create_number(g_metrics_collector->total_storage_used_mb));
    json_object_set(result, "global_stats", global_stats);
    
    pthread_mutex_unlock(&g_metrics_collector->metrics_lock);
    
    return result;
}

json_value_t* index_metrics_get_removal_candidates(uint64_t threshold_days,
                                                  double min_effectiveness) {
    if (!g_metrics_initialized || !g_metrics_collector) {
        return json_create_array();
    }
    
    pthread_mutex_lock(&g_metrics_collector->metrics_lock);
    
    json_value_t* candidates = json_create_array();
    
    for (size_t i = 0; i < g_metrics_collector->metrics_count; i++) {
        index_performance_metrics_t* metrics = &g_metrics_collector->metrics_array[i];
        
        /* Update derived fields */
        metrics->days_since_last_use = days_between(metrics->last_used_at, time(NULL));
        metrics->effectiveness_score = index_metrics_calculate_effectiveness(metrics);
        
        /* Check removal criteria */
        int should_remove = 0;
        char* reason = NULL;
        
        if (metrics->days_since_last_use > threshold_days) {
            should_remove = 1;
            reason = "Inactive for too long";
        } else if (metrics->effectiveness_score < min_effectiveness) {
            should_remove = 1;
            reason = "Low effectiveness score";
        } else if (metrics->roi_percentage < -50) { /* Costs more than it saves */
            should_remove = 1;
            reason = "Negative ROI";
        } else if (metrics->hit_rate < 10 && metrics->queries_using_index > 100) {
            should_remove = 1;
            reason = "Low hit rate";
        }
        
        if (should_remove) {
            json_value_t* candidate = json_create_object();
            json_object_set(candidate, "collection", json_create_string(metrics->collection_name));
            json_object_set(candidate, "field", json_create_string(metrics->field_path));
            json_object_set(candidate, "index_name", json_create_string(metrics->index_name));
            json_object_set(candidate, "reason", json_create_string(reason));
            json_object_set(candidate, "effectiveness_score", json_create_number(metrics->effectiveness_score));
            json_object_set(candidate, "days_inactive", json_create_number(metrics->days_since_last_use));
            json_object_set(candidate, "size_mb", json_create_number(metrics->index_size_bytes / (1024.0 * 1024.0)));
            
            json_array_append(candidates, candidate);
        }
    }
    
    pthread_mutex_unlock(&g_metrics_collector->metrics_lock);
    
    return candidates;
}

int index_metrics_register_prometheus_metrics(void) {
    extern metrics_registry_t* g_metrics_registry;
    
    if (!g_metrics_registry) {
        LOG_WARNING("Metrics registry not available for index metrics");
        return -1;
    }
    
    /* Create Prometheus-style metrics */
    g_index_queries_total = metrics_create_counter(g_metrics_registry,
        "jsondb_index_queries_total",
        "Total number of queries using indexes"
    );
    
    /* Create histogram with reasonable buckets for query duration (in seconds) */
    double query_buckets[] = {0.001, 0.005, 0.01, 0.05, 0.1, 0.5, 1.0, 5.0};
    g_index_query_duration_seconds = metrics_create_histogram(g_metrics_registry,
        "jsondb_index_query_duration_seconds",
        "Query execution time in seconds",
        query_buckets, sizeof(query_buckets) / sizeof(query_buckets[0])
    );
    
    /* Create histogram with buckets for maintenance operations */
    double maint_buckets[] = {0.001, 0.005, 0.01, 0.05, 0.1, 0.5, 1.0};
    g_index_maintenance_duration_seconds = metrics_create_histogram(g_metrics_registry,
        "jsondb_index_maintenance_duration_seconds",
        "Index maintenance operation time in seconds",
        maint_buckets, sizeof(maint_buckets) / sizeof(maint_buckets[0])
    );
    
    g_index_size_bytes_gauge = metrics_create_gauge(g_metrics_registry,
        "jsondb_index_size_bytes",
        "Current size of indexes in bytes"
    );
    
    g_index_effectiveness_gauge = metrics_create_gauge(g_metrics_registry,
        "jsondb_index_effectiveness",
        "Index effectiveness score (0-100)"
    );
    
    /* No need to manually register - metrics_create_* functions handle registration */
    
    LOG_INFO("Registered Prometheus metrics for index performance");
    
    return 0;
}

void index_metrics_format_bytes(uint64_t bytes, char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) return;
    
    if (bytes < 1024) {
        snprintf(buffer, buffer_size, "%lu B", bytes);
    } else if (bytes < 1024 * 1024) {
        snprintf(buffer, buffer_size, "%.2f KB", bytes / 1024.0);
    } else if (bytes < 1024 * 1024 * 1024) {
        snprintf(buffer, buffer_size, "%.2f MB", bytes / (1024.0 * 1024.0));
    } else {
        snprintf(buffer, buffer_size, "%.2f GB", bytes / (1024.0 * 1024.0 * 1024.0));
    }
}

json_value_t* index_metrics_effectiveness_report(void) {
    if (!g_metrics_initialized || !g_metrics_collector) {
        return json_create_object();
    }
    
    pthread_mutex_lock(&g_metrics_collector->metrics_lock);
    
    json_value_t* report = json_create_object();
    
    /* Calculate aggregated statistics */
    int highly_effective = 0, moderately_effective = 0, ineffective = 0;
    double total_speedup = 0, total_roi = 0;
    size_t active_indexes = 0;
    
    for (size_t i = 0; i < g_metrics_collector->metrics_count; i++) {
        index_performance_metrics_t* metrics = &g_metrics_collector->metrics_array[i];
        double effectiveness = index_metrics_calculate_effectiveness(metrics);
        
        if (effectiveness >= 70) {
            highly_effective++;
        } else if (effectiveness >= 40) {
            moderately_effective++;
        } else {
            ineffective++;
        }
        
        if (metrics->days_since_last_use < 7) {
            active_indexes++;
            total_speedup += metrics->query_speedup_factor;
            total_roi += metrics->roi_percentage;
        }
    }
    
    /* Build report */
    json_value_t* summary = json_create_object();
    json_object_set(summary, "total_indexes", json_create_number(g_metrics_collector->metrics_count));
    json_object_set(summary, "active_indexes", json_create_number(active_indexes));
    json_object_set(summary, "highly_effective", json_create_number(highly_effective));
    json_object_set(summary, "moderately_effective", json_create_number(moderately_effective));
    json_object_set(summary, "ineffective", json_create_number(ineffective));
    json_object_set(report, "summary", summary);
    
    if (active_indexes > 0) {
        json_value_t* performance = json_create_object();
        json_object_set(performance, "avg_speedup", json_create_number(total_speedup / active_indexes));
        json_object_set(performance, "avg_roi_percentage", json_create_number(total_roi / active_indexes));
        json_object_set(performance, "total_queries_optimized", 
                       json_create_number(g_metrics_collector->total_queries_optimized));
        json_object_set(report, "performance", performance);
    }
    
    /* Add recommendations */
    json_value_t* recommendations = json_create_array();
    
    if (ineffective > highly_effective) {
        json_array_append(recommendations, 
            json_create_string("Many indexes are ineffective. Review and remove unused indexes."));
    }
    
    if (g_metrics_collector->total_storage_used_mb > 1024) {
        json_array_append(recommendations, 
            json_create_string("Index storage exceeds 1GB. Consider optimizing large indexes."));
    }
    
    if (active_indexes < g_metrics_collector->metrics_count * 0.5) {
        json_array_append(recommendations, 
            json_create_string("Less than 50% of indexes are actively used. Clean up stale indexes."));
    }
    
    json_object_set(report, "recommendations", recommendations);
    
    pthread_mutex_unlock(&g_metrics_collector->metrics_lock);
    
    return report;
}

/* Export metrics to file */
int index_metrics_export(const char* file_path) {
    if (!g_metrics_initialized || !g_metrics_collector || !file_path) {
        return -1;
    }
    
    /* Get all metrics as JSON */
    json_value_t* metrics = index_metrics_get_all_json();
    if (!metrics) {
        return -1;
    }
    
    /* Convert to string */
    char* json_str = json_stringify(metrics);
    json_free(metrics);
    
    if (!json_str) {
        return -1;
    }
    
    /* Write to file */
    FILE* fp = fopen(file_path, "w");
    if (!fp) {
        buffer_pool_free_safe(json_str);
        return -1;
    }
    
    size_t written = fwrite(json_str, 1, strlen(json_str), fp);
    fclose(fp);
    buffer_pool_free_safe(json_str);
    
    return written > 0 ? 0 : -1;
}

/* Import metrics from file */
int index_metrics_import(const char* file_path) {
    /* TODO: Implement metrics import functionality */
    (void)file_path;
    return -1;
}

/* Enable automatic metrics persistence */
int index_metrics_enable_persistence(const char* file_path, int interval_seconds) {
    if (!g_metrics_initialized || !g_metrics_collector || !file_path) {
        return -1;
    }
    
    /* Set persistence parameters */
    g_metrics_collector->metrics_file_path = strdup(file_path);
    g_metrics_collector->auto_persist_enabled = 1;
    g_metrics_collector->collection_interval_seconds = interval_seconds;
    
    return 0;
}

/* Reset metrics for an index */
int index_metrics_reset(const char* collection_name, const char* field_path) {
    if (!g_metrics_initialized || !collection_name || !field_path) {
        return -1;
    }
    
    pthread_mutex_lock(&g_metrics_collector->metrics_lock);
    
    /* Find metrics entry */
    for (size_t i = 0; i < g_metrics_collector->metrics_count; i++) {
        if (strcmp(g_metrics_collector->metrics_array[i].collection_name, collection_name) == 0 &&
            strcmp(g_metrics_collector->metrics_array[i].field_path, field_path) == 0) {
            
            /* Reset metrics while preserving identity */
            index_performance_metrics_t* metrics = &g_metrics_collector->metrics_array[i];
            
            /* Reset counters and statistics */
            metrics->queries_using_index = 0;
            metrics->queries_without_index = 0;
            metrics->avg_query_time_with_index_ms = 0.0;
            metrics->avg_query_time_without_index_ms = 0.0;
            metrics->query_speedup_factor = 0.0;
            metrics->index_lookups = 0;
            metrics->index_hits = 0;
            metrics->index_misses = 0;
            metrics->hit_rate = 0.0;
            metrics->insert_operations = 0;
            metrics->update_operations = 0;
            metrics->delete_operations = 0;
            metrics->avg_insert_time_ms = 0.0;
            metrics->avg_update_time_ms = 0.0;
            metrics->avg_delete_time_ms = 0.0;
            metrics->maintenance_errors = 0;
            metrics->query_time_saved_ms = 0.0;
            metrics->maintenance_time_spent_ms = 0.0;
            metrics->net_benefit_ms = 0.0;
            metrics->roi_percentage = 0.0;
            metrics->effectiveness_score = 50.0;
            metrics->is_recommended_for_removal = 0;
            
            /* Update last used timestamp */
            metrics->last_used_at = time(NULL);
            metrics->days_since_last_use = 0;
            
            pthread_mutex_unlock(&g_metrics_collector->metrics_lock);
            LOG_INFO("Reset metrics for index %s.%s", collection_name, field_path);
            return 0;
        }
    }
    
    pthread_mutex_unlock(&g_metrics_collector->metrics_lock);
    return -1;
}

/* Find metrics entry for an index */
index_performance_metrics_t* index_metrics_find(const char* collection_name,
                                               const char* field_path) {
    if (!g_metrics_initialized || !collection_name || !field_path) {
        return NULL;
    }
    
    pthread_mutex_lock(&g_metrics_collector->metrics_lock);
    
    for (size_t i = 0; i < g_metrics_collector->metrics_count; i++) {
        if (strcmp(g_metrics_collector->metrics_array[i].collection_name, collection_name) == 0 &&
            strcmp(g_metrics_collector->metrics_array[i].field_path, field_path) == 0) {
            pthread_mutex_unlock(&g_metrics_collector->metrics_lock);
            return &g_metrics_collector->metrics_array[i];
        }
    }
    
    pthread_mutex_unlock(&g_metrics_collector->metrics_lock);
    return NULL;
}

/* Get metrics for a specific index */
index_performance_metrics_t* index_metrics_get(const char* collection_name,
                                              const char* field_path) {
    return index_metrics_find(collection_name, field_path);
}

/* Stop tracking metrics for an index */
int index_metrics_stop_tracking(const char* collection_name,
                               const char* field_path) {
    if (!g_metrics_initialized || !collection_name || !field_path) {
        return -1;
    }
    
    pthread_mutex_lock(&g_metrics_collector->metrics_lock);
    
    /* Find and remove metrics entry */
    for (size_t i = 0; i < g_metrics_collector->metrics_count; i++) {
        if (strcmp(g_metrics_collector->metrics_array[i].collection_name, collection_name) == 0 &&
            strcmp(g_metrics_collector->metrics_array[i].field_path, field_path) == 0) {
            
            /* Shift remaining entries */
            for (size_t j = i; j < g_metrics_collector->metrics_count - 1; j++) {
                g_metrics_collector->metrics_array[j] = g_metrics_collector->metrics_array[j + 1];
            }
            
            g_metrics_collector->metrics_count--;
            pthread_mutex_unlock(&g_metrics_collector->metrics_lock);
            
            LOG_INFO("Stopped tracking metrics for index %s.%s", collection_name, field_path);
            return 0;
        }
    }
    
    pthread_mutex_unlock(&g_metrics_collector->metrics_lock);
    return -1;
}

/* Get collection summary */
json_value_t* index_metrics_get_collection_summary(const char* collection_name) {
    if (!g_metrics_initialized || !collection_name) {
        return NULL;
    }
    
    pthread_mutex_lock(&g_metrics_collector->metrics_lock);
    
    json_value_t* summary = json_create_object();
    json_value_t* indexes = json_create_array();
    
    size_t collection_index_count = 0;
    double total_effectiveness = 0.0;
    uint64_t total_queries = 0;
    double total_storage_mb = 0.0;
    
    for (size_t i = 0; i < g_metrics_collector->metrics_count; i++) {
        if (strcmp(g_metrics_collector->metrics_array[i].collection_name, collection_name) == 0) {
            index_performance_metrics_t* metrics = &g_metrics_collector->metrics_array[i];
            
            /* Update derived fields */
            metrics->effectiveness_score = index_metrics_calculate_effectiveness(metrics);
            
            /* Add to summary */
            json_value_t* index_info = json_create_object();
            json_object_set(index_info, "field", json_create_string(metrics->field_path));
            json_object_set(index_info, "effectiveness", json_create_number(metrics->effectiveness_score));
            json_object_set(index_info, "queries", json_create_number(metrics->queries_using_index));
            json_array_append(indexes, index_info);
            
            collection_index_count++;
            total_effectiveness += metrics->effectiveness_score;
            total_queries += metrics->queries_using_index;
            total_storage_mb += metrics->index_size_bytes / (1024.0 * 1024.0);
        }
    }
    
    json_object_set(summary, "collection", json_create_string(collection_name));
    json_object_set(summary, "index_count", json_create_number(collection_index_count));
    json_object_set(summary, "avg_effectiveness", 
                   json_create_number(collection_index_count > 0 ? total_effectiveness / collection_index_count : 0));
    json_object_set(summary, "total_queries_optimized", json_create_number(total_queries));
    json_object_set(summary, "total_storage_mb", json_create_number(total_storage_mb));
    json_object_set(summary, "indexes", indexes);
    
    pthread_mutex_unlock(&g_metrics_collector->metrics_lock);
    
    return summary;
}

/* Calculate ROI percentage */
double index_metrics_calculate_roi(double time_saved_ms, double time_spent_ms) {
    if (time_spent_ms <= 0) {
        return 0.0;
    }
    
    return ((time_saved_ms - time_spent_ms) / time_spent_ms) * 100.0;
}