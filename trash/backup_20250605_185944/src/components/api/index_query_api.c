#include "api/api.h"
#include "database/database.h"
#include "utils/json.h"
#include "utils/buffer_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Helper function to extract collection name from path */
static char* extract_collection_name(const char* path) {
  if (!path) {
    return NULL;
  }
  
  /* Expected format: /api/indexes/query/{collection} */
  const char* prefix = "/api/indexes/query/";
  size_t prefix_len = strlen(prefix);
  
  if (strncmp(path, prefix, prefix_len) != 0) {
    return NULL;
  }
  
  /* Collection name is the rest of the path */
  return strdup(path + prefix_len);
}

/* Parse limit and skip parameters */
#ifdef INCLUDE_PAGINATION_PARSING
static void parse_pagination(const char* query_str, size_t* limit, size_t* skip) {
  if (!query_str || !limit || !skip) {
    return;
  }
  
  /* Default values */
  *limit = 0; /* No limit */
  *skip = 0;  /* No skip */
  
  /* Parse query string for limit and skip parameters */
  char* query = strdup(query_str);
  char* token = strtok(query, "&");
  
  while (token) {
    if (strncmp(token, "limit=", 6) == 0) {
      *limit = (size_t)atoi(token + 6);
    } else if (strncmp(token, "skip=", 5) == 0) {
      *skip = (size_t)atoi(token + 5);
    }
    token = strtok(NULL, "&");
  }
  
  free(query);
}
#endif

/* Execute a query using an index */
http_response_t* api_handle_index_query(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request || !request->body) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  /* Extract collection name */
  char* collection = extract_collection_name(request->path);
  if (!collection) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid path format\"}", "application/json");
  }
  
  /* Parse request body */
  json_value_t* body = json_parse(request->body);
  if (!body || body->type != JSON_OBJECT) {
    free(collection);
    if (body) json_free(body);
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request body\"}", "application/json");
  }
  
  /* Extract field path */
  json_value_t* field_val = json_object_get(body, "field");
  if (!field_val || field_val->type != JSON_STRING) {
    free(collection);
    json_free(body);
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Field path is required\"}", "application/json");
  }
  
  const char* field_path = field_val->value.string;
  
  /* Extract value to search for */
  json_value_t* value_val = json_object_get(body, "value");
  if (!value_val) {
    free(collection);
    json_free(body);
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Search value is required\"}", "application/json");
  }
  
  /* Convert value to string */
  char* value_str = NULL;
  
  if (value_val->type == JSON_STRING) {
    value_str = strdup(value_val->value.string);
  } else if (value_val->type == JSON_NUMBER) {
    value_str = malloc(64);
    if (value_str) {
      snprintf(value_str, 64, "%f", value_val->value.number);
    }
  } else if (value_val->type == JSON_INTEGER) {
    value_str = malloc(64);
    if (value_str) {
      snprintf(value_str, 64, "%lld", (long long)value_val->value.integer);
    }
  } else if (value_val->type == JSON_BOOLEAN) {
    value_str = strdup(value_val->value.boolean ? "true" : "false");
  } else if (value_val->type == JSON_NULL) {
    value_str = strdup("null");
  } else {
    /* Complex types not supported */
    free(collection);
    json_free(body);
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Unsupported value type\"}", "application/json");
  }
  
  if (!value_str) {
    free(collection);
    json_free(body);
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Memory allocation failed\"}", "application/json");
  }
  
  /* Parse pagination parameters */
  size_t limit = 0;
  size_t skip = 0;
  
  json_value_t* limit_val = json_object_get(body, "limit");
  if (limit_val && (limit_val->type == JSON_NUMBER || limit_val->type == JSON_INTEGER)) {
    limit = (limit_val->type == JSON_NUMBER) ? 
        (size_t)limit_val->value.number : (size_t)limit_val->value.integer;
  }
  
  json_value_t* skip_val = json_object_get(body, "skip");
  if (skip_val && (skip_val->type == JSON_NUMBER || skip_val->type == JSON_INTEGER)) {
    skip = (skip_val->type == JSON_NUMBER) ? 
        (size_t)skip_val->value.number : (size_t)skip_val->value.integer;
  }
  
  /* Free the request body as we don't need it anymore */
  json_free(body);
  
  /* Execute index query */
  json_value_t* results = db_query_by_index(ctx->db, collection, field_path, value_str, limit, skip);
  
  /* Free resources */
  free(collection);
  free(value_str);
  
  if (!results) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Query failed\"}", "application/json");
  }
  
  /* Create response */
  json_value_t* response = json_create_object();
  json_object_set(response, "documents", results);
  json_object_set(response, "count", json_create_integer(json_array_size(results)));
  
  /* Serialize response */
  char* response_str = json_stringify(response);
  
  /* Free resources */
  json_free(response);
  
  /* Create HTTP response */
  http_response_t* http_response = create_http_response(HTTP_OK, response_str, "application/json");
  
  /* Free response string */
  buffer_pool_free_safe(response_str);
  
  return http_response;
}

/* Perform a compound query (AND operation) */
http_response_t* api_handle_index_compound_query(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request || !request->body) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  /* Extract collection name */
  char* collection = extract_collection_name(request->path);
  if (!collection) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid path format\"}", "application/json");
  }
  
  /* Parse request body */
  json_value_t* body = json_parse(request->body);
  if (!body || body->type != JSON_OBJECT) {
    free(collection);
    if (body) json_free(body);
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request body\"}", "application/json");
  }
  
  /* Extract queries array */
  json_value_t* queries_val = json_object_get(body, "queries");
  if (!queries_val || queries_val->type != JSON_ARRAY || json_array_size(queries_val) == 0) {
    free(collection);
    json_free(body);
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Queries array is required\"}", "application/json");
  }
  
  /* Operation type is fixed to AND for now */
  /* Future enhancement: Support OR operations */
  
  /* Parse pagination parameters */
  size_t limit = 0;
  size_t skip = 0;
  
  json_value_t* limit_val = json_object_get(body, "limit");
  if (limit_val && (limit_val->type == JSON_NUMBER || limit_val->type == JSON_INTEGER)) {
    limit = (limit_val->type == JSON_NUMBER) ? 
        (size_t)limit_val->value.number : (size_t)limit_val->value.integer;
  }
  
  json_value_t* skip_val = json_object_get(body, "skip");
  if (skip_val && (skip_val->type == JSON_NUMBER || skip_val->type == JSON_INTEGER)) {
    skip = (skip_val->type == JSON_NUMBER) ? 
        (size_t)skip_val->value.number : (size_t)skip_val->value.integer;
  }
  
  /* Convert queries array to a real query object for database */
  json_value_t* query = json_create_object();
  if (!query) {
    free(collection);
    json_free(body);
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Memory allocation failed\"}", "application/json");
  }
  
  /* For now, we'll handle this as a simple AND query */
  for (size_t i = 0; i < json_array_size(queries_val); i++) {
    json_value_t* q = json_array_get(queries_val, i);
    if (q && q->type == JSON_OBJECT) {
      json_value_t* field_val = json_object_get(q, "field");
      json_value_t* value_val = json_object_get(q, "value");
      
      if (field_val && field_val->type == JSON_STRING && value_val) {
        /* Add this condition to the query */
        json_object_set(query, field_val->value.string, json_clone(value_val));
      }
    }
  }
  
  /* Free the request body as we don't need it anymore */
  json_free(body);
  
  /* Execute database query */
  json_value_t* results = db_query_documents(ctx->db, collection, query);
  
  /* Free resources */
  json_free(query);
  free(collection);
  
  if (!results) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Query failed\"}", "application/json");
  }
  
  /* Apply pagination if needed */
  if (skip > 0 || limit > 0) {
    json_value_t* paginated = json_create_array();
    if (!paginated) {
      json_free(results);
      return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                   "{\"error\":\"Memory allocation failed\"}", "application/json");
    }
    
    size_t count = json_array_size(results);
    for (size_t i = skip; i < count && (limit == 0 || i < skip + limit); i++) {
      json_array_append(paginated, json_clone(json_array_get(results, i)));
    }
    
    json_free(results);
    results = paginated;
  }
  
  /* Create response */
  json_value_t* response = json_create_object();
  json_object_set(response, "documents", results);
  json_object_set(response, "count", json_create_integer(json_array_size(results)));
  
  /* Serialize response */
  char* response_str = json_stringify(response);
  
  /* Free resources */
  json_free(response);
  
  /* Create HTTP response */
  http_response_t* http_response = create_http_response(HTTP_OK, response_str, "application/json");
  
  /* Free response string */
  buffer_pool_free_safe(response_str);
  
  return http_response;
}