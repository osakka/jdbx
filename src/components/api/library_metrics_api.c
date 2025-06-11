#include "api/api.h"
#include "utils/library_metrics.h"
#include "utils/json.h"
#include "utils/logger.h"
#include <string.h>
#include <stdlib.h>

/**
 * Handle library metrics request
 * GET /api/libraries/{library}/metrics
 */
http_response_t* api_handle_library_metrics(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !ctx->db || !request) {
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  /* Extract library name from URL: /api/libraries/{library}/metrics */
  const char* prefix = "/api/libraries/";
  if (strncmp(request->path, prefix, strlen(prefix)) != 0) {
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid URL format\"}", "application/json");
  }
  
  const char* after_prefix = request->path + strlen(prefix);
  const char* metrics_part = strstr(after_prefix, "/metrics");
  if (!metrics_part) {
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid URL format\"}", "application/json");
  }
  
  size_t library_name_len = metrics_part - after_prefix;
  if (library_name_len == 0) {
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Library name required\"}", "application/json");
  }
  
  char* library_name_copy = malloc(library_name_len + 1);
  if (!library_name_copy) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 "{\"error\":\"Memory allocation failed\"}", "application/json");
  }
  
  strncpy(library_name_copy, after_prefix, library_name_len);
  library_name_copy[library_name_len] = '\0';
  
  /* Get aggregated metrics for the library */
  json_value_t* metrics = library_metrics_aggregate(ctx->db, library_name_copy);
  free(library_name_copy);
  
  if (!metrics) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 "{\"error\":\"Failed to retrieve metrics\"}", "application/json");
  }
  
  char* response_str = json_stringify(metrics);
  json_free(metrics);
  
  return create_http_response(HTTP_OK, response_str, "application/json");
}

/**
 * Handle library metrics recording
 * POST /api/libraries/{library}/metrics
 */
http_response_t* api_handle_record_library_metric(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !ctx->db || !request) {
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  /* Extract library name from URL: /api/libraries/{library}/metrics */
  const char* prefix = "/api/libraries/";
  if (strncmp(request->path, prefix, strlen(prefix)) != 0) {
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid URL format\"}", "application/json");
  }
  
  const char* after_prefix = request->path + strlen(prefix);
  const char* metrics_part = strstr(after_prefix, "/metrics");
  if (!metrics_part) {
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid URL format\"}", "application/json");
  }
  
  size_t library_name_len = metrics_part - after_prefix;
  if (library_name_len == 0) {
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Library name required\"}", "application/json");
  }
  
  char* library_name_copy = malloc(library_name_len + 1);
  if (!library_name_copy) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 "{\"error\":\"Memory allocation failed\"}", "application/json");
  }
  
  strncpy(library_name_copy, after_prefix, library_name_len);
  library_name_copy[library_name_len] = '\0';
  
  /* Parse request body */
  json_value_t* body = json_parse(request->body);
  if (!body || body->type != JSON_OBJECT) {
    free(library_name_copy);
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid JSON body\"}", "application/json");
  }
  
  /* Get metric type */
  json_value_t* metric_type_val = json_object_get(body, "metric_type");
  if (!metric_type_val || metric_type_val->type != JSON_STRING) {
    json_free(body);
    free(library_name_copy);
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"metric_type required\"}", "application/json");
  }
  
  const char* metric_type = metric_type_val->value.string;
  
  /* Get metric data */
  json_value_t* metric_data = json_object_get(body, "data");
  if (!metric_data) {
    json_free(body);
    free(library_name_copy);
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"data required\"}", "application/json");
  }
  
  /* Record the metric */
  int success = library_metrics_record(ctx->db, library_name_copy, metric_type, metric_data);
  
  json_free(body);
  free(library_name_copy);
  
  if (success) {
    return create_http_response(HTTP_CREATED,
                 "{\"success\":true,\"message\":\"Metric recorded\"}", "application/json");
  } else {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 "{\"error\":\"Failed to record metric\"}", "application/json");
  }
}

/**
 * Handle library metrics query
 * GET /api/libraries/{library}/metrics/query?type={type}&limit={limit}
 */
http_response_t* api_handle_query_library_metrics(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !ctx->db || !request) {
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  /* Extract library name from URL: /api/libraries/{library}/metrics/query */
  const char* prefix = "/api/libraries/";
  const char* base_path = request->path;
  const char* query_start = strchr(base_path, '?');
  
  if (strncmp(base_path, prefix, strlen(prefix)) != 0) {
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid URL format\"}", "application/json");
  }
  
  const char* after_prefix = base_path + strlen(prefix);
  const char* metrics_part = strstr(after_prefix, "/metrics/query");
  if (!metrics_part) {
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid URL format\"}", "application/json");
  }
  
  size_t library_name_len = metrics_part - after_prefix;
  if (library_name_len == 0) {
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Library name required\"}", "application/json");
  }
  
  char* library_name_copy = malloc(library_name_len + 1);
  if (!library_name_copy) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 "{\"error\":\"Memory allocation failed\"}", "application/json");
  }
  
  strncpy(library_name_copy, after_prefix, library_name_len);
  library_name_copy[library_name_len] = '\0';
  
  /* Parse query parameters */
  const char* metric_type = NULL;
  int limit = 100; /* Default limit */
  
  if (query_start) {
    char* query_copy = strdup(query_start + 1);
    char* saveptr;
    char* param = strtok_r(query_copy, "&", &saveptr);
    
    while (param) {
      char* equals = strchr(param, '=');
      if (equals) {
        *equals = '\0';
        char* value = equals + 1;
        
        if (strcmp(param, "type") == 0) {
          metric_type = strdup(value);
        } else if (strcmp(param, "limit") == 0) {
          limit = atoi(value);
        }
      }
      param = strtok_r(NULL, "&", &saveptr);
    }
    free(query_copy);
  }
  
  /* Query metrics */
  json_value_t* results = library_metrics_query(ctx->db, library_name_copy, metric_type, limit);
  free(library_name_copy);
  
  if (metric_type) {
    free((void*)metric_type);
  }
  
  if (!results) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 "{\"error\":\"Failed to query metrics\"}", "application/json");
  }
  
  char* response_str = json_stringify(results);
  json_free(results);
  
  return create_http_response(HTTP_OK, response_str, "application/json");
}