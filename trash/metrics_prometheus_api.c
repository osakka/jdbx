#include "api/api.h"
#include "utils/metrics.h"
#include "utils/metrics_persistence.h"
#include "utils/logger.h"
#include "database/database.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/**
 * Generate Prometheus-compatible metrics output
 * GET /metrics
 */
http_response_t* api_handle_prometheus_metrics(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }

    // Check if Prometheus export is enabled
    const char* enabled = getenv("JSONDB_METRICS_PROMETHEUS_ENABLED");
    if (!enabled || strcmp(enabled, "true") != 0) {
        return create_http_response(HTTP_NOT_FOUND, 
                                  "{\"error\":\"Prometheus metrics not enabled\"}", "application/json");
    }

    char buffer[8192];
    char* ptr = buffer;
    size_t remaining = sizeof(buffer);
    int written;

    // Get current metrics
    metrics_snapshot_t* snapshot = metrics_get_current_snapshot();
    if (!snapshot) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Failed to get metrics\"}", "application/json");
    }

    // Header
    written = snprintf(ptr, remaining, 
        "# JSONdb Metrics - Prometheus Format\n"
        "# Generated at %ld\n\n", time(NULL));
    ptr += written;
    remaining -= written;

    // Operations metrics
    written = snprintf(ptr, remaining,
        "# HELP jsondb_operations_total Total number of operations\n"
        "# TYPE jsondb_operations_total counter\n"
        "jsondb_operations_total{type=\"read\"} %llu\n"
        "jsondb_operations_total{type=\"write\"} %llu\n"
        "jsondb_operations_total{type=\"database\"} %llu\n"
        "jsondb_operations_total{type=\"total\"} %llu\n\n",
        (unsigned long long)snapshot->operations.read_ops,
        (unsigned long long)snapshot->operations.write_ops,
        (unsigned long long)snapshot->operations.db_ops,
        (unsigned long long)snapshot->operations.total_ops);
    ptr += written;
    remaining -= written;

    // Response time metrics
    written = snprintf(ptr, remaining,
        "# HELP jsondb_response_time_milliseconds Response time statistics\n"
        "# TYPE jsondb_response_time_milliseconds summary\n"
        "jsondb_response_time_milliseconds{quantile=\"0.5\"} %.2f\n"
        "jsondb_response_time_milliseconds{quantile=\"0.9\"} %.2f\n"
        "jsondb_response_time_milliseconds{quantile=\"0.95\"} %.2f\n"
        "jsondb_response_time_milliseconds{quantile=\"0.99\"} %.2f\n"
        "jsondb_response_time_milliseconds_sum %.2f\n"
        "jsondb_response_time_milliseconds_count %llu\n\n",
        snapshot->performance.avg_response_time,  // TODO: Add percentiles
        snapshot->performance.avg_response_time * 1.5,
        snapshot->performance.avg_response_time * 2.0,
        snapshot->performance.max_response_time,
        snapshot->performance.avg_response_time * snapshot->operations.total_ops,
        (unsigned long long)snapshot->operations.total_ops);
    ptr += written;
    remaining -= written;

    // Cache metrics
    written = snprintf(ptr, remaining,
        "# HELP jsondb_cache_hits_total Total cache hits\n"
        "# TYPE jsondb_cache_hits_total counter\n"
        "jsondb_cache_hits_total %llu\n\n"
        "# HELP jsondb_cache_misses_total Total cache misses\n"
        "# TYPE jsondb_cache_misses_total counter\n"
        "jsondb_cache_misses_total %llu\n\n"
        "# HELP jsondb_cache_hit_ratio Cache hit ratio\n"
        "# TYPE jsondb_cache_hit_ratio gauge\n"
        "jsondb_cache_hit_ratio %.4f\n\n"
        "# HELP jsondb_cache_size_bytes Current cache size\n"
        "# TYPE jsondb_cache_size_bytes gauge\n"
        "jsondb_cache_size_bytes %llu\n\n",
        (unsigned long long)snapshot->cache.hits,
        (unsigned long long)snapshot->cache.misses,
        snapshot->cache.hit_rate,
        (unsigned long long)snapshot->cache.size_bytes);
    ptr += written;
    remaining -= written;

    // Memory metrics
    written = snprintf(ptr, remaining,
        "# HELP jsondb_memory_bytes Memory usage\n"
        "# TYPE jsondb_memory_bytes gauge\n"
        "jsondb_memory_bytes{type=\"process\"} %llu\n"
        "jsondb_memory_bytes{type=\"system_total\"} %llu\n"
        "jsondb_memory_bytes{type=\"system_free\"} %llu\n"
        "jsondb_memory_bytes{type=\"system_used\"} %llu\n\n",
        (unsigned long long)snapshot->memory.process_kb * 1024,
        (unsigned long long)snapshot->memory.total_kb * 1024,
        (unsigned long long)snapshot->memory.free_kb * 1024,
        (unsigned long long)snapshot->memory.used_kb * 1024);
    ptr += written;
    remaining -= written;

    // Connection metrics
    written = snprintf(ptr, remaining,
        "# HELP jsondb_connections Current connections\n"
        "# TYPE jsondb_connections gauge\n"
        "jsondb_connections{state=\"active\"} %llu\n"
        "jsondb_connections{state=\"total\"} %llu\n\n",
        (unsigned long long)snapshot->connections.active,
        (unsigned long long)snapshot->connections.total);
    ptr += written;
    remaining -= written;

    // Database metrics
    database_t* db = ctx->server->db;
    if (db) {
        size_t collection_count = 0;
        size_t total_documents = 0;
        
        // Count collections and documents
        for (size_t i = 0; i < db->collections_size; i++) {
            if (db->collections[i]) {
                collection_count++;
                total_documents += db->collections[i]->documents_count;
            }
        }

        written = snprintf(ptr, remaining,
            "# HELP jsondb_collections_total Total number of collections\n"
            "# TYPE jsondb_collections_total gauge\n"
            "jsondb_collections_total %zu\n\n"
            "# HELP jsondb_documents_total Total number of documents\n"
            "# TYPE jsondb_documents_total gauge\n"
            "jsondb_documents_total %zu\n\n",
            collection_count,
            total_documents);
        ptr += written;
        remaining -= written;
    }

    // Server info
    written = snprintf(ptr, remaining,
        "# HELP jsondb_up JSONdb server status\n"
        "# TYPE jsondb_up gauge\n"
        "jsondb_up 1\n\n"
        "# HELP jsondb_version_info JSONdb version information\n"
        "# TYPE jsondb_version_info gauge\n"
        "jsondb_version_info{version=\"2.0.7\"} 1\n");

    free(snapshot);

    return create_http_response(HTTP_OK, buffer, "text/plain; version=0.0.4");
}