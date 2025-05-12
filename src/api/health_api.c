#include "jsondb/api/api.h"
#include "jsondb/utils/logger.h"
#include "jsondb/utils/metrics.h"
#include "jsondb/utils/json.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/sysinfo.h>

/* Global server start time */
static time_t g_server_start_time = 0;

/* Initialize health API */
void health_api_init() {
    g_server_start_time = time(NULL);
    
    if (g_logger) {
        LOG_INFO("Health API initialized");
    }
}

/* Get system load average */
static double get_load_average() {
    struct sysinfo info;
    if (sysinfo(&info) != 0) {
        return -1.0;
    }
    
    /* Convert load average to double */
    /* Sysload is stored as fixed-point (scaled by 65536) */
    return (double)info.loads[0] / 65536.0;
}

/* Get memory usage information */
static void get_memory_info(unsigned long *total, unsigned long *free, unsigned long *used) {
    struct sysinfo info;
    if (sysinfo(&info) != 0) {
        *total = 0;
        *free = 0;
        *used = 0;
        return;
    }
    
    *total = info.totalram * info.mem_unit;
    *free = info.freeram * info.mem_unit;
    *used = *total - *free;
}

/* Get process memory usage (RSS) */
static unsigned long get_process_memory() {
    FILE *f;
    unsigned long rss = 0;
    char buf[256];
    
    /* Open /proc/self/statm for reading process memory stats */
    f = fopen("/proc/self/statm", "r");
    if (!f) {
        return 0;
    }
    
    /* Format is: size resident shared text lib data dt */
    if (fgets(buf, sizeof(buf), f)) {
        unsigned long size, resident;
        if (sscanf(buf, "%lu %lu", &size, &resident) == 2) {
            /* Convert to KB - page size is typically 4KB */
            rss = resident * 4;
        }
    }
    
    fclose(f);
    return rss;
}

/* Get server uptime in seconds */
static time_t get_uptime() {
    if (g_server_start_time == 0) {
        return 0;
    }
    
    return time(NULL) - g_server_start_time;
}

/* Handle health check request */
http_response_t* api_handle_health_check(api_context_t *ctx, http_request_t *request) {
    /* Avoid unused parameter warnings */
    (void)ctx;
    (void)request;
    
    /* Create JSON response with health information */
    json_value_t *health = json_object_new();
    
    /* Add status */
    json_object_set_string(health, "status", "ok");
    
    /* Add timestamp */
    json_object_set_number(health, "timestamp", (double)time(NULL));
    
    /* Add uptime */
    time_t uptime = get_uptime();
    json_object_set_number(health, "uptime_seconds", (double)uptime);
    
    /* Add formatted uptime */
    char uptime_str[64];
    int days = uptime / 86400;
    int hours = (uptime % 86400) / 3600;
    int minutes = (uptime % 3600) / 60;
    int seconds = uptime % 60;
    
    snprintf(uptime_str, sizeof(uptime_str), "%dd %dh %dm %ds", days, hours, minutes, seconds);
    json_object_set_string(health, "uptime", uptime_str);
    
    /* Add load average */
    double load = get_load_average();
    if (load >= 0) {
        json_object_set_number(health, "load_average", load);
    }
    
    /* Add memory information */
    unsigned long total_mem, free_mem, used_mem;
    get_memory_info(&total_mem, &free_mem, &used_mem);
    
    json_value_t *memory = json_object_new();
    json_object_set_number(memory, "total_kb", (double)(total_mem / 1024));
    json_object_set_number(memory, "free_kb", (double)(free_mem / 1024));
    json_object_set_number(memory, "used_kb", (double)(used_mem / 1024));
    
    /* Add process memory information */
    unsigned long process_mem = get_process_memory();
    json_object_set_number(memory, "process_kb", (double)process_mem);
    
    json_object_set_value(health, "memory", memory);
    
    /* Add metrics information if available */
    metrics_t *metrics = metrics_get_instance();
    if (metrics) {
        json_value_t *metrics_json = json_object_new();
        
        /* Add request count */
        json_object_set_number(metrics_json, "requests", (double)metrics_get_counter(metrics, "http.requests"));
        
        /* Add average response time */
        json_object_set_number(metrics_json, "avg_response_time_ms", metrics_get_timer_mean(metrics, "http.response_time"));
        
        /* Add database operations */
        json_object_set_number(metrics_json, "db_operations", (double)metrics_get_counter(metrics, "db.operations"));
        
        /* Add JavaScript operations if not disabled */
#ifndef DISABLE_JS
        json_object_set_number(metrics_json, "js_operations", (double)metrics_get_counter(metrics, "js.operations"));
#endif
        
        json_object_set_value(health, "metrics", metrics_json);
    }
    
    /* Convert health object to JSON string */
    char *health_json = json_serialize(health);
    
    /* Create HTTP response */
    http_response_t *response = create_http_response(HTTP_OK, health_json, "application/json");
    
    /* Free resources */
    free(health_json);
    json_value_free(health);
    
    return response;
}

/* Handle metrics request */
http_response_t* api_handle_metrics(api_context_t *ctx, http_request_t *request) {
    /* Avoid unused parameter warnings */
    (void)ctx;
    (void)request;
    
    /* Get metrics instance */
    metrics_t *metrics = metrics_get_instance();
    if (!metrics) {
        return create_http_response(HTTP_INTERNAL_ERROR, "{\"error\":\"Metrics not available\"}", "application/json");
    }
    
    /* Get metrics dump */
    char *metrics_dump = metrics_to_json(metrics);
    if (!metrics_dump) {
        return create_http_response(HTTP_INTERNAL_ERROR, "{\"error\":\"Failed to dump metrics\"}", "application/json");
    }
    
    /* Create HTTP response */
    http_response_t *response = create_http_response(HTTP_OK, metrics_dump, "application/json");
    
    /* Free resources */
    free(metrics_dump);
    
    return response;
}

/* Handle available metrics request */
http_response_t* api_handle_metrics_available(api_context_t *ctx, http_request_t *request) {
    /* Avoid unused parameter warnings */
    (void)ctx;
    (void)request;
    
    /* Get metrics instance */
    metrics_t *metrics = metrics_get_instance();
    if (!metrics) {
        return create_http_response(HTTP_INTERNAL_ERROR, "{\"error\":\"Metrics not available\"}", "application/json");
    }
    
    /* Get available metrics */
    char *available_metrics = metrics_list_available(metrics);
    if (!available_metrics) {
        return create_http_response(HTTP_INTERNAL_ERROR, "{\"error\":\"Failed to list available metrics\"}", "application/json");
    }
    
    /* Create HTTP response */
    http_response_t *response = create_http_response(HTTP_OK, available_metrics, "application/json");
    
    /* Free resources */
    free(available_metrics);
    
    return response;
}

/* Register health API endpoints */
void register_health_api_endpoints(api_context_t *ctx) {
    /* Initialize health API */
    health_api_init();
    
    /* Register endpoints */
    api_register_endpoint(ctx, "GET", "/health", api_handle_health_check);
    api_register_endpoint(ctx, "GET", "/metrics", api_handle_metrics);
    api_register_endpoint(ctx, "GET", "/metrics/available", api_handle_metrics_available);
    
    if (g_logger) {
        LOG_INFO("Health API endpoints registered");
    }
}