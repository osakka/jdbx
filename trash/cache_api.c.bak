#include "api/api.h"
#include "database/database.h"
#include "utils/json.h"
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
    
    /* Get cache statistics */
    json_value_t* stats = db_get_cache_stats(ctx->db);
    if (!stats) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Failed to get cache statistics\"}", "application/json");
    }
    
    /* Serialize response */
    char* response_str = json_stringify(stats);
    
    /* Free JSON object */
    json_free(stats);
    
    /* Create response */
    http_response_t* response = create_http_response(HTTP_OK, response_str, "application/json");
    
    /* Free response string */
    free(response_str);
    
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
        if (body) json_free(body);
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request body\"}", "application/json");
    }
    
    /* Extract configuration parameters */
    json_value_t* enabled_val = json_object_get(body, "enabled");
    
    if (enabled_val && enabled_val->type == JSON_BOOLEAN) {
        int enable = enabled_val->value.boolean;
        
        if (enable) {
            /* Enable cache */
            int capacity = 1000;  /* Default capacity */
            int ttl = 0;          /* Default TTL (no expiration) */
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
            
            /* Enable and configure cache */
            if (!db_enable_cache(ctx->db, capacity, ttl)) {
                json_free(body);
                return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                          "{\"error\":\"Failed to enable cache\"}", "application/json");
            }
            
            /* Configure cache */
            db_configure_cache(ctx->db, capacity, ttl, type, max_memory_mb);
        } else {
            /* Disable cache */
            if (!db_disable_cache(ctx->db)) {
                json_free(body);
                return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                          "{\"error\":\"Failed to disable cache\"}", "application/json");
            }
        }
    } else {
        /* Just reconfigure existing cache */
        int capacity = 0;  /* Don't change */
        int ttl = 0;       /* Don't change */
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
        
        /* Configure cache */
        if (!db_configure_cache(ctx->db, capacity, ttl, type, max_memory_mb)) {
            json_free(body);
            return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                      "{\"error\":\"Failed to configure cache\"}", "application/json");
        }
    }
    
    /* Free request body */
    json_free(body);
    
    /* Get updated cache statistics */
    json_value_t* stats = db_get_cache_stats(ctx->db);
    if (!stats) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Failed to get cache statistics\"}", "application/json");
    }
    
    /* Add success message */
    json_object_set(stats, "success", json_create_boolean(1));
    json_object_set(stats, "message", json_create_string("Cache configured successfully"));
    
    /* Serialize response */
    char* response_str = json_stringify(stats);
    
    /* Free JSON object */
    json_free(stats);
    
    /* Create response */
    http_response_t* response = create_http_response(HTTP_OK, response_str, "application/json");
    
    /* Free response string */
    free(response_str);
    
    return response;
}

/* Clear cache */
http_response_t* api_handle_cache_clear(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Clear cache */
    if (!db_clear_cache(ctx->db)) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Failed to clear cache\"}", "application/json");
    }
    
    /* Create response */
    json_value_t* response = json_create_object();
    json_object_set(response, "success", json_create_boolean(1));
    json_object_set(response, "message", json_create_string("Cache cleared successfully"));
    
    /* Serialize response */
    char* response_str = json_stringify(response);
    
    /* Free JSON object */
    json_free(response);
    
    /* Create response */
    http_response_t* http_response = create_http_response(HTTP_OK, response_str, "application/json");
    
    /* Free response string */
    free(response_str);
    
    return http_response;
}

/* Process cache invalidations */
http_response_t* api_handle_cache_invalidate(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Process cache invalidations */
    int processed = process_cache_invalidations(ctx->db);
    
    /* Create response */
    json_value_t* response = json_create_object();
    json_object_set(response, "success", json_create_boolean(1));
    json_object_set(response, "message", json_create_string("Cache invalidations processed"));
    json_object_set(response, "processed", json_create_integer(processed));
    
    /* Serialize response */
    char* response_str = json_stringify(response);
    
    /* Free JSON object */
    json_free(response);
    
    /* Create response */
    http_response_t* http_response = create_http_response(HTTP_OK, response_str, "application/json");
    
    /* Free response string */
    free(response_str);
    
    return http_response;
}