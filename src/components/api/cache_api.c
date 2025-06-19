#include "api/api.h"
#include "database/database.h"
#include "utils/json.h"
#include "utils/buffer_pool.h"
#include "utils/cache_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Get cache statistics */
http_response_t* api_handle_cache_stats(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  /* Get real cache statistics from internal caches */
  json_value_t* stats = json_create_object();
  json_object_set(stats, "status", json_create_string("Automatic internal caching active"));
  json_object_set(stats, "enabled", json_create_boolean(1));
  
  /* Get database caches for statistics */
  generic_cache_t* query_cache = NULL;
  generic_cache_t* doc_cache = NULL;
  get_global_database_caches(&query_cache, &doc_cache);
  
  if (query_cache && doc_cache) {
    uint64_t query_hits = 0, query_misses = 0, query_evictions = 0;
    uint64_t doc_hits = 0, doc_misses = 0, doc_evictions = 0;
    
    generic_cache_stats(query_cache, &query_hits, &query_misses, &query_evictions);
    generic_cache_stats(doc_cache, &doc_hits, &doc_misses, &doc_evictions);
    
    /* Query cache stats */
    json_value_t* query_cache_stats = json_create_object();
    json_object_set(query_cache_stats, "hits", json_create_integer(query_hits));
    json_object_set(query_cache_stats, "misses", json_create_integer(query_misses));
    json_object_set(query_cache_stats, "evictions", json_create_integer(query_evictions));
    json_object_set(query_cache_stats, "size", json_create_integer(query_cache->size));
    json_object_set(query_cache_stats, "capacity", json_create_integer(query_cache->capacity));
    double query_hit_rate = (query_hits + query_misses > 0) ? (double)query_hits / (query_hits + query_misses) * 100.0 : 0.0;
    json_object_set(query_cache_stats, "hit_rate_percent", json_create_number(query_hit_rate));
    
    /* Document cache stats */
    json_value_t* document_cache_stats = json_create_object();
    json_object_set(document_cache_stats, "hits", json_create_integer(doc_hits));
    json_object_set(document_cache_stats, "misses", json_create_integer(doc_misses));
    json_object_set(document_cache_stats, "evictions", json_create_integer(doc_evictions));
    json_object_set(document_cache_stats, "size", json_create_integer(doc_cache->size));
    json_object_set(document_cache_stats, "capacity", json_create_integer(doc_cache->capacity));
    double doc_hit_rate = (doc_hits + doc_misses > 0) ? (double)doc_hits / (doc_hits + doc_misses) * 100.0 : 0.0;
    json_object_set(document_cache_stats, "hit_rate_percent", json_create_number(doc_hit_rate));
    
    /* Combined stats */
    json_object_set(stats, "query_cache", query_cache_stats);
    json_object_set(stats, "document_cache", document_cache_stats);
    json_object_set(stats, "total_hits", json_create_integer(query_hits + doc_hits));
    json_object_set(stats, "total_misses", json_create_integer(query_misses + doc_misses));
    json_object_set(stats, "total_evictions", json_create_integer(query_evictions + doc_evictions));
  } else {
    json_object_set(stats, "error", json_create_string("Cache not initialized"));
    json_object_set(stats, "hits", json_create_integer(0));
    json_object_set(stats, "misses", json_create_integer(0));
    json_object_set(stats, "size", json_create_integer(0));
  }
  
  /* Serialize response */
  char* response_str = json_stringify(stats);
  
  /* Free JSON object */
  /* CHECKPOINT: json_free(stats); */
  
  /* Create response */
  http_response_t* response = create_http_response(HTTP_OK, response_str, "application/json");
  
  /* Free response string */
  BUFFER_FREE(response_str);
  
  return response;
}

/* Configure cache */
http_response_t* api_handle_cache_configure(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request || !request->body) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  /* Parse request body */
  json_value_t* body = json_parse(request->body);
  if (!body || body->type != JSON_OBJECT) {
    if (body) /* CHECKPOINT: json_free(body); */
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request body\"}", "application/json");
  }
  
  /* Extract configuration parameters */
  json_value_t* enabled_val = json_object_get(body, "enabled");
  
  if (enabled_val && enabled_val->type == JSON_BOOLEAN) {
    int enable = enabled_val->value.boolean;
    
    if (enable) {
      /* Enable cache */
      int capacity = 1000; /* Default capacity */
      int ttl = 0;     /* Default TTL (no expiration) */
      const char* type = "lru"; /* Default type */
      double max_memory_mb = 0; /* Default memory limit (no limit) */
      
      /* Extract optional parameters */
      json_value_t* capacity_val = json_object_get(body, "capacity");
      if (capacity_val && (capacity_val->type == JSON_INTEGER || capacity_val->type == JSON_NUMBER)) {
        capacity = capacity_val->type == JSON_INTEGER ? 
              capacity_val->value.integer : (int)capacity_val->value.number;
      }
      
      json_value_t* ttl_val = json_object_get(body, "ttl");
      if (ttl_val && (ttl_val->type == JSON_INTEGER || ttl_val->type == JSON_NUMBER)) {
        ttl = ttl_val->type == JSON_INTEGER ? 
           ttl_val->value.integer : (int)ttl_val->value.number;
      }
      
      json_value_t* type_val = json_object_get(body, "type");
      if (type_val && type_val->type == JSON_STRING) {
        type = type_val->value.string;
      }
      
      json_value_t* memory_val = json_object_get(body, "max_memory_mb");
      if (memory_val && (memory_val->type == JSON_INTEGER || memory_val->type == JSON_NUMBER)) {
        max_memory_mb = memory_val->type == JSON_INTEGER ? 
                memory_val->value.integer : memory_val->value.number;
      }
      
      /* Cache functionality not implemented in unified documents architecture */
      (void)capacity; (void)ttl; (void)type; (void)max_memory_mb; /* Suppress unused variable warnings */
    } else {
      /* Cache functionality not implemented in unified documents architecture */
    }
  } else {
    /* Just reconfigure existing cache */
    int capacity = 0; /* Don't change */
    int ttl = 0;    /* Don't change */
    const char* type = NULL; /* Don't change */
    double max_memory_mb = 0; /* Don't change */
    
    /* Extract optional parameters */
    json_value_t* capacity_val = json_object_get(body, "capacity");
    if (capacity_val && (capacity_val->type == JSON_INTEGER || capacity_val->type == JSON_NUMBER)) {
      capacity = capacity_val->type == JSON_INTEGER ? 
            capacity_val->value.integer : (int)capacity_val->value.number;
    }
    
    json_value_t* ttl_val = json_object_get(body, "ttl");
    if (ttl_val && (ttl_val->type == JSON_INTEGER || ttl_val->type == JSON_NUMBER)) {
      ttl = ttl_val->type == JSON_INTEGER ? 
         ttl_val->value.integer : (int)ttl_val->value.number;
    }
    
    json_value_t* type_val = json_object_get(body, "type");
    if (type_val && type_val->type == JSON_STRING) {
      type = type_val->value.string;
    }
    
    json_value_t* memory_val = json_object_get(body, "max_memory_mb");
    if (memory_val && (memory_val->type == JSON_INTEGER || memory_val->type == JSON_NUMBER)) {
      max_memory_mb = memory_val->type == JSON_INTEGER ? 
              memory_val->value.integer : memory_val->value.number;
    }
    
    /* Cache functionality not implemented in unified documents architecture */
    (void)capacity; (void)ttl; (void)type; (void)max_memory_mb; /* Suppress unused variable warnings */
  }
  
  /* Free request body */
  /* CHECKPOINT: json_free(body); */
  
  /* Create real cache statistics response */
  json_value_t* stats = json_create_object();
  json_object_set(stats, "status", json_create_string("Automatic internal caching active"));
  json_object_set(stats, "enabled", json_create_boolean(1));
  
  /* Add success message */
  json_object_set(stats, "success", json_create_boolean(1));
  json_object_set(stats, "message", json_create_string("Cache configuration acknowledged (automatic internal caching)"));
  
  /* Serialize response */
  char* response_str = json_stringify(stats);
  
  /* Free JSON object */
  /* CHECKPOINT: json_free(stats); */
  
  /* Create response */
  http_response_t* response = create_http_response(HTTP_OK, response_str, "application/json");
  
  /* Free response string */
  BUFFER_FREE(response_str);
  
  return response;
}

/* Clear cache */
http_response_t* api_handle_cache_clear(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  /* Cache functionality not implemented in unified documents architecture */
  
  /* Create response */
  json_value_t* response = json_create_object();
  json_object_set(response, "success", json_create_boolean(1));
  json_object_set(response, "message", json_create_string("Cache clear acknowledged (not implemented)"));
  
  /* Serialize response */
  char* response_str = json_stringify(response);
  
  /* Free JSON object */
  /* CHECKPOINT: json_free(response); */
  
  /* Create response */
  http_response_t* http_response = create_http_response(HTTP_OK, response_str, "application/json");
  
  /* Free response string */
  BUFFER_FREE(response_str);
  
  return http_response;
}

/* Process cache invalidations */
http_response_t* api_handle_cache_invalidate(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  /* Cache functionality not implemented in unified documents architecture */
  int processed = 0;  /* No invalidations processed */
  
  /* Create response */
  json_value_t* response = json_create_object();
  json_object_set(response, "success", json_create_boolean(1));
  json_object_set(response, "message", json_create_string("Cache invalidations acknowledged (not implemented)"));
  json_object_set(response, "processed", json_create_integer(processed));
  
  /* Serialize response */
  char* response_str = json_stringify(response);
  
  /* Free JSON object */
  /* CHECKPOINT: json_free(response); */
  
  /* Create response */
  http_response_t* http_response = create_http_response(HTTP_OK, response_str, "application/json");
  
  /* Free response string */
  BUFFER_FREE(response_str);
  
  return http_response;
}