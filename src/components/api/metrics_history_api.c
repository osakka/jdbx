#include "api/api.h"
#include "utils/metrics_persistence.h"
#include "utils/json.h"
#include "utils/logger.h"
#include "utils/buffer_pool.h"
#include <time.h>
#include <stdlib.h>
#include <string.h>

/**
 * Handle historical metrics request
 * GET /api/metrics/history?start=<timestamp>&end=<timestamp>&metric=<name>
 */
http_response_t* api_handle_metrics_history(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  /* Parse query parameters from URL */
  time_t start_time = 0;
  time_t end_time = time(NULL); /* Default to now */
  const char* metric_name = NULL;
  
  /* Extract query string from path */
  const char* query_start = strchr(request->path, '?');
  if (query_start) {
    char* query_copy = strdup(query_start + 1);
    char* saveptr;
    char* param = strtok_r(query_copy, "&", &saveptr);
    
    while (param) {
      char* equals = strchr(param, '=');
      if (equals) {
        *equals = '\0';
        char* value = equals + 1;
        
        if (strcmp(param, "start") == 0) {
          start_time = (time_t)atol(value);
        } else if (strcmp(param, "end") == 0) {
          end_time = (time_t)atol(value);
        } else if (strcmp(param, "metric") == 0) {
          metric_name = strdup(value);
        }
      }
      param = strtok_r(NULL, "&", &saveptr);
    }
    free(query_copy);
  }
  
  /* Default to last hour if no start time */
  if (start_time == 0) {
    start_time = end_time - 3600;
  }
  
  /* Validate time range */
  if (start_time >= end_time) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid time range\"}", "application/json");
  }
  
  LOG_DEBUG("Fetching metrics history from %ld to %ld", start_time, end_time);
  
  /* Get historical metrics */
  json_value_t* history = metrics_get_historical(start_time, end_time, metric_name);
  if (!history) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to retrieve metrics history\"}", 
                 "application/json");
  }
  
  /* Create response object */
  json_value_t* response = json_create_object();
  json_object_set(response, "start_time", json_create_integer(start_time));
  json_object_set(response, "end_time", json_create_integer(end_time));
  
  if (metric_name) {
    json_object_set(response, "metric", json_create_string(metric_name));
  }
  
  json_object_set(response, "data", history);
  
  /* Serialize response */
  char* response_str = json_stringify(response);
  json_free(response);
  
  if (!response_str) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to serialize response\"}", 
                 "application/json");
  }
  
  /* Create HTTP response */
  http_response_t* http_response = create_http_response(HTTP_OK, response_str, "application/json");
  buffer_pool_free_safe(response_str);
  
  return http_response;
}

/**
 * Handle metrics aggregation request
 * GET /api/metrics/aggregate?metric=<name>&interval=<seconds>&start=<timestamp>&end=<timestamp>
 */
http_response_t* api_handle_metrics_aggregate(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  /* Parse query parameters from URL */
  const char* metric_name = NULL;
  time_t end_time = time(NULL);
  time_t start_time = end_time - 3600; /* Default to last hour */
  int interval = 300; /* Default 5 minutes */
  
  /* Extract query string from path */
  const char* query_start = strchr(request->path, '?');
  if (query_start) {
    char* query_copy = strdup(query_start + 1);
    char* saveptr;
    char* param = strtok_r(query_copy, "&", &saveptr);
    
    while (param) {
      char* equals = strchr(param, '=');
      if (equals) {
        *equals = '\0';
        char* value = equals + 1;
        
        if (strcmp(param, "start") == 0) {
          start_time = (time_t)atol(value);
        } else if (strcmp(param, "end") == 0) {
          end_time = (time_t)atol(value);
        } else if (strcmp(param, "metric") == 0) {
          metric_name = strdup(value);
        } else if (strcmp(param, "interval") == 0) {
          interval = atoi(value);
          if (interval < 60) interval = 60; /* Minimum 1 minute */
        }
      }
      param = strtok_r(NULL, "&", &saveptr);
    }
    free(query_copy);
  }
  
  /* Validate metric name */
  if (!metric_name) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Missing metric parameter\"}", "application/json");
  }
  
  /* Get raw historical data */
  json_value_t* history = metrics_get_historical(start_time, end_time, metric_name);
  if (!history) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to retrieve metrics history\"}", 
                 "application/json");
  }
  
  /* Aggregate data by interval */
  json_value_t* aggregated = json_create_array();
  time_t current_bucket = start_time;
  
  while (current_bucket < end_time) {
    time_t bucket_end = current_bucket + interval;
    double sum = 0;
    int count = 0;
    double min = 0, max = 0;
    int first = 1;
    
    /* Process all points in this interval */
    for (size_t i = 0; i < history->value.array.size; i++) {
      json_value_t* point = json_array_get(history, i);
      json_value_t* timestamp = json_object_get(point, "timestamp");
      json_value_t* value = json_object_get(point, "value");
      
      if (timestamp && value && 
        timestamp->type == JSON_INTEGER &&
        value->type == JSON_NUMBER) {
        
        time_t point_time = (time_t)timestamp->value.integer;
        if (point_time >= current_bucket && point_time < bucket_end) {
          double v = value->value.number;
          sum += v;
          count++;
          
          if (first) {
            min = max = v;
            first = 0;
          } else {
            if (v < min) min = v;
            if (v > max) max = v;
          }
        }
      }
    }
    
    /* Add aggregated point */
    if (count > 0) {
      json_value_t* agg_point = json_create_object();
      json_object_set(agg_point, "timestamp", json_create_integer(current_bucket));
      json_object_set(agg_point, "avg", json_create_number(sum / count));
      json_object_set(agg_point, "min", json_create_number(min));
      json_object_set(agg_point, "max", json_create_number(max));
      json_object_set(agg_point, "count", json_create_integer(count));
      json_array_append(aggregated, agg_point);
    }
    
    current_bucket = bucket_end;
  }
  
  json_free(history);
  
  /* Create response */
  json_value_t* response = json_create_object();
  json_object_set(response, "metric", json_create_string(metric_name));
  json_object_set(response, "interval", json_create_integer(interval));
  json_object_set(response, "start_time", json_create_integer(start_time));
  json_object_set(response, "end_time", json_create_integer(end_time));
  json_object_set(response, "data", aggregated);
  
  /* Serialize response */
  char* response_str = json_stringify(response);
  json_free(response);
  
  if (!response_str) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to serialize response\"}", 
                 "application/json");
  }
  
  /* Create HTTP response */
  http_response_t* http_response = create_http_response(HTTP_OK, response_str, "application/json");
  buffer_pool_free_safe(response_str);
  
  return http_response;
}