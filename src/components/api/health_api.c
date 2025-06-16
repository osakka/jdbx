#include "api/api.h"
#include "utils/logger.h"
#include "utils/metrics.h"
#include "utils/json.h"
#include "utils/buffer_pool.h"
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
    LOG_INFO("Health API initialized.");
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
  if (g_metrics_registry) {
    json_value_t *metrics_json = json_create_object();
    
    /* Get server request metrics */
    metric_t *request_counter = get_server_requests_metric();
    metric_t *request_timer = get_server_request_duration_metric();
    metric_t *db_ops_counter = get_db_operations_metric();
    metric_t *active_conns = get_active_connections_metric();
    
    /* Add operations object */
    json_value_t *operations = json_create_object();
    
    if (request_counter) {
      json_object_set(operations, "total", json_create_number((double)request_counter->value.counter));
    }
    
    if (db_ops_counter) {
      json_object_set(operations, "database", json_create_number((double)db_ops_counter->value.counter));
    }
    
    /* Add read/write operation breakdown */
    metric_t* db_read_ops = get_db_read_operations_metric();
    metric_t* db_write_ops = get_db_write_operations_metric();
    
    if (db_read_ops) {
      json_object_set(operations, "read", json_create_number((double)db_read_ops->value.counter));
    }
    
    if (db_write_ops) {
      json_object_set(operations, "write", json_create_number((double)db_write_ops->value.counter));
    }
    
    json_object_set(metrics_json, "operations", operations);
    
    /* Add performance object */
    json_value_t *performance = json_create_object();
    
    if (request_timer && request_timer->value.timer.count > 0) {
      double avg_ms = (request_timer->value.timer.sum / request_timer->value.timer.count) * 1000.0;
      json_object_set(performance, "avg_response_time_ms", json_create_number(avg_ms));
      json_object_set(performance, "min_response_time_ms", json_create_number(request_timer->value.timer.min * 1000.0));
      json_object_set(performance, "max_response_time_ms", json_create_number(request_timer->value.timer.max * 1000.0));
    } else {
      json_object_set(performance, "avg_response_time_ms", json_create_number(0.0));
    }
    
    if (active_conns) {
      json_object_set(performance, "active_connections", json_create_number(active_conns->value.gauge));
    }
    
    json_object_set(metrics_json, "performance", performance);
    
    /* Add cache metrics */
    json_value_t *cache = json_create_object();
    
    /* Get cache metrics */
    metric_t* cache_hits = get_cache_hits_metric();
    metric_t* cache_misses = get_cache_misses_metric();
    metric_t* cache_evictions = get_cache_evictions_metric();
    metric_t* cache_size = get_cache_size_metric();
    
    double hits = cache_hits ? cache_hits->value.counter : 0.0;
    double misses = cache_misses ? cache_misses->value.counter : 0.0;
    double total_requests = hits + misses;
    double hit_rate = (total_requests > 0) ? (hits / total_requests) * 100.0 : 0.0;
    
    json_object_set(cache, "hit_rate", json_create_number(hit_rate));
    json_object_set(cache, "hits", json_create_number(hits));
    json_object_set(cache, "misses", json_create_number(misses));
    json_object_set(cache, "evictions", json_create_number(cache_evictions ? cache_evictions->value.counter : 0.0));
    json_object_set(cache, "size_bytes", json_create_number(cache_size ? cache_size->value.gauge : 0.0));
    json_object_set(cache, "size_mb", json_create_number(cache_size ? cache_size->value.gauge / (1024.0 * 1024.0) : 0.0));
    
    json_object_set(metrics_json, "cache", cache);
    
    json_object_set(health, "metrics", metrics_json);
  }
  
  /* Convert health object to JSON string */
  char *health_json = json_stringify(health);
  
  /* Create HTTP response */
  http_response_t *response = create_http_response(HTTP_OK, health_json, "application/json");
  
  /* Free resources */
  BUFFER_FREE(health_json);
  json_free(health);
  
  return response;
}

/* Declare global metrics registry */
extern metrics_registry_t* g_metrics_registry;

/* Handle metrics request - Prometheus format */
http_response_t* health_api_handle_metrics(api_context_t *ctx, http_request_t *request) {
  if (!ctx || !request) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
#ifndef TOOLS_BUILD
  if (!g_metrics_registry) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Metrics registry not initialized\"}", "application/json");
  }
  
  /* Get metrics JSON representation */
  char* metrics_json = metrics_get_json(g_metrics_registry);
  if (!metrics_json) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to get metrics\"}", "application/json");
  }
  
  /* Convert JSON to Prometheus format (simplified) */
  json_value_t* root = json_parse(metrics_json);
  if (!root) {
    BUFFER_FREE(metrics_json);
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to parse metrics JSON\"}", "application/json");
  }
  
  /* Create a buffer for the Prometheus format output */
  size_t buffer_size = 10240; /* Start with 10KB buffer */
  char* prom_buffer = (char*)BUFFER_ALLOC(buffer_size);
  if (!prom_buffer) {
    json_free(root);
    BUFFER_FREE(metrics_json);
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Memory allocation failure\"}", "application/json");
  }
  
  /* Initialize buffer */
  size_t buffer_used = 0;
  prom_buffer[0] = '\0';
  
  /* Get the metrics array */
  json_value_t* metrics_array = json_object_get(root, "metrics");
  if (metrics_array && metrics_array->type == JSON_ARRAY) {
    for (size_t i = 0; i < metrics_array->value.array.size; i++) {
      json_value_t* metric = json_array_get(metrics_array, i);
      
      /* Extract metric properties */
      json_value_t* name_val = json_object_get(metric, "name");
      json_value_t* description_val = json_object_get(metric, "description");
      json_value_t* type_val = json_object_get(metric, "type");
      
      if (!name_val || name_val->type != JSON_STRING || 
        !type_val || type_val->type != JSON_STRING) {
        continue; /* Skip metrics with missing or invalid required fields */
      }
      
      const char* name = name_val->value.string;
      const char* type = type_val->value.string;
      const char* description = description_val && description_val->type == JSON_STRING ? 
                   description_val->value.string : "";
      
      /* Add metric description as a comment */
      buffer_used += snprintf(prom_buffer + buffer_used, buffer_size - buffer_used, 
                  "# HELP %s %s\n", name, description);
      
      /* Add metric type */
      const char* prom_type = "untyped";
      if (strcmp(type, "counter") == 0) {
        prom_type = "counter";
      } else if (strcmp(type, "gauge") == 0) {
        prom_type = "gauge";
      } else if (strcmp(type, "timer") == 0) {
        prom_type = "summary";
      } else if (strcmp(type, "histogram") == 0) {
        prom_type = "histogram";
      }
      
      buffer_used += snprintf(prom_buffer + buffer_used, buffer_size - buffer_used, 
                  "# TYPE %s %s\n", name, prom_type);
      
      /* Add metric value(s) based on type */
      if (strcmp(type, "counter") == 0) {
        json_value_t* value = json_object_get(metric, "value");
        if (value && value->type == JSON_NUMBER) {
          buffer_used += snprintf(prom_buffer + buffer_used, buffer_size - buffer_used, 
                     "%s %g\n", name, value->value.number);
        }
      } else if (strcmp(type, "gauge") == 0) {
        json_value_t* value = json_object_get(metric, "value");
        if (value && value->type == JSON_NUMBER) {
          buffer_used += snprintf(prom_buffer + buffer_used, buffer_size - buffer_used, 
                     "%s %g\n", name, value->value.number);
        }
      } else if (strcmp(type, "timer") == 0) {
        json_value_t* count = json_object_get(metric, "count");
        json_value_t* sum = json_object_get(metric, "sum");
        json_value_t* min = json_object_get(metric, "min");
        json_value_t* max = json_object_get(metric, "max");
        
        if (count && count->type == JSON_NUMBER) {
          buffer_used += snprintf(prom_buffer + buffer_used, buffer_size - buffer_used, 
                     "%s_count %g\n", name, count->value.number);
        }
        
        if (sum && sum->type == JSON_NUMBER) {
          buffer_used += snprintf(prom_buffer + buffer_used, buffer_size - buffer_used, 
                     "%s_sum %g\n", name, sum->value.number);
        }
        
        if (min && min->type == JSON_NUMBER) {
          buffer_used += snprintf(prom_buffer + buffer_used, buffer_size - buffer_used, 
                     "%s_min %g\n", name, min->value.number);
        }
        
        if (max && max->type == JSON_NUMBER) {
          buffer_used += snprintf(prom_buffer + buffer_used, buffer_size - buffer_used, 
                     "%s_max %g\n", name, max->value.number);
        }
      } else if (strcmp(type, "histogram") == 0) {
        json_value_t* count = json_object_get(metric, "count");
        json_value_t* sum = json_object_get(metric, "sum");
        json_value_t* buckets = json_object_get(metric, "buckets");
        
        if (count && count->type == JSON_NUMBER) {
          buffer_used += snprintf(prom_buffer + buffer_used, buffer_size - buffer_used, 
                     "%s_count %g\n", name, count->value.number);
        }
        
        if (sum && sum->type == JSON_NUMBER) {
          buffer_used += snprintf(prom_buffer + buffer_used, buffer_size - buffer_used, 
                     "%s_sum %g\n", name, sum->value.number);
        }
        
        if (buckets && buckets->type == JSON_ARRAY) {
          for (size_t j = 0; j < buckets->value.array.size; j++) {
            json_value_t* bucket = json_array_get(buckets, j);
            json_value_t* le = json_object_get(bucket, "le");
            json_value_t* bucket_count = json_object_get(bucket, "count");
            
            if (le && le->type == JSON_NUMBER && 
              bucket_count && bucket_count->type == JSON_NUMBER) {
              buffer_used += snprintf(prom_buffer + buffer_used, buffer_size - buffer_used, 
                         "%s_bucket{le=\"%g\"} %g\n", 
                         name, le->value.number, bucket_count->value.number);
            }
          }
          
          /* Add +Inf bucket with the total count */
          if (count && count->type == JSON_NUMBER) {
            buffer_used += snprintf(prom_buffer + buffer_used, buffer_size - buffer_used, 
                       "%s_bucket{le=\"+Inf\"} %g\n", name, count->value.number);
          }
        }
      }
      
      /* Add extra newline between metrics for readability */
      buffer_used += snprintf(prom_buffer + buffer_used, buffer_size - buffer_used, "\n");
      
      /* Check for buffer overflow and resize if needed */
      if (buffer_used >= buffer_size - 1024) {
        buffer_size *= 2;
        char* new_buffer = (char*)BUFFER_REALLOC(prom_buffer, buffer_size);
        if (!new_buffer) {
          BUFFER_FREE(prom_buffer);
          json_free(root);
          BUFFER_FREE(metrics_json);
          return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                       "{\"error\":\"Memory allocation failure\"}", "application/json");
        }
        prom_buffer = new_buffer;
      }
    }
  }
  
  /* Create response */
  http_response_t* response = create_http_response(HTTP_OK, prom_buffer, "text/plain");
  
  /* Clean up */
  BUFFER_FREE(prom_buffer);
  json_free(root);
  BUFFER_FREE(metrics_json);
  
  return response;
#else
  /* In tools build, return empty metrics */
  return create_http_response(HTTP_OK, "# No metrics available in tools build\n", "text/plain");
#endif
}

/* Handler to list available metrics */
http_response_t* health_api_handle_metrics_available(api_context_t *ctx, http_request_t *request) {
  if (!ctx || !request) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
#ifndef TOOLS_BUILD
  if (!g_metrics_registry) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Metrics registry not initialized\"}", "application/json");
  }
  
  /* Get metrics JSON representation */
  char* metrics_json = metrics_get_json(g_metrics_registry);
  if (!metrics_json) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to get metrics\"}", "application/json");
  }
  
  /* Parse the JSON to extract just the names and types */
  json_value_t* root = json_parse(metrics_json);
  if (!root) {
    BUFFER_FREE(metrics_json);
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to parse metrics JSON\"}", "application/json");
  }
  
  /* Create a new JSON object for the response */
  json_value_t* response_obj = json_create_object();
  if (!response_obj) {
    json_free(root);
    BUFFER_FREE(metrics_json);
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Memory allocation failure\"}", "application/json");
  }
  
  /* Create arrays for each metric type */
  json_value_t* counters_array = json_create_array();
  json_value_t* gauges_array = json_create_array();
  json_value_t* timers_array = json_create_array();
  json_value_t* histograms_array = json_create_array();
  
  if (!counters_array || !gauges_array || !timers_array || !histograms_array) {
    if (counters_array) json_free(counters_array);
    if (gauges_array) json_free(gauges_array);
    if (timers_array) json_free(timers_array);
    if (histograms_array) json_free(histograms_array);
    json_free(response_obj);
    json_free(root);
    BUFFER_FREE(metrics_json);
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Memory allocation failure\"}", "application/json");
  }
  
  /* Get the metrics array */
  json_value_t* metrics_array = json_object_get(root, "metrics");
  if (metrics_array && metrics_array->type == JSON_ARRAY) {
    for (size_t i = 0; i < metrics_array->value.array.size; i++) {
      json_value_t* metric = json_array_get(metrics_array, i);
      
      /* Extract metric properties */
      json_value_t* name_val = json_object_get(metric, "name");
      json_value_t* type_val = json_object_get(metric, "type");
      json_value_t* description_val = json_object_get(metric, "description");
      
      if (!name_val || name_val->type != JSON_STRING || 
        !type_val || type_val->type != JSON_STRING) {
        continue; /* Skip metrics with missing or invalid required fields */
      }
      
      /* Create a simple object with name and description */
      json_value_t* metric_info = json_create_object();
      if (!metric_info) {
        continue; /* Skip on memory allocation failure */
      }
      
      /* Add name and description */
      json_object_set(metric_info, "name", json_create_string(name_val->value.string));
      
      if (description_val && description_val->type == JSON_STRING) {
        json_object_set(metric_info, "description", 
                json_create_string(description_val->value.string));
      } else {
        json_object_set(metric_info, "description", json_create_string(""));
      }
      
      /* Add to the appropriate array based on type */
      if (strcmp(type_val->value.string, "counter") == 0) {
        json_array_append(counters_array, metric_info);
      } else if (strcmp(type_val->value.string, "gauge") == 0) {
        json_array_append(gauges_array, metric_info);
      } else if (strcmp(type_val->value.string, "timer") == 0) {
        json_array_append(timers_array, metric_info);
      } else if (strcmp(type_val->value.string, "histogram") == 0) {
        json_array_append(histograms_array, metric_info);
      } else {
        json_free(metric_info); /* Unknown type, free the object */
      }
    }
  }
  
  /* Add arrays to response object */
  json_object_set(response_obj, "counters", counters_array);
  json_object_set(response_obj, "gauges", gauges_array);
  json_object_set(response_obj, "timers", timers_array);
  json_object_set(response_obj, "histograms", histograms_array);
  
  /* Convert to JSON string */
  char* response_json = json_stringify(response_obj);
  
  /* Clean up JSON objects */
  json_free(response_obj);
  json_free(root);
  BUFFER_FREE(metrics_json);
  
  if (!response_json) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to generate response\"}", "application/json");
  }
  
  /* Create response */
  http_response_t* response = create_http_response(HTTP_OK, response_json, "application/json");
  
  /* Free response JSON */
  BUFFER_FREE(response_json);
  
  return response;
#else
  /* In tools build, return empty metrics */
  return create_http_response(HTTP_OK, 
               "{\"counters\":[],\"gauges\":[],\"timers\":[],\"histograms\":[]}", 
               "application/json");
#endif
}

/* Handler for metrics export to file */
http_response_t* health_api_handle_metrics_export(api_context_t *ctx, http_request_t *request) {
  if (!ctx || !request) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
#ifndef TOOLS_BUILD
  if (!g_metrics_registry) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Metrics registry not initialized\"}", "application/json");
  }
  
  /* Check if request is POST with JSON body */
  if (request->method != HTTP_POST) {
    return create_http_response(HTTP_METHOD_NOT_ALLOWED,
                 "{\"error\":\"Method not allowed\"}", "application/json");
  }
  
  /* Parse JSON request body */
  json_value_t* req_body = json_parse(request->body);
  if (!req_body) {
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid JSON request body\"}", "application/json");
  }
  
  /* Extract export path */
  json_value_t* export_path_val = json_object_get(req_body, "export_path");
  if (!export_path_val || export_path_val->type != JSON_STRING) {
    json_free(req_body);
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"export_path is required in request body\"}", 
                 "application/json");
  }
  
  const char* export_path = export_path_val->value.string;
  
  /* Export metrics to file */
  int result = metrics_registry_export(g_metrics_registry, export_path);
  
  /* Clean up */
  json_free(req_body);
  
  if (!result) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 "{\"error\":\"Failed to export metrics to file\"}", 
                 "application/json");
  }
  
  /* Return success response */
  char success_response[256];
  snprintf(success_response, sizeof(success_response),
      "{\"success\":true,\"message\":\"Metrics exported to %s\"}", export_path);
  
  return create_http_response(HTTP_OK, success_response, "application/json");
#else
  /* In tools build, return error */
  return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
               "{\"error\":\"Metrics export not available in tools build\"}", 
               "application/json");
#endif
}

/* Register health API endpoints */
void register_health_api_endpoints(api_context_t *ctx) {
  /* Initialize health API */
  health_api_init();
  
  if (!ctx) {
    if (g_logger) {
      LOG_ERROR("register health API endpoints: NULL context.");
    }
    return;
  }
  
  /* Check if we have enough space for our routes */
  if (ctx->num_routes + 4 > ctx->max_routes) {
    if (g_logger) {
      LOG_ERROR("register health API endpoints: Not enough space in routes array.");
      LOG_ERROR("Current routes: %d, Max routes: %d, Need to add: 4", 
           ctx->num_routes, ctx->max_routes);
    }
    return;
  }
  
  /* Register health check endpoint */
  ctx->routes[ctx->num_routes++] = (api_route_t){"/api/health", HTTP_GET, api_handle_health_check, 0};
  
  /* Register metrics endpoints */
  ctx->routes[ctx->num_routes++] = (api_route_t){"/api/metrics", HTTP_GET, health_api_handle_metrics, 1};
  ctx->routes[ctx->num_routes++] = (api_route_t){"/api/metrics/available", HTTP_GET, health_api_handle_metrics_available, 1};
  ctx->routes[ctx->num_routes++] = (api_route_t){"/api/metrics/export", HTTP_POST, health_api_handle_metrics_export, 1};
  
  if (g_logger) {
    LOG_INFO("Health API endpoints registered - added 4 routes.");
  }
}