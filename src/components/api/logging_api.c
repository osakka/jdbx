#include "api/logging_api.h"
#include "utils/logger.h"
#include "utils/json.h"
#include "api/api.h"
#include "utils/buffer_pool.h"
#include "core/server.h"
#include <string.h>

/* API endpoint to get current logging configuration */
static http_response_t* handle_get_logging_config(api_context_t* ctx, 
                                                  http_request_t* request) {
    (void)ctx;
    (void)request;

    LOG_DEBUG("Retrieving current logging configuration");

    /* Build response JSON */
    json_value_t* response = json_create_object();
    
    /* Get current log level */
    const char* level_str;
    log_level_t current_level = logger_get_level();
    switch (current_level) {
        case LOG_LEVEL_ERROR:   level_str = "ERROR"; break;
        case LOG_LEVEL_WARNING: level_str = "WARNING"; break;
        case LOG_LEVEL_INFO:    level_str = "INFO"; break;
        case LOG_LEVEL_DEBUG:   level_str = "DEBUG"; break;
        case LOG_LEVEL_TRACE:   level_str = "TRACE"; break;
        default:                level_str = "NONE"; break;
    }
    json_object_set(response, "level", json_create_string(level_str));
    
    /* Get current trace categories */
    json_value_t* categories = json_create_array();
    uint32_t trace_mask = logger_get_trace_mask();
    
    if (trace_mask & TRACE_DATABASE) json_array_append(categories, json_create_string("database"));
    if (trace_mask & TRACE_RBAC) json_array_append(categories, json_create_string("rbac"));
    if (trace_mask & TRACE_API) json_array_append(categories, json_create_string("api"));
    if (trace_mask & TRACE_AUTH) json_array_append(categories, json_create_string("auth"));
    if (trace_mask & TRACE_TRANSACTION) json_array_append(categories, json_create_string("transaction"));
    if (trace_mask & TRACE_BINARY) json_array_append(categories, json_create_string("binary"));
    if (trace_mask & TRACE_JAVASCRIPT) json_array_append(categories, json_create_string("javascript"));
    if (trace_mask & TRACE_NETWORK) json_array_append(categories, json_create_string("network"));
    if (trace_mask & TRACE_METRICS) json_array_append(categories, json_create_string("metrics"));
    if (trace_mask & TRACE_MEMORY) json_array_append(categories, json_create_string("memory"));
    
    json_object_set(response, "trace_categories", categories);
    
    /* Add available log levels */
    json_value_t* available_levels = json_create_array();
    json_array_append(available_levels, json_create_string("ERROR"));
    json_array_append(available_levels, json_create_string("WARNING"));
    json_array_append(available_levels, json_create_string("INFO"));
    json_array_append(available_levels, json_create_string("DEBUG"));
    json_array_append(available_levels, json_create_string("TRACE"));
    json_object_set(response, "available_levels", available_levels);
    
    /* Add available trace categories */
    json_value_t* available_categories = json_create_array();
    json_array_append(available_categories, json_create_string("database"));
    json_array_append(available_categories, json_create_string("rbac"));
    json_array_append(available_categories, json_create_string("api"));
    json_array_append(available_categories, json_create_string("auth"));
    json_array_append(available_categories, json_create_string("transaction"));
    json_array_append(available_categories, json_create_string("binary"));
    json_array_append(available_categories, json_create_string("javascript"));
    json_array_append(available_categories, json_create_string("network"));
    json_array_append(available_categories, json_create_string("metrics"));
    json_array_append(available_categories, json_create_string("memory"));
    json_object_set(response, "available_categories", available_categories);
    
    /* Convert response object to JSON string */
    char* json_str = json_stringify(response);
    
    /* Create HTTP response */
    http_response_t* http_response = create_http_response(HTTP_OK, json_str, "application/json");
    
    /* CHECKPOINT: json_free(response); */
    
    return http_response;
}

/* API endpoint to update logging configuration */
static http_response_t* handle_update_logging_config(api_context_t* ctx,
                                                    http_request_t* request) {
    (void)ctx;

    LOG_INFO("Updating logging configuration");

    /* Parse request body */
    if (!request->body || request->content_length == 0) {
        LOG_WARNING("Empty body in logging configuration update request");
        char* error_json = "{\"error\":\"Request body required\"}";
        return create_http_response(HTTP_BAD_REQUEST, error_json, "application/json");
    }

    json_value_t* body = json_parse(request->body);
    if (!body || body->type != JSON_OBJECT) {
        LOG_WARNING("Invalid JSON in logging configuration update request");
        /* CHECKPOINT: json_free(body); */
        char* error_json = "{\"error\":\"Invalid JSON body\"}";
        return create_http_response(HTTP_BAD_REQUEST, error_json, "application/json");
    }

    /* Update log level if provided */
    json_value_t* level_value = json_object_get(body, "level");
    if (level_value && level_value->type == JSON_STRING) {
        const char* level_str = level_value->value.string;
        log_level_t new_level;
        
        if (strcasecmp(level_str, "ERROR") == 0) {
            new_level = LOG_LEVEL_ERROR;
        } else if (strcasecmp(level_str, "WARNING") == 0) {
            new_level = LOG_LEVEL_WARNING;
        } else if (strcasecmp(level_str, "INFO") == 0) {
            new_level = LOG_LEVEL_INFO;
        } else if (strcasecmp(level_str, "DEBUG") == 0) {
            new_level = LOG_LEVEL_DEBUG;
        } else if (strcasecmp(level_str, "TRACE") == 0) {
            new_level = LOG_LEVEL_TRACE;
        } else {
            LOG_WARNING("Invalid log level requested: %s", level_str);
            /* CHECKPOINT: json_free(body); */
            char* error_json = "{\"error\":\"Invalid log level\"}";
            return create_http_response(HTTP_BAD_REQUEST, error_json, "application/json");
        }
        
        logger_set_level(new_level);
        LOG_INFO("Log level changed to %s", level_str);
    }
    
    /* Update trace categories if provided */
    json_value_t* categories = json_object_get(body, "trace_categories");
    if (categories && categories->type == JSON_ARRAY) {
        uint32_t new_mask = TRACE_NONE;
        size_t array_size = json_array_size(categories);
        
        for (size_t i = 0; i < array_size; i++) {
            json_value_t* cat = json_array_get(categories, i);
            if (cat && cat->type == JSON_STRING) {
                const char* cat_str = cat->value.string;
                
                if (strcasecmp(cat_str, "database") == 0) {
                    new_mask |= TRACE_DATABASE;
                } else if (strcasecmp(cat_str, "rbac") == 0) {
                    new_mask |= TRACE_RBAC;
                } else if (strcasecmp(cat_str, "api") == 0) {
                    new_mask |= TRACE_API;
                } else if (strcasecmp(cat_str, "auth") == 0) {
                    new_mask |= TRACE_AUTH;
                } else if (strcasecmp(cat_str, "transaction") == 0) {
                    new_mask |= TRACE_TRANSACTION;
                } else if (strcasecmp(cat_str, "binary") == 0) {
                    new_mask |= TRACE_BINARY;
                } else if (strcasecmp(cat_str, "javascript") == 0) {
                    new_mask |= TRACE_JAVASCRIPT;
                } else if (strcasecmp(cat_str, "network") == 0) {
                    new_mask |= TRACE_NETWORK;
                } else if (strcasecmp(cat_str, "metrics") == 0) {
                    new_mask |= TRACE_METRICS;
                } else if (strcasecmp(cat_str, "memory") == 0) {
                    new_mask |= TRACE_MEMORY;
                } else {
                    LOG_WARNING("Unknown trace category: %s", cat_str);
                }
            }
        }
        
        logger_set_trace_mask(new_mask);
        LOG_INFO("Trace categories updated");
    }
    
    /* CHECKPOINT: json_free(body); */
    
    /* Return current configuration */
    return handle_get_logging_config(ctx, request);
}

/* Register logging API endpoints */
void register_logging_api_endpoints(api_context_t* ctx) {
    if (!ctx) {
        LOG_ERROR("Cannot register logging endpoints with NULL context");
        return;
    }

    LOG_DEBUG("Registering logging API endpoints");

    /* Register routes using the same pattern as health API */
    ctx->routes[ctx->num_routes++] = (api_route_t){"/api/system/logging", HTTP_GET, handle_get_logging_config, 1};
    ctx->routes[ctx->num_routes++] = (api_route_t){"/api/system/logging", HTTP_PUT, handle_update_logging_config, 1};

    LOG_INFO("Logging API endpoints registered - added 2 routes");
}