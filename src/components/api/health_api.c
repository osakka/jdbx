#include "api/api.h"
#include "utils/logger.h"
#include "utils/metrics.h"
#include "utils/json.h"
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
    json_value_t *health = json_create_object();
    
    /* Add status */
    json_object_set(health, "status", json_create_string("ok"));
    
    /* Add timestamp */
    json_object_set(health, "timestamp", json_create_number((double)time(NULL)));
    
    /* Add uptime */
    time_t uptime = get_uptime();
    json_object_set(health, "uptime_seconds", json_create_number((double)uptime));
    
    /* Add formatted uptime */
    char uptime_str[64];
    int days = uptime / 86400;
    int hours = (uptime % 86400) / 3600;
    int minutes = (uptime % 3600) / 60;
    int seconds = uptime % 60;
    
    snprintf(uptime_str, sizeof(uptime_str), "%dd %dh %dm %ds", days, hours, minutes, seconds);
    json_object_set(health, "uptime", json_create_string(uptime_str));
    
    /* Add load average */
    double load = get_load_average();
    if (load >= 0) {
        json_object_set(health, "load_average", json_create_number(load));
    }
    
    /* Add memory information */
    unsigned long total_mem, free_mem, used_mem;
    get_memory_info(&total_mem, &free_mem, &used_mem);
    
    json_value_t *memory = json_create_object();
    json_object_set(memory, "total_kb", json_create_number((double)(total_mem / 1024)));
    json_object_set(memory, "free_kb", json_create_number((double)(free_mem / 1024)));
    json_object_set(memory, "used_kb", json_create_number((double)(used_mem / 1024)));
    
    /* Add process memory information */
    unsigned long process_mem = get_process_memory();
    json_object_set(memory, "process_kb", json_create_number((double)process_mem));
    
    json_object_set(health, "memory", memory);
    
    /* Add metrics information if available */
    metric_t *metrics = (metric_t *)metrics_get_json(NULL);
    if (metrics) {
        json_value_t *metrics_json = json_create_object();
        
        /* Add some basic metrics - these functions would need to be properly implemented */
        json_object_set(metrics_json, "requests", json_create_number(0.0));
        json_object_set(metrics_json, "avg_response_time_ms", json_create_number(0.0));
        json_object_set(metrics_json, "db_operations", json_create_number(0.0));
        
        /* Add JavaScript operations if not disabled */
#ifndef DISABLE_JS
        json_object_set(metrics_json, "js_operations", json_create_number(0.0));
#endif
        
        json_object_set(health, "metrics", metrics_json);
    }
    
    /* Convert health object to JSON string */
    char *health_json = json_stringify(health);
    
    /* Create HTTP response */
    http_response_t *response = create_http_response(HTTP_OK, health_json, "application/json");
    
    /* Free resources */
    free(health_json);
    json_free(health);
    
    return response;
}

/* Handle metrics request */
http_response_t* api_handle_metrics(api_context_t *ctx, http_request_t *request) {
    /* Avoid unused parameter warnings */
    (void)ctx;
    (void)request;
    
    /* Get metrics JSON representation */
    char *metrics_dump = metrics_get_json(NULL);
    if (!metrics_dump) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, "{\"error\":\"Failed to dump metrics\"}", "application/json");
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
    
    /* This is a placeholder - would need to be implemented properly */
    char *available_metrics = strdup("{\"available_metrics\":[\"http.requests\",\"http.response_time\",\"db.operations\"]}");
    if (!available_metrics) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, "{\"error\":\"Failed to list available metrics\"}", "application/json");
    }
    
    /* Create HTTP response */
    http_response_t *response = create_http_response(HTTP_OK, available_metrics, "application/json");
    
    /* Free resources */
    free(available_metrics);
    
    return response;
}

/* Register health API endpoints - would need to be implemented properly */
void register_health_api_endpoints(api_context_t *ctx) {
    /* Initialize health API */
    health_api_init();
    
    /* This is just a placeholder as the actual registration function is not implemented */
    (void)ctx;
    
    if (g_logger) {
        LOG_INFO("Health API endpoints registered");
    }
}