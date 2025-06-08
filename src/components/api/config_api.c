/*
 * config_api.c - Configuration management API endpoints
 *
 * Provides REST API for runtime configuration management
 */

#include "api/api.h"
#include "rbac/rbac.h"
#include "rbac/jwt.h"
#include "utils/json.h"
#include "utils/logger.h"
#include "utils/config_loader.h"
#include "utils/config_defaults.h"
#include <string.h>
#include <time.h>

/* Helper function to extract username from JWT token in request */
static char* extract_username_from_request(http_request_t* request) {
    if (!request) return NULL;
    
    char* token = api_extract_token(request);
    if (!token) return NULL;
    
    jwt_token_t* decoded = jwt_decode(token);
    free(token);
    
    if (!decoded || !decoded->payload || !decoded->payload->claims) {
        if (decoded) jwt_free(decoded);
        return NULL;
    }
    
    json_value_t* username_val = json_object_get(decoded->payload->claims, "username");
    char* username = NULL;
    if (username_val && username_val->type == JSON_STRING) {
        username = strdup(username_val->value.string);
    }
    
    jwt_free(decoded);
    return username;
}

/* GET /api/config - Retrieve current configuration */
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
        free(username);
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
    json_free(result);
    
    http_response_t* response = create_http_response(HTTP_OK, json_str, "application/json");
    free(json_str);
    
    LOG_INFO("Configuration retrieved by user: %s", username);
    free(username);
    return response;
}

/* PUT /api/config - Update configuration */
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
        free(username);
        return create_http_response(HTTP_FORBIDDEN,
            "{\"error\":\"Admin access required\"}", "application/json");
    }
    
    /* Parse request body */
    json_value_t* new_config = json_parse(request->body);
    if (!new_config || new_config->type != JSON_OBJECT) {
        free(username);
        return create_http_response(HTTP_BAD_REQUEST,
            "{\"error\":\"Invalid JSON in request body\"}", "application/json");
    }
    
    /* Validate configuration */
    json_value_t* settings = json_object_get(new_config, "settings");
    if (!settings || settings->type != JSON_OBJECT) {
        json_free(new_config);
        free(username);
        return create_http_response(HTTP_BAD_REQUEST,
            "{\"error\":\"Missing or invalid settings object\"}", "application/json");
    }
    
    /* Save to database */
    int save_result = config_save_to_database(ctx->db, settings);
    if (save_result != 0) {
        json_free(new_config);
        free(username);
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
            "{\"error\":\"Failed to save configuration\"}", "application/json");
    }
    
    /* Apply changes */
    extern server_config_t* g_server_config;
    int apply_result = config_apply_database_settings(g_server_config, ctx->db);
    if (apply_result != 0) {
        LOG_WARNING("Failed to apply some configuration changes.");
    }
    
    json_free(new_config);
    
    /* Return success response */
    json_value_t* result = json_create_object();
    json_object_set(result, "status", json_create_string("success"));
    json_object_set(result, "message", json_create_string("Configuration updated successfully"));
    json_object_set(result, "applied", json_create_boolean(apply_result == 0));
    
    char* json_str = json_stringify(result);
    json_free(result);
    
    http_response_t* response = create_http_response(HTTP_OK, json_str, "application/json");
    free(json_str);
    
    LOG_INFO("Configuration updated by admin user: %s", username);
    free(username);
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
        free(username);
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
    json_free(result);
    
    http_response_t* response = create_http_response(HTTP_OK, json_str, "application/json");
    free(json_str);
    
    LOG_INFO("Configuration reloaded by admin user: %s", username);
    free(username);
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
        free(username);
        return create_http_response(HTTP_FORBIDDEN,
            "{\"error\":\"Access denied\"}", "application/json");
    }
    
    free(username); /* Don't need username after permission check */
    
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
    json_free(result);
    
    http_response_t* response = create_http_response(HTTP_OK, json_str, "application/json");
    free(json_str);
    return response;
}

/* Register configuration API routes */
void register_config_api_routes(api_context_t* ctx) {
    if (!ctx) {
        LOG_ERROR("Cannot register config API routes - NULL context.");
        return;
    }
    
    /* Check if we have enough space for our routes */
    if (ctx->num_routes + 4 > ctx->max_routes) {
        LOG_ERROR("Cannot register config API routes - not enough space.");
        LOG_ERROR("Current routes: %d, Max routes: %d, Need to add: 4", 
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
    
    LOG_INFO("Configuration API routes registered - added 4 routes.");
}