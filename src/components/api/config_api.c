/**
 * @file config_api.c
 * @brief Runtime configuration management API endpoints for JDBX
 * 
 * Provides REST API endpoints for comprehensive configuration management including:
 * - Server configuration retrieval and updates
 * - Runtime configuration reloading and application
 * - Default configuration values and templates
 * - Logging configuration and trace category management
 * 
 * Architecture: Implements three-tier configuration priority system:
 * - Environment variables (highest priority)
 * - CLI flags (medium priority)  
 * - Database configuration (lowest priority - managed by these endpoints)
 * 
 * Security Features:
 * - RBAC-based authentication and authorization
 * - Admin privileges required for configuration modifications
 * - JWT token validation for all operations
 * - Comprehensive audit logging for configuration changes
 * 
 * Configuration Management:
 * - Real-time configuration updates without server restart
 * - Database-backed persistence for configuration changes
 * - Automatic validation and sanity checking
 * - Rollback capabilities for invalid configurations
 * 
 * @note Configuration changes are persisted to database and applied immediately
 * @performance Configuration operations are O(1) with database persistence overhead
 * @threadsafe Thread-safe configuration access with proper locking
 * @memory Uses checkpoint-based allocation for request processing
 */

#include "api/api.h"
#include "rbac/rbac.h"
#include "rbac/jwt.h"
#include "utils/json.h"
#include "utils/logger.h"
#include "utils/config_loader.h"
#include "utils/config_defaults.h"
#include "utils/buffer_pool.h"
#include <string.h>
#include <time.h>

/**
 * Extract username from JWT token in HTTP request
 * 
 * Parses the JWT token from the request headers or cookies and extracts
 * the username claim for RBAC authorization. Used by all configuration
 * endpoints to identify the requesting user.
 * 
 * @param request HTTP request containing JWT token (may be NULL)
 * @return Username string (caller must free) or NULL if extraction fails
 * 
 * @note Caller responsible for freeing returned username string
 * @performance O(1) token parsing with JWT decode overhead
 * @threadsafe Safe for concurrent token extraction
 * @memory Returns allocated string - caller must BUFFER_FREE()
 */
static char* extract_username_from_request(http_request_t* request) {
    if (!request) return NULL;
    
    char* token = api_extract_token(request);
    if (!token) return NULL;
    
    jwt_token_t* decoded = jwt_decode(token);
    BUFFER_FREE(token);
    
    if (!decoded || !decoded->payload || !decoded->payload->claims) {
        if (decoded) jwt_free(decoded);
        return NULL;
    }
    
    json_value_t* username_val = json_object_get(decoded->payload->claims, "username");
    char* username = NULL;
    if (username_val && username_val->type == JSON_STRING) {
        username = BUFFER_STRDUP(username_val->value.string);
    }
    
    jwt_free(decoded);
    return username;
}

/**
 * Handle configuration retrieval API request
 * 
 * Returns the current server configuration including all settings that can
 * be modified at runtime. Provides comprehensive view of active configuration
 * with metadata about configuration source and version.
 * 
 * @param ctx API context containing database and RBAC system references
 * @param request HTTP request with JWT authentication (no body required)
 * @return JSON response with current configuration or error message
 * 
 * @note Requires RBAC read permission on database resources
 * @performance O(1) configuration serialization with database access overhead
 * @threadsafe Safe for concurrent configuration access
 * @memory Uses checkpoint-based allocation for response generation
 * 
 * @example
 * GET /api/config
 * Headers: Authorization: Bearer <jwt_token>
 * Response: {
 *   "status": "success",
 *   "version": "3.1.0", 
 *   "priority": "database",
 *   "configuration": {...}
 * }
 */
http_response_t* api_handle_config_get(api_context_t* ctx, http_request_t* request) {
    /* Extract username for permissions check */
    char* username = extract_username_from_request(request);
    if (!username) {
        return create_http_response(HTTP_UNAUTHORIZED,
            "{\"error\":\"Authentication required\"}", "application/json");
    }
    
    /* Check permissions - user needs read access to system resources */
    int has_permission = rbac_check_permission(ctx->rbac, username,
                                             RBAC_DATABASE, "*", RBAC_READ);
    if (!has_permission) {
        BUFFER_FREE(username);
        return create_http_response(HTTP_FORBIDDEN, 
            "{\"error\":\"Access denied\"}", "application/json");
    }
    
    /* Get server config from global */
    extern server_config_t* g_server_config;
    if (!g_server_config) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
            "{\"error\":\"Server configuration not available\"}", "application/json");
    }
    
    /* Get current configuration */
    json_value_t* config = config_to_json(g_server_config);
    if (!config) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
            "{\"error\":\"Failed to retrieve configuration\"}", "application/json");
    }
    
    /* Add metadata */
    json_value_t* result = json_create_object();
    json_object_set(result, "status", json_create_string("success"));
    json_object_set(result, "configuration", config);
    json_object_set(result, "version", json_create_string("3.1.0"));
    json_object_set(result, "priority", json_create_string("database"));
    
    char* json_str = json_stringify(result);
    /* CHECKPOINT: json_free(result); */
    
    http_response_t* response = create_http_response(HTTP_OK, json_str, "application/json");
    BUFFER_FREE(json_str);
    
    LOG_INFO("Configuration retrieved by user: %s", username);
    BUFFER_FREE(username);
    return response;
}

/**
 * Handle configuration update API request
 * 
 * Processes configuration change requests with validation, persistence,
 * and immediate application. Supports partial configuration updates and
 * provides detailed feedback on update success and validation results.
 * 
 * @param ctx API context containing database and RBAC system references
 * @param request HTTP request with JSON body containing configuration settings
 * @return JSON response confirming update status and any validation errors
 * 
 * @note Requires RBAC admin permission - destructive operation
 * @performance O(1) configuration update with database persistence overhead
 * @threadsafe Safe configuration updates with proper transaction handling
 * @memory Request body parsed using checkpoint memory management
 * 
 * @example
 * PUT /api/config
 * Headers: Authorization: Bearer <admin_jwt_token>
 * Body: {"settings": {"server": {"max_connections": 1000}}}
 * Response: {
 *   "status": "success",
 *   "message": "Configuration updated successfully",
 *   "applied": true
 * }
 */
http_response_t* api_handle_config_update(api_context_t* ctx, http_request_t* request) {
    /* Extract username for permissions check */
    char* username = extract_username_from_request(request);
    if (!username) {
        return create_http_response(HTTP_UNAUTHORIZED,
            "{\"error\":\"Authentication required\"}", "application/json");
    }
    
    /* Check admin permissions */
    int has_admin_permission = rbac_check_permission(ctx->rbac, username,
                                                   RBAC_DATABASE, "*", RBAC_ADMIN);
    if (!has_admin_permission) {
        BUFFER_FREE(username);
        return create_http_response(HTTP_FORBIDDEN,
            "{\"error\":\"Admin access required\"}", "application/json");
    }
    
    /* Parse request body */
    json_value_t* new_config = json_parse(request->body);
    if (!new_config || new_config->type != JSON_OBJECT) {
        BUFFER_FREE(username);
        return create_http_response(HTTP_BAD_REQUEST,
            "{\"error\":\"Invalid JSON in request body\"}", "application/json");
    }
    
    /* Validate configuration */
    json_value_t* settings = json_object_get(new_config, "settings");
    if (!settings || settings->type != JSON_OBJECT) {
        /* CHECKPOINT: json_free(new_config); */
        BUFFER_FREE(username);
        return create_http_response(HTTP_BAD_REQUEST,
            "{\"error\":\"Missing or invalid settings object\"}", "application/json");
    }
    
    /* Save to database */
    int save_result = config_save_to_database(ctx->db, settings);
    if (save_result != 0) {
        /* CHECKPOINT: json_free(new_config); */
        BUFFER_FREE(username);
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
            "{\"error\":\"Failed to save configuration\"}", "application/json");
    }
    
    /* Apply changes */
    extern server_config_t* g_server_config;
    int apply_result = config_apply_database_settings(g_server_config, ctx->db);
    if (apply_result != 0) {
        LOG_WARNING("Failed to apply some configuration changes.");
    }
    
    /* CHECKPOINT: json_free(new_config); */
    
    /* Return success response */
    json_value_t* result = json_create_object();
    json_object_set(result, "status", json_create_string("success"));
    json_object_set(result, "message", json_create_string("Configuration updated successfully"));
    json_object_set(result, "applied", json_create_boolean(apply_result == 0));
    
    char* json_str = json_stringify(result);
    /* CHECKPOINT: json_free(result); */
    
    http_response_t* response = create_http_response(HTTP_OK, json_str, "application/json");
    BUFFER_FREE(json_str);
    
    LOG_INFO("Configuration updated by admin user: %s", username);
    BUFFER_FREE(username);
    return response;
}

/* POST /api/config/reload - Reload configuration from database */
http_response_t* api_handle_config_reload(api_context_t* ctx, http_request_t* request) {
    /* Extract username for permissions check */
    char* username = extract_username_from_request(request);
    if (!username) {
        return create_http_response(HTTP_UNAUTHORIZED,
            "{\"error\":\"Authentication required\"}", "application/json");
    }
    
    /* Check admin permissions */
    int has_admin_permission = rbac_check_permission(ctx->rbac, username,
                                                   RBAC_DATABASE, "*", RBAC_ADMIN);
    if (!has_admin_permission) {
        BUFFER_FREE(username);
        return create_http_response(HTTP_FORBIDDEN,
            "{\"error\":\"Admin access required\"}", "application/json");
    }
    
    /* Reload configuration from database */
    extern server_config_t* g_server_config;
    int reload_result = config_apply_database_settings(g_server_config, ctx->db);
    
    /* Build response */
    json_value_t* result = json_create_object();
    json_object_set(result, "status", json_create_string(reload_result == 0 ? "success" : "partial"));
    json_object_set(result, "message", json_create_string(
        reload_result == 0 ? "Configuration reloaded successfully" : "Some settings require restart"));
    
    /* Get current config */
    json_value_t* current_config = config_to_json(g_server_config);
    if (current_config) {
        json_object_set(result, "configuration", current_config);
    }
    
    char* json_str = json_stringify(result);
    /* CHECKPOINT: json_free(result); */
    
    http_response_t* response = create_http_response(HTTP_OK, json_str, "application/json");
    BUFFER_FREE(json_str);
    
    LOG_INFO("Configuration reloaded by admin user: %s", username);
    BUFFER_FREE(username);
    return response;
}

/* GET /api/config/defaults - Get default configuration */
http_response_t* api_handle_config_defaults(api_context_t* ctx, http_request_t* request) {
    /* Extract username for permissions check */
    char* username = extract_username_from_request(request);
    if (!username) {
        return create_http_response(HTTP_UNAUTHORIZED,
            "{\"error\":\"Authentication required\"}", "application/json");
    }
    
    /* Check permissions - user needs read access to database resources */
    int has_permission = rbac_check_permission(ctx->rbac, username,
                                             RBAC_DATABASE, "*", RBAC_READ);
    if (!has_permission) {
        BUFFER_FREE(username);
        return create_http_response(HTTP_FORBIDDEN,
            "{\"error\":\"Access denied\"}", "application/json");
    }
    
    BUFFER_FREE(username); /* Don't need username after permission check */
    
    /* Build defaults object */
    json_value_t* defaults = json_create_object();
    
    /* Server defaults */
    json_value_t* server = json_create_object();
    json_object_set(server, "host", json_create_string(DEFAULT_HOST));
    json_object_set(server, "port", json_create_integer(DEFAULT_PORT));
    json_object_set(server, "daemon", json_create_boolean(1));
    
    json_value_t* ssl = json_create_object();
    json_object_set(ssl, "enabled", json_create_boolean(DEFAULT_SSL_ENABLED));
    json_object_set(ssl, "cert_path", json_create_string(DEFAULT_SSL_CERT_PATH));
    json_object_set(ssl, "key_path", json_create_string(DEFAULT_SSL_KEY_PATH));
    json_object_set(server, "ssl", ssl);
    
    json_object_set(defaults, "server", server);
    
    /* Thread pool defaults */
    json_value_t* thread_pool = json_create_object();
    json_object_set(thread_pool, "min_threads", json_create_integer(DEFAULT_THREAD_POOL_MIN));
    json_object_set(thread_pool, "max_threads", json_create_integer(DEFAULT_THREAD_POOL_MAX));
    json_object_set(thread_pool, "queue_size", json_create_integer(DEFAULT_THREAD_POOL_QUEUE_SIZE));
    json_object_set(thread_pool, "idle_timeout", json_create_integer(DEFAULT_THREAD_POOL_IDLE_TIMEOUT));
    json_object_set(defaults, "thread_pool", thread_pool);
    
    /* Cache defaults */
    json_value_t* cache = json_create_object();
    json_object_set(cache, "enabled", json_create_boolean(DEFAULT_CACHE_ENABLED));
    json_object_set(cache, "max_size", json_create_integer(DEFAULT_CACHE_SIZE));
    json_object_set(cache, "ttl", json_create_integer(DEFAULT_CACHE_TTL));
    json_object_set(defaults, "cache", cache);
    
    /* Metrics defaults */
    json_value_t* metrics = json_create_object();
    json_object_set(metrics, "enabled", json_create_boolean(DEFAULT_METRICS_ENABLED));
    json_object_set(metrics, "retention", json_create_integer(DEFAULT_METRICS_RETENTION));
    json_object_set(defaults, "metrics", metrics);
    
    /* Indexing defaults */
    json_value_t* indexing = json_create_object();
    json_object_set(indexing, "query_threshold", json_create_integer(DEFAULT_INDEX_QUERY_THRESHOLD));
    json_object_set(indexing, "time_threshold", json_create_integer(DEFAULT_INDEX_TIME_THRESHOLD));
    json_object_set(indexing, "system_query_threshold", json_create_integer(DEFAULT_INDEX_QUERY_THRESHOLD_SYSTEM));
    json_object_set(indexing, "system_time_threshold", json_create_integer(DEFAULT_INDEX_TIME_THRESHOLD_SYSTEM));
    json_object_set(indexing, "startup_delay", json_create_integer(DEFAULT_INDEX_STARTUP_DELAY));
    json_object_set(indexing, "check_interval", json_create_integer(DEFAULT_INDEX_CHECK_INTERVAL));
    json_object_set(defaults, "indexing", indexing);
    
    /* Build response */
    json_value_t* result = json_create_object();
    json_object_set(result, "status", json_create_string("success"));
    json_object_set(result, "defaults", defaults);
    json_object_set(result, "description", json_create_string("Default configuration values"));
    
    char* json_str = json_stringify(result);
    /* CHECKPOINT: json_free(result); */
    
    http_response_t* response = create_http_response(HTTP_OK, json_str, "application/json");
    BUFFER_FREE(json_str);
    return response;
}

/* GET /api/config/logging - Get current logging configuration */
http_response_t* api_handle_logging_get(api_context_t* ctx, http_request_t* request) {
    /* Extract username for permissions check */
    char* username = extract_username_from_request(request);
    if (!username) {
        return create_http_response(HTTP_UNAUTHORIZED,
            "{\"error\":\"Authentication required\"}", "application/json");
    }
    
    /* Check permissions - user needs read access to system resources */
    int has_permission = rbac_check_permission(ctx->rbac, username,
                                             RBAC_DATABASE, "*", RBAC_READ);
    if (!has_permission) {
        BUFFER_FREE(username);
        return create_http_response(HTTP_FORBIDDEN, 
            "{\"error\":\"Access denied\"}", "application/json");
    }
    
    BUFFER_FREE(username);
    
    /* Get current logging configuration */
    json_value_t* result = json_create_object();
    json_object_set(result, "status", json_create_string("success"));
    
    json_value_t* logging = json_create_object();
    json_object_set(logging, "log_level", json_create_integer(logger_get_level()));
    json_object_set(logging, "log_level_name", json_create_string(logger_level_string(logger_get_level())));
    json_object_set(logging, "trace_mask", json_create_integer(logger_get_trace_mask()));
    
    /* Add trace category breakdown */
    json_value_t* trace_categories = json_create_object();
    trace_category_t mask = logger_get_trace_mask();
    json_object_set(trace_categories, "database", json_create_boolean(mask & TRACE_DATABASE));
    json_object_set(trace_categories, "rbac", json_create_boolean(mask & TRACE_RBAC));
    json_object_set(trace_categories, "api", json_create_boolean(mask & TRACE_API));
    json_object_set(trace_categories, "auth", json_create_boolean(mask & TRACE_AUTH));
    json_object_set(trace_categories, "transaction", json_create_boolean(mask & TRACE_TRANSACTION));
    json_object_set(trace_categories, "binary", json_create_boolean(mask & TRACE_BINARY));
    json_object_set(trace_categories, "javascript", json_create_boolean(mask & TRACE_JAVASCRIPT));
    json_object_set(trace_categories, "network", json_create_boolean(mask & TRACE_NETWORK));
    json_object_set(trace_categories, "metrics", json_create_boolean(mask & TRACE_METRICS));
    json_object_set(trace_categories, "memory", json_create_boolean(mask & TRACE_MEMORY));
    json_object_set(logging, "trace_categories", trace_categories);
    
    json_object_set(result, "logging", logging);
    
    char* json_str = json_stringify(result);
    /* CHECKPOINT: json_free(result); */
    
    http_response_t* response = create_http_response(HTTP_OK, json_str, "application/json");
    BUFFER_FREE(json_str);
    return response;
}

/* PUT /api/config/logging - Update logging configuration */
http_response_t* api_handle_logging_update(api_context_t* ctx, http_request_t* request) {
    /* Extract username for permissions check */
    char* username = extract_username_from_request(request);
    if (!username) {
        return create_http_response(HTTP_UNAUTHORIZED,
            "{\"error\":\"Authentication required\"}", "application/json");
    }
    
    /* Check admin permissions */
    int has_admin_permission = rbac_check_permission(ctx->rbac, username,
                                                   RBAC_DATABASE, "*", RBAC_ADMIN);
    if (!has_admin_permission) {
        BUFFER_FREE(username);
        return create_http_response(HTTP_FORBIDDEN,
            "{\"error\":\"Admin access required\"}", "application/json");
    }
    
    /* Parse request body */
    json_value_t* new_config = json_parse(request->body);
    if (!new_config || new_config->type != JSON_OBJECT) {
        BUFFER_FREE(username);
        return create_http_response(HTTP_BAD_REQUEST,
            "{\"error\":\"Invalid JSON in request body\"}", "application/json");
    }
    
    int changes_made = 0;
    
    /* Update log level if provided */
    json_value_t* log_level_val = json_object_get(new_config, "log_level");
    if (log_level_val) {
        if (log_level_val->type == JSON_INTEGER) {
            log_level_t new_level = (log_level_t)log_level_val->value.integer;
            if (new_level >= LOG_LEVEL_NONE && new_level <= LOG_LEVEL_TRACE) {
                logger_set_level(new_level);
                changes_made = 1;
                LOG_INFO("Log level changed to %s by admin user: %s", 
                         logger_level_string(new_level), username);
            }
        } else if (log_level_val->type == JSON_STRING) {
            log_level_t new_level = logger_parse_level(log_level_val->value.string);
            if (new_level != LOG_LEVEL_NONE || strcmp(log_level_val->value.string, "none") == 0) {
                logger_set_level(new_level);
                changes_made = 1;
                LOG_INFO("Log level changed to %s by admin user: %s", 
                         logger_level_string(new_level), username);
            }
        }
    }
    
    /* Update trace categories if provided */
    json_value_t* trace_categories = json_object_get(new_config, "trace_categories");
    if (trace_categories && trace_categories->type == JSON_STRING) {
        trace_category_t new_mask = logger_parse_trace(trace_categories->value.string);
        logger_set_trace_mask(new_mask);
        changes_made = 1;
        LOG_INFO("Trace categories updated by admin user: %s", username);
    }
    
    /* Handle individual trace category updates */
    json_value_t* trace_mask_val = json_object_get(new_config, "trace_mask");
    if (trace_mask_val && trace_mask_val->type == JSON_INTEGER) {
        trace_category_t new_mask = (trace_category_t)trace_mask_val->value.integer;
        logger_set_trace_mask(new_mask);
        changes_made = 1;
        LOG_INFO("Trace mask updated to %d by admin user: %s", new_mask, username);
    }
    
    /* CHECKPOINT: json_free(new_config); */
    BUFFER_FREE(username);
    
    if (!changes_made) {
        return create_http_response(HTTP_BAD_REQUEST,
            "{\"error\":\"No valid logging configuration provided\"}", "application/json");
    }
    
    /* Return success response with current configuration */
    json_value_t* result = json_create_object();
    json_object_set(result, "status", json_create_string("success"));
    json_object_set(result, "message", json_create_string("Logging configuration updated successfully"));
    
    json_value_t* current_logging = json_create_object();
    json_object_set(current_logging, "log_level", json_create_integer(logger_get_level()));
    json_object_set(current_logging, "log_level_name", json_create_string(logger_level_string(logger_get_level())));
    json_object_set(current_logging, "trace_mask", json_create_integer(logger_get_trace_mask()));
    json_object_set(result, "logging", current_logging);
    
    char* json_str = json_stringify(result);
    /* CHECKPOINT: json_free(result); */
    
    http_response_t* response = create_http_response(HTTP_OK, json_str, "application/json");
    BUFFER_FREE(json_str);
    return response;
}

/**
 * Register configuration API routes with API context
 * 
 * Registers all configuration management endpoints including configuration
 * retrieval, updates, reloading, defaults, and logging management. Sets up
 * proper authentication requirements for each endpoint.
 * 
 * @param ctx API context to register routes with (must not be NULL)
 * 
 * @note All configuration endpoints require authentication
 * @performance O(1) route registration
 * @threadsafe Safe for concurrent route registration during startup
 * @memory Routes stored in API context array - no additional allocation
 * 
 * Registered Routes:
 * - GET /api/config - Retrieve current configuration
 * - PUT /api/config - Update configuration  
 * - POST /api/config/reload - Reload configuration from database
 * - GET /api/config/defaults - Get default configuration values
 * - GET /api/config/logging - Get logging configuration
 * - PUT /api/config/logging - Update logging configuration
 */
void register_config_api_routes(api_context_t* ctx) {
    if (!ctx) {
        LOG_ERROR("Cannot register config API routes - NULL context.");
        return;
    }
    
    /* Check if we have enough space for our routes */
    if (ctx->num_routes + 6 > ctx->max_routes) {
        LOG_ERROR("Cannot register config API routes - not enough space.");
        LOG_ERROR("Current routes: %d, Max routes: %d, Need to add: 6", 
             ctx->num_routes, ctx->max_routes);
        return;
    }
    
    /* Register configuration endpoints */
    ctx->routes[ctx->num_routes++] = (api_route_t){
        "/api/config", HTTP_GET, api_handle_config_get, 1
    };
    ctx->routes[ctx->num_routes++] = (api_route_t){
        "/api/config", HTTP_PUT, api_handle_config_update, 1
    };
    ctx->routes[ctx->num_routes++] = (api_route_t){
        "/api/config/reload", HTTP_POST, api_handle_config_reload, 1
    };
    ctx->routes[ctx->num_routes++] = (api_route_t){
        "/api/config/defaults", HTTP_GET, api_handle_config_defaults, 1
    };
    ctx->routes[ctx->num_routes++] = (api_route_t){
        "/api/config/logging", HTTP_GET, api_handle_logging_get, 1
    };
    ctx->routes[ctx->num_routes++] = (api_route_t){
        "/api/config/logging", HTTP_PUT, api_handle_logging_update, 1
    };
    
    LOG_INFO("Configuration API routes registered - added 6 routes.");
}