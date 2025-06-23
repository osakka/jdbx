/**
 * @file api_metrics.c
 * @brief System Metrics API handlers implementation
 * 
 * Implements comprehensive system metrics collection, querying, and export
 * functionality. Supports both JSON and Prometheus/OpenMetrics formats for
 * monitoring and integration with external systems.
 * 
 * Extracted from api.c as part of Phase 2.5A API module decomposition.
 */

#include "api/api_metrics.h"
#include "api/api.h"
#include "core/server.h"
#include "utils/metrics.h"
#include "utils/logger.h"
#include "utils/buffer_pool.h"
#include "utils/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* External globals declaration */
#ifndef TOOLS_BUILD
extern metrics_registry_t* g_metrics_registry;
#else
metrics_registry_t* g_metrics_registry = NULL;
#endif

/* Metrics handler for authenticated users accessing /api/metrics endpoints */
http_response_t* api_handle_metrics_get(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
#ifndef TOOLS_BUILD
  if (!g_metrics_registry) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Metrics registry not initialized\"}", "application/json");
  }
  
  /* Parse the path to determine which metrics to return */
  char* path = request->path;
  char* category = NULL;
  
  /* Extract the category from the path if present */
  if (strcmp(path, "/api/metrics/stats") == 0) {
    category = "stats";
  } else if (strcmp(path, "/api/metrics/activity") == 0) {
    category = "activity";
  }
  
  /* Check for query parameters */
  char* type_filter = NULL;
  char* name_filter = NULL;
  
  if (request->query) {
    char* query = BUFFER_STRDUP(request->query);
    if (query) {
      char* token = strtok(query, "&");
      while (token) {
        if (strncmp(token, "type=", 5) == 0) {
          type_filter = BUFFER_STRDUP(token + 5);
        } else if (strncmp(token, "name=", 5) == 0) {
          name_filter = BUFFER_STRDUP(token + 5);
        }
        token = strtok(NULL, "&");
      }
      BUFFER_FREE(query);
    }
  }
  
  /* Get all metrics as JSON */
  char* metrics_json = metrics_get_json(g_metrics_registry);
  if (!metrics_json) {
    if (type_filter) BUFFER_FREE(type_filter);
    if (name_filter) BUFFER_FREE(name_filter);
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Failed to get metrics\"}", "application/json");
  }
  
  /* If no filters are specified, return all metrics */
  if (!category && !type_filter && !name_filter) {
    /* Create response */
    http_response_t* response = create_http_response(HTTP_OK, metrics_json, "application/json");
    
    /* Free metrics JSON */
    BUFFER_FREE(metrics_json);
    
    return response;
  }
  
  /* Parse the metrics JSON to filter based on the criteria */
  json_value_t* root = json_parse(metrics_json);
  if (!root) {
    if (type_filter) BUFFER_FREE(type_filter);
    if (name_filter) BUFFER_FREE(name_filter);
    BUFFER_FREE(metrics_json);
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to parse metrics JSON\"}", "application/json");
  }
  
  /* Create a new metrics array for the filtered metrics */
  json_value_t* filtered_metrics = json_create_array();
  if (!filtered_metrics) {
    if (type_filter) BUFFER_FREE(type_filter);
    if (name_filter) BUFFER_FREE(name_filter);
    /* CHECKPOINT: json_free(root); */
    BUFFER_FREE(metrics_json);
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Memory allocation failure\"}", "application/json");
  }
  
  /* Get the metrics array */
  json_value_t* metrics_array = json_object_get(root, "metrics");
  if (metrics_array && metrics_array->type == JSON_ARRAY) {
    for (size_t i = 0; i < metrics_array->value.array.size; i++) {
      json_value_t* metric = json_array_get(metrics_array, i);
      
      /* Extract metric properties for filtering */
      json_value_t* name_val = json_object_get(metric, "name");
      json_value_t* type_val = json_object_get(metric, "type");
      
      if (!name_val || name_val->type != JSON_STRING || 
        !type_val || type_val->type != JSON_STRING) {
        continue; /* Skip metrics with missing or invalid required fields */
      }
      
      const char* name = name_val->value.string;
      const char* type = type_val->value.string;
      
      /* Check if this metric matches the filters */
      int include_metric = 1;
      
      /* Apply category filter */
      if (category) {
        /* For now, we use simple prefix matching for categories */
        if (strcmp(category, "stats") == 0) {
          /* Include metrics related to statistics */
          if (!(strstr(name, "server_") == name || 
             strstr(name, "db_") == name || 
             strstr(name, "system_") == name)) {
            include_metric = 0;
          }
        } else if (strcmp(category, "activity") == 0) {
          /* Include metrics related to activity */
          if (!(strstr(name, "active_") == name || 
             strstr(name, "requests_") == name || 
             strstr(name, "operations_") == name || 
             strstr(name, "queries_") == name)) {
            include_metric = 0;
          }
        }
      }
      
      /* Apply type filter */
      if (include_metric && type_filter) {
        if (strcmp(type, type_filter) != 0) {
          include_metric = 0;
        }
      }
      
      /* Apply name filter */
      if (include_metric && name_filter) {
        if (strstr(name, name_filter) == NULL) {
          include_metric = 0;
        }
      }
      
      /* If all filters pass, include this metric */
      if (include_metric) {
        /* Create a deep copy of the metric */
        json_value_t* metric_copy = json_clone(metric);
        if (metric_copy) {
          json_array_append(filtered_metrics, metric_copy);
        }
      }
    }
  }
  
  /* Create a new JSON object with the filtered metrics */
  json_value_t* filtered_obj = json_create_object();
  if (!filtered_obj) {
    if (type_filter) BUFFER_FREE(type_filter);
    if (name_filter) BUFFER_FREE(name_filter);
    /* CHECKPOINT: json_free(filtered_metrics); */
    /* CHECKPOINT: json_free(root); */
    BUFFER_FREE(metrics_json);
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Memory allocation failure\"}", "application/json");
  }
  
  json_object_set(filtered_obj, "metrics", filtered_metrics);
  
  /* Convert to JSON string */
  char* filtered_json = json_stringify(filtered_obj);
  
  /* Clean up */
  if (type_filter) BUFFER_FREE(type_filter);
  if (name_filter) BUFFER_FREE(name_filter);
  /* CHECKPOINT: json_free(filtered_obj); */ /* This will also free filtered_metrics */
  /* CHECKPOINT: json_free(root); */
  BUFFER_FREE(metrics_json);
  
  if (!filtered_json) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to generate response\"}", "application/json");
  }
  
  /* Create response */
  http_response_t* response = create_http_response(HTTP_OK, filtered_json, "application/json");
  
  /* Free filtered JSON */
  BUFFER_FREE(filtered_json);
  
  return response;
#else
  /* In tools build, return empty metrics */
  return create_http_response(HTTP_OK, "{\"metrics\":[]}", "application/json");
#endif
}

/* Metrics handler for public /metrics endpoint (Prometheus/OpenMetrics format) */
http_response_t* api_handle_metrics(api_context_t* ctx, http_request_t* request) {
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
    /* CHECKPOINT: json_free(root); */
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
      json_value_t* type_val = json_object_get(metric, "type");
      json_value_t* value_val = json_object_get(metric, "value");
      json_value_t* help_val = json_object_get(metric, "help");
      
      if (!name_val || name_val->type != JSON_STRING || 
        !type_val || type_val->type != JSON_STRING ||
        !value_val) {
        continue; /* Skip metrics with missing or invalid required fields */
      }
      
      const char* name = name_val->value.string;
      const char* type = type_val->value.string;
      const char* help = (help_val && help_val->type == JSON_STRING) ? 
                         help_val->value.string : "No description available";
      
      /* Format for Prometheus */
      char metric_line[512];
      int line_len = 0;
      
      /* Add help line if available */
      if (help && strlen(help) > 0) {
        line_len = snprintf(metric_line, sizeof(metric_line), 
                           "# HELP %s %s\n", name, help);
        if (buffer_used + line_len < buffer_size - 1) {
          strcat(prom_buffer, metric_line);
          buffer_used += line_len;
        }
      }
      
      /* Add type line */
      line_len = snprintf(metric_line, sizeof(metric_line), 
                         "# TYPE %s %s\n", name, type);
      if (buffer_used + line_len < buffer_size - 1) {
        strcat(prom_buffer, metric_line);
        buffer_used += line_len;
      }
      
      /* Add metric value line */
      if (value_val->type == JSON_INTEGER) {
        line_len = snprintf(metric_line, sizeof(metric_line), 
                           "%s %ld\n", name, value_val->value.integer);
      } else if (value_val->type == JSON_NUMBER) {
        line_len = snprintf(metric_line, sizeof(metric_line), 
                           "%s %f\n", name, value_val->value.number);
      } else {
        /* Default to 0 for non-numeric values */
        line_len = snprintf(metric_line, sizeof(metric_line), 
                           "%s 0\n", name);
      }
      
      if (buffer_used + line_len < buffer_size - 1) {
        strcat(prom_buffer, metric_line);
        buffer_used += line_len;
      }
    }
  }
  
  /* Clean up JSON resources */
  /* CHECKPOINT: json_free(root); */
  BUFFER_FREE(metrics_json);
  
  /* Create response with Prometheus content type */
  http_response_t* response = create_http_response(HTTP_OK, prom_buffer, "text/plain");
  
  /* Free Prometheus buffer */
  BUFFER_FREE(prom_buffer);
  
  return response;
#else
  /* In tools build, return empty metrics */
  return create_http_response(HTTP_OK, "# Empty metrics\n", "text/plain");
#endif
}

/* Handler for metrics export to file */
http_response_t* api_handle_metrics_export(api_context_t* ctx, http_request_t* request) {
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
    /* CHECKPOINT: json_free(req_body); */
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"export_path is required in request body\"}", 
                 "application/json");
  }
  
  const char* export_path = export_path_val->value.string;
  
  /* Export metrics to file */
  int result = metrics_registry_export(g_metrics_registry, export_path);
  
  /* Clean up */
  /* CHECKPOINT: json_free(req_body); */
  
  if (!result) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 "{\"error\":\"Failed to export metrics to file\"}", 
                 "application/json");
  }
  
  /* Return success response */
  char success_response[256];
  snprintf(success_response, sizeof(success_response), 
           "{\"message\":\"Metrics exported successfully\",\"export_path\":\"%s\"}", 
           export_path);
  
  return create_http_response(HTTP_OK, success_response, "application/json");
#else
  /* In tools build, metrics export is not available */
  return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
               "{\"error\":\"Metrics export not available in tools build\"}", 
               "application/json");
#endif
}

/* Handler to list available metrics */
http_response_t* api_handle_metrics_available(api_context_t* ctx, http_request_t* request) {
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
    /* CHECKPOINT: json_free(root); */
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
    if (counters_array) {
      /* CHECKPOINT: json_free(counters_array); */
    }
    if (gauges_array) {
      /* CHECKPOINT: json_free(gauges_array); */
    }
    if (timers_array) {
      /* CHECKPOINT: json_free(timers_array); */
    }
    if (histograms_array) {
      /* CHECKPOINT: json_free(histograms_array); */
    }
    /* CHECKPOINT: json_free(response_obj); */
    /* CHECKPOINT: json_free(root); */
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
      json_value_t* help_val = json_object_get(metric, "help");
      
      if (!name_val || name_val->type != JSON_STRING || 
        !type_val || type_val->type != JSON_STRING) {
        continue; /* Skip metrics with missing or invalid required fields */
      }
      
      const char* name = name_val->value.string;
      const char* type = type_val->value.string;
      const char* help = (help_val && help_val->type == JSON_STRING) ? 
                         help_val->value.string : "No description available";
      
      /* Create metric info object */
      json_value_t* metric_info = json_create_object();
      if (metric_info) {
        json_object_set(metric_info, "name", json_create_string(name));
        json_object_set(metric_info, "description", json_create_string(help));
        
        /* Add to appropriate type array */
        if (strcmp(type, "counter") == 0) {
          json_array_append(counters_array, metric_info);
        } else if (strcmp(type, "gauge") == 0) {
          json_array_append(gauges_array, metric_info);
        } else if (strcmp(type, "timer") == 0) {
          json_array_append(timers_array, metric_info);
        } else if (strcmp(type, "histogram") == 0) {
          json_array_append(histograms_array, metric_info);
        } else {
          /* Default to gauge for unknown types */
          json_array_append(gauges_array, metric_info);
        }
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
  /* CHECKPOINT: json_free(response_obj); */
  /* CHECKPOINT: json_free(root); */
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