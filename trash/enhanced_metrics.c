/**
 * Enhanced Metrics Collection System for JDBX
 * Integrates with QuickJS engine for advanced analytics and configurable collection
 */

#include "utils/metrics.h"
#include "utils/logger.h"
#include "js/js_engine.h"
#include "database/database.h"
#include "utils/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <errno.h>

/* Enhanced metrics structure */
typedef struct enhanced_metrics {
    // Error tracking
    uint64_t http_2xx_count;
    uint64_t http_3xx_count;
    uint64_t http_4xx_count;
    uint64_t http_5xx_count;
    uint64_t auth_failures;
    uint64_t db_errors;
    
    // Performance metrics with percentiles
    double response_times[1000]; // Ring buffer for percentile calculation
    size_t response_time_index;
    size_t response_time_count;
    
    // Resource utilization
    double cpu_usage_percent;
    uint64_t memory_rss_bytes;
    uint64_t memory_vms_bytes;
    uint32_t open_file_descriptors;
    uint32_t thread_count;
    uint64_t network_bytes_in;
    uint64_t network_bytes_out;
    
    // Database performance
    uint64_t db_query_count;
    uint64_t db_index_hits;
    uint64_t db_index_misses;
    double db_avg_query_time;
    uint64_t db_lock_waits;
    uint64_t db_deadlocks;
    
    // Business metrics
    uint64_t documents_created;
    uint64_t documents_updated;
    uint64_t documents_deleted;
    uint64_t collections_created;
    uint64_t users_active;
    
    // Temporal data
    time_t last_update;
    uint32_t update_interval_seconds;
    
    pthread_mutex_t mutex;
} enhanced_metrics_t;

static enhanced_metrics_t* g_enhanced_metrics = NULL;

/* Initialize enhanced metrics system */
int enhanced_metrics_init(uint32_t update_interval) {
    if (g_enhanced_metrics != NULL) {
        LOG_WARNING("Enhanced metrics already initialized");
        return 0;
    }
    
    g_enhanced_metrics = calloc(1, sizeof(enhanced_metrics_t));
    if (!g_enhanced_metrics) {
        LOG_ERROR("Failed to allocate enhanced metrics structure");
        return -1;
    }
    
    g_enhanced_metrics->update_interval_seconds = update_interval > 0 ? update_interval : 60;
    g_enhanced_metrics->last_update = time(NULL);
    
    if (pthread_mutex_init(&g_enhanced_metrics->mutex, NULL) != 0) {
        LOG_ERROR("Failed to initialize enhanced metrics mutex");
        free(g_enhanced_metrics);
        g_enhanced_metrics = NULL;
        return -1;
    }
    
    LOG_INFO("Enhanced metrics system initialized with %u second update interval", 
             g_enhanced_metrics->update_interval_seconds);
    return 0;
}

/* Record HTTP status code */
void enhanced_metrics_record_http_status(int status_code) {
    if (!g_enhanced_metrics) return;
    
    pthread_mutex_lock(&g_enhanced_metrics->mutex);
    
    if (status_code >= 200 && status_code < 300) {
        g_enhanced_metrics->http_2xx_count++;
    } else if (status_code >= 300 && status_code < 400) {
        g_enhanced_metrics->http_3xx_count++;
    } else if (status_code >= 400 && status_code < 500) {
        g_enhanced_metrics->http_4xx_count++;
        if (status_code == 401 || status_code == 403) {
            g_enhanced_metrics->auth_failures++;
        }
    } else if (status_code >= 500) {
        g_enhanced_metrics->http_5xx_count++;
    }
    
    pthread_mutex_unlock(&g_enhanced_metrics->mutex);
}

/* Record response time for percentile calculation */
void enhanced_metrics_record_response_time(double response_time_ms) {
    if (!g_enhanced_metrics) return;
    
    pthread_mutex_lock(&g_enhanced_metrics->mutex);
    
    size_t index = g_enhanced_metrics->response_time_index;
    g_enhanced_metrics->response_times[index] = response_time_ms;
    g_enhanced_metrics->response_time_index = (index + 1) % 1000;
    
    if (g_enhanced_metrics->response_time_count < 1000) {
        g_enhanced_metrics->response_time_count++;
    }
    
    pthread_mutex_unlock(&g_enhanced_metrics->mutex);
}

/* Record database operation */
void enhanced_metrics_record_db_operation(const char* operation, double duration_ms, int success, int index_used) {
    if (!g_enhanced_metrics) return;
    
    pthread_mutex_lock(&g_enhanced_metrics->mutex);
    
    g_enhanced_metrics->db_query_count++;
    
    if (index_used) {
        g_enhanced_metrics->db_index_hits++;
    } else {
        g_enhanced_metrics->db_index_misses++;
    }
    
    // Update average query time (exponential moving average)
    if (g_enhanced_metrics->db_avg_query_time == 0) {
        g_enhanced_metrics->db_avg_query_time = duration_ms;
    } else {
        g_enhanced_metrics->db_avg_query_time = 
            0.9 * g_enhanced_metrics->db_avg_query_time + 0.1 * duration_ms;
    }
    
    if (!success) {
        g_enhanced_metrics->db_errors++;
    }
    
    pthread_mutex_unlock(&g_enhanced_metrics->mutex);
}

/* Update system resource metrics */
void enhanced_metrics_update_system_resources() {
    if (!g_enhanced_metrics) return;
    
    pthread_mutex_lock(&g_enhanced_metrics->mutex);
    
    // Get memory usage
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        g_enhanced_metrics->memory_rss_bytes = usage.ru_maxrss * 1024; // Convert KB to bytes on Linux
    }
    
    // Get file descriptor count
    char fd_path[256];
    snprintf(fd_path, sizeof(fd_path), "/proc/%d/fd", getpid());
    
    DIR* fd_dir = opendir(fd_path);
    if (fd_dir) {
        struct dirent* entry;
        uint32_t fd_count = 0;
        while ((entry = readdir(fd_dir)) != NULL) {
            if (entry->d_name[0] != '.') {
                fd_count++;
            }
        }
        closedir(fd_dir);
        g_enhanced_metrics->open_file_descriptors = fd_count;
    }
    
    // Get CPU usage (simplified - would need more sophisticated tracking)
    static unsigned long long last_total = 0, last_idle = 0;
    FILE* stat_file = fopen("/proc/stat", "r");
    if (stat_file) {
        unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
        if (fscanf(stat_file, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
                   &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) == 8) {
            unsigned long long total = user + nice + system + idle + iowait + irq + softirq + steal;
            if (last_total > 0) {
                unsigned long long total_diff = total - last_total;
                unsigned long long idle_diff = idle - last_idle;
                if (total_diff > 0) {
                    g_enhanced_metrics->cpu_usage_percent = 
                        100.0 * (total_diff - idle_diff) / total_diff;
                }
            }
            last_total = total;
            last_idle = idle;
        }
        fclose(stat_file);
    }
    
    g_enhanced_metrics->last_update = time(NULL);
    
    pthread_mutex_unlock(&g_enhanced_metrics->mutex);
}

/* Calculate percentiles using JavaScript engine */
json_value_t* enhanced_metrics_calculate_percentiles() {
    if (!g_enhanced_metrics || g_enhanced_metrics->response_time_count == 0) {
        return json_create_object();
    }
    
    pthread_mutex_lock(&g_enhanced_metrics->mutex);
    
    // Create JavaScript array of response times
    json_value_t* times_array = json_create_array();
    size_t count = g_enhanced_metrics->response_time_count;
    
    for (size_t i = 0; i < count; i++) {
        json_array_append(times_array, json_create_number(g_enhanced_metrics->response_times[i]));
    }
    
    pthread_mutex_unlock(&g_enhanced_metrics->mutex);
    
    // Execute JavaScript percentile calculation
    if (g_js_engine) {
        char* js_code = 
            "function calculatePercentiles(values, percentiles) {\n"
            "    if (!values || values.length === 0) return {};\n"
            "    const sorted = values.slice().sort((a, b) => a - b);\n"
            "    const result = {};\n"
            "    percentiles.forEach(p => {\n"
            "        const index = (p / 100) * (sorted.length - 1);\n"
            "        if (index === Math.floor(index)) {\n"
            "            result['p' + p] = sorted[index];\n"
            "        } else {\n"
            "            const lower = sorted[Math.floor(index)];\n"
            "            const upper = sorted[Math.ceil(index)];\n"
            "            result['p' + p] = lower + (upper - lower) * (index - Math.floor(index));\n"
            "        }\n"
            "    });\n"
            "    return result;\n"
            "}\n"
            "calculatePercentiles(responseTimeData, [50, 75, 90, 95, 99]);";
        
        // Set the response time data as a global variable
        js_engine_set_global_json(g_js_engine, "responseTimeData", times_array);
        
        // Execute the JavaScript code
        char* result_str = js_engine_eval(g_js_engine, js_code);
        json_value_t* percentiles = NULL;
        
        if (result_str) {
            percentiles = json_parse(result_str);
            free(result_str);
        }
        
        json_free(times_array);
        return percentiles ? percentiles : json_create_object();
    }
    
    json_free(times_array);
    return json_create_object();
}

/* Generate comprehensive metrics report */
json_value_t* enhanced_metrics_get_report(int include_percentiles) {
    if (!g_enhanced_metrics) {
        return NULL;
    }
    
    pthread_mutex_lock(&g_enhanced_metrics->mutex);
    
    json_value_t* report = json_create_object();
    json_value_t* timestamp_val = json_create_number((double)g_enhanced_metrics->last_update);
    json_object_set(report, "timestamp", timestamp_val);
    
    // HTTP status codes
    json_value_t* http_stats = json_create_object();
    json_object_set(http_stats, "status_2xx", json_create_number(g_enhanced_metrics->http_2xx_count));
    json_object_set(http_stats, "status_3xx", json_create_number(g_enhanced_metrics->http_3xx_count));
    json_object_set(http_stats, "status_4xx", json_create_number(g_enhanced_metrics->http_4xx_count));
    json_object_set(http_stats, "status_5xx", json_create_number(g_enhanced_metrics->http_5xx_count));
    json_object_set(http_stats, "auth_failures", json_create_number(g_enhanced_metrics->auth_failures));
    
    uint64_t total_requests = g_enhanced_metrics->http_2xx_count + 
                             g_enhanced_metrics->http_3xx_count + 
                             g_enhanced_metrics->http_4xx_count + 
                             g_enhanced_metrics->http_5xx_count;
    
    double error_rate = total_requests > 0 ? 
        (double)(g_enhanced_metrics->http_4xx_count + g_enhanced_metrics->http_5xx_count) / total_requests : 0.0;
    json_object_set(http_stats, "total_requests", json_create_number(total_requests));
    json_object_set(http_stats, "error_rate", json_create_number(error_rate));
    
    json_object_set(report, "http", http_stats);
    
    // Resource utilization
    json_value_t* resources = json_create_object();
    json_object_set(resources, "cpu_usage_percent", json_create_number(g_enhanced_metrics->cpu_usage_percent));
    json_object_set(resources, "memory_rss_bytes", json_create_number(g_enhanced_metrics->memory_rss_bytes));
    json_object_set(resources, "open_file_descriptors", json_create_number(g_enhanced_metrics->open_file_descriptors));
    json_object_set(report, "resources", resources);
    
    // Database performance
    json_value_t* database = json_create_object();
    json_object_set(database, "query_count", json_create_number(g_enhanced_metrics->db_query_count));
    json_object_set(database, "avg_query_time_ms", json_create_number(g_enhanced_metrics->db_avg_query_time));
    json_object_set(database, "index_hits", json_create_number(g_enhanced_metrics->db_index_hits));
    json_object_set(database, "index_misses", json_create_number(g_enhanced_metrics->db_index_misses));
    json_object_set(database, "errors", json_create_number(g_enhanced_metrics->db_errors));
    
    uint64_t total_index_ops = g_enhanced_metrics->db_index_hits + g_enhanced_metrics->db_index_misses;
    double index_hit_rate = total_index_ops > 0 ? 
        (double)g_enhanced_metrics->db_index_hits / total_index_ops : 0.0;
    json_object_set(database, "index_hit_rate", json_create_number(index_hit_rate));
    
    json_object_set(report, "database", database);
    
    pthread_mutex_unlock(&g_enhanced_metrics->mutex);
    
    // Add percentiles if requested
    if (include_percentiles) {
        json_value_t* percentiles = enhanced_metrics_calculate_percentiles();
        json_object_set(report, "response_time_percentiles", percentiles);
    }
    
    return report;
}

/* Store enhanced metrics to database */
int enhanced_metrics_persist_to_database(database_t* db) {
    if (!g_enhanced_metrics || !db) {
        return -1;
    }
    
    json_value_t* metrics_doc = enhanced_metrics_get_report(1);
    if (!metrics_doc) {
        return -1;
    }
    
    // Add metadata
    json_object_set(metrics_doc, "type", json_create_string("enhanced_metrics"));
    json_object_set(metrics_doc, "version", json_create_string("1.0"));
    
    char timestamp_str[64];
    time_t now = time(NULL);
    strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
    json_object_set(metrics_doc, "created_at", json_create_string(timestamp_str));
    
    // Store in system/enhanced_metrics collection
    char* doc_id = db_insert_document(db, "system/enhanced_metrics", metrics_doc);
    
    json_free(metrics_doc);
    
    if (doc_id) {
        LOG_DEBUG("Enhanced metrics persisted with ID: %s", doc_id);
        free(doc_id);
        return 0;
    } else {
        LOG_ERROR("Failed to persist enhanced metrics to database");
        return -1;
    }
}

/* Cleanup enhanced metrics */
void enhanced_metrics_cleanup() {
    if (g_enhanced_metrics) {
        pthread_mutex_destroy(&g_enhanced_metrics->mutex);
        free(g_enhanced_metrics);
        g_enhanced_metrics = NULL;
        LOG_INFO("Enhanced metrics system cleaned up");
    }
}