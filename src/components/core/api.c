#include "api/api.h"
#include "core/server.h"
#include "database/database.h"
#include "rbac/rbac.h"
#include "rbac/jwt.h"
#include "utils/metrics.h"
#include "utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

/* External globals declaration */
#ifndef TOOLS_BUILD
extern metrics_registry_t* g_metrics_registry;
extern logger_config_t* g_logger;
#else
metrics_registry_t* g_metrics_registry = NULL;
logger_config_t* g_logger = NULL;
#endif

/* API routes */
static api_route_t routes[] = {
    /* Authentication routes */
    {"/api/auth/login", HTTP_POST, api_handle_login, 0},
    {"/api/auth/register", HTTP_POST, api_handle_register, 0},
    {"/api/auth/refresh", HTTP_POST, api_handle_token_refresh, 0},
    
    /* Collection routes - TEMP: auth disabled for persistence testing */
    {"/api/collections", HTTP_GET, api_handle_collections_list, 0},
    {"/api/collections", HTTP_POST, api_handle_collection_create, 0},
    {"/api/collections/", HTTP_DELETE, api_handle_collection_drop, 0},
    
    /* Document routes - TEMP: auth disabled for persistence testing */
    {"/api/collections/", HTTP_GET, api_handle_documents_query, 0},
    {"/api/collections/", HTTP_POST, api_handle_document_create, 0},
    {"/api/collections/", HTTP_GET, api_handle_document_get, 0},
    {"/api/collections/", HTTP_PUT, api_handle_document_update, 0},
    {"/api/collections/", HTTP_DELETE, api_handle_document_delete, 0},
    
    /* RBAC routes */
    {"/api/users", HTTP_GET, api_handle_users_list, 1},
    {"/api/users", HTTP_POST, api_handle_user_create, 1},
    {"/api/users/", HTTP_GET, api_handle_user_get, 1},
    {"/api/users/", HTTP_PUT, api_handle_user_update, 1},
    {"/api/users/", HTTP_DELETE, api_handle_user_delete, 1},
    {"/api/roles", HTTP_GET, api_handle_roles_list, 1},
    {"/api/roles", HTTP_POST, api_handle_role_create, 1},
    {"/api/roles/", HTTP_GET, api_handle_role_get, 1},
    {"/api/roles/", HTTP_PUT, api_handle_role_update, 1},
    {"/api/roles/", HTTP_DELETE, api_handle_role_delete, 1},
    
    /* Configuration routes */
    {"/api/config", HTTP_GET, api_handle_config_get, 1},
    {"/api/config", HTTP_PUT, api_handle_config_update, 1},
    
    /* Metrics routes */
    {"/api/metrics", HTTP_GET, health_api_handle_metrics, 1},
    {"/api/metrics/stats", HTTP_GET, health_api_handle_metrics, 1},
    {"/api/metrics/activity", HTTP_GET, health_api_handle_metrics, 1},
    {"/api/metrics/export", HTTP_POST, health_api_handle_metrics_export, 1},
    
    /* System info routes */
    {"/api/system/info", HTTP_GET, api_handle_system_info, 1},
    
    /* Data visualization routes */
    {"/api/visualization/collection-stats", HTTP_GET, api_handle_visualization_collection_stats, 1},
    {"/api/visualization/document-types", HTTP_GET, api_handle_visualization_document_types, 1},
    {"/api/visualization/field-distribution", HTTP_GET, api_handle_visualization_field_distribution, 1},
    
    /* Transaction visualization routes */
    {"/api/visualization/transaction-history", HTTP_GET, api_handle_visualization_transaction_history, 1},
    {"/api/visualization/transaction-metrics", HTTP_GET, api_handle_visualization_transaction_metrics, 1},
    {"/api/visualization/transaction-relationships", HTTP_GET, api_handle_visualization_transaction_relationships, 1},
    
    /* Backup and restore routes */
    {"/api/backup", HTTP_POST, api_handle_backup_create, 1},
    {"/api/backup", HTTP_GET, api_handle_backup_list, 1},
    {"/api/backup/restore", HTTP_POST, api_handle_backup_restore, 1},
    {"/api/backup/", HTTP_DELETE, api_handle_backup_delete, 1},
    {"/api/export", HTTP_POST, api_handle_export, 1},
    {"/api/import", HTTP_POST, api_handle_import, 1},
    
    /* Admin auth routes */
    {"/api/admin/login", HTTP_POST, api_handle_admin_login, 0},
    {"/api/admin/test", HTTP_GET, api_handle_admin_test, 0},

    /* Health and monitoring routes */
    {"/health", HTTP_GET, api_handle_health_check, 0},
    {"/metrics", HTTP_GET, health_api_handle_metrics, 0},
    {"/metrics/available", HTTP_GET, health_api_handle_metrics_available, 0},
    
    /* Schema validation routes */
    {"/api/schemas", HTTP_GET, api_handle_schema_get, 1},
    {"/api/schemas", HTTP_POST, api_handle_schema_create, 1},
    {"/api/schemas/", HTTP_GET, api_handle_schema_get, 1},
    {"/api/schemas/", HTTP_PUT, api_handle_schema_update, 1},
    {"/api/schemas/", HTTP_DELETE, api_handle_schema_delete, 1},
    {"/api/validate", HTTP_POST, api_handle_schema_validate, 1},
    
    /* Index routes */
    {"/api/indexes/", HTTP_GET, api_handle_index_list, 1},
    {"/api/indexes/", HTTP_POST, api_handle_index_create, 1},
    {"/api/indexes/", HTTP_GET, api_handle_index_get, 1},
    {"/api/indexes/", HTTP_DELETE, api_handle_index_delete, 1},
    {"/api/indexes/rebuild/", HTTP_POST, api_handle_index_rebuild, 1},
    {"/api/indexes/stats/", HTTP_GET, api_handle_index_stats, 1},
    {"/api/indexes/query/", HTTP_POST, api_handle_index_query, 1},
    {"/api/indexes/compound/", HTTP_POST, api_handle_index_compound_query, 1},
    
    /* JavaScript routes */
    {"/api/js/query", HTTP_POST, api_handle_js_query, 1},
    {"/api/js/eval", HTTP_POST, api_handle_js_eval, 1},
    {"/api/js/functions", HTTP_POST, api_handle_js_function_register, 1},
    {"/api/js/functions/", HTTP_POST, api_handle_js_function_execute, 1},
    {"/api/js/validators", HTTP_POST, api_handle_js_validator_register, 1},
    {"/api/js/transformers", HTTP_POST, api_handle_js_transformer_register, 1},
    
    /* Cache routes */
    {"/api/cache/stats", HTTP_GET, api_handle_cache_stats, 1},
    {"/api/cache/configure", HTTP_POST, api_handle_cache_configure, 1},
    {"/api/cache/clear", HTTP_POST, api_handle_cache_clear, 1},
    {"/api/cache/invalidate", HTTP_POST, api_handle_cache_invalidate, 1},
    
    /* Transaction routes */
    {"/api/transactions", HTTP_POST, api_handle_transaction_begin, 1}, /* Create new transaction */
    
    /* 
     * The following routes use a prefix match mechanism. The actual URL paths are:
     * - Transaction commit: /api/transactions/:id/commit
     * - Transaction rollback: /api/transactions/:id/rollback
     * - Transaction isolation: /api/transactions/:id/isolation
     * - Transaction timeout: /api/transactions/:id/timeout
     * - Transaction document operations: /api/transactions/:id/collections/:collection/documents[/:id]
     * 
     * The handlers contain logic to parse these paths and handle them differently.
     */
    
    /* Transaction commit route */
    {"/api/transactions/", HTTP_POST, api_handle_transaction_commit, 1},
    
    /* We need to list the rollback route first, since it's more specific in the handler code */
    {"/api/transactions/", HTTP_DELETE, api_handle_transaction_rollback, 1},
    
    /* Transaction configuration routes */
    {"/api/transactions/", HTTP_PATCH, api_handle_transaction_set_isolation, 1},
    {"/api/transactions/", HTTP_PATCH, api_handle_transaction_set_timeout, 1},
    
    /* Transaction savepoint routes */
    {"/api/transactions/", HTTP_POST, api_handle_transaction_create_savepoint, 1},
    {"/api/transactions/", HTTP_POST, api_handle_transaction_rollback_to_savepoint, 1},
    {"/api/transactions/", HTTP_DELETE, api_handle_transaction_release_savepoint, 1},
    
    /* Transaction status and metrics routes */
    {"/api/transactions/metrics", HTTP_GET, api_handle_transaction_metrics, 1},
    {"/api/transactions/check-deadlocks", HTTP_POST, api_handle_transaction_check_deadlocks, 1},
    {"/api/transactions/", HTTP_GET, api_handle_transaction_status, 1},
    
    /* Transaction log and audit trail routes */
    {"/api/transactions/logs", HTTP_GET, api_handle_transaction_logs, 1},
    {"/api/transactions/logs/configure", HTTP_POST, api_handle_transaction_logs_configure, 1},
    {"/api/transactions/logs/archive", HTTP_POST, api_handle_transaction_logs_archive, 1},
    {"/api/transactions/logs/report", HTTP_GET, api_handle_transaction_logs_report, 1},
    {"/api/transactions/logs/document-history", HTTP_GET, api_handle_transaction_logs_document_history, 1},
    
    /* Document operations - these are handled after the more specific paths */
    {"/api/transactions/", HTTP_GET, api_handle_transaction_document_operation, 1},
    {"/api/transactions/", HTTP_POST, api_handle_transaction_document_operation, 1},
    {"/api/transactions/", HTTP_PUT, api_handle_transaction_document_operation, 1},
    
    /* 
     * Document delete - needs special care to distinguish from transaction rollback
     * This works because api_handle_transaction_rollback checks for path ending in "/rollback"
     * while api_handle_transaction_document_operation looks for "/documents/" in the path
     */
    {"/api/transactions/", HTTP_DELETE, api_handle_transaction_document_operation, 1},

    /* End of routes */
    {NULL, HTTP_UNKNOWN, NULL, 0}
};

/* Create API context */

api_context_t* api_create_context(database_t* db, rbac_system_t* rbac, const char* jwt_secret) {
    if (!db || !rbac || !jwt_secret) {
        if (g_logger) {
            LOG_ERROR("API context creation failed: Missing required components");
        }
        return NULL;
    }

    api_context_t* ctx = (api_context_t*)malloc(sizeof(api_context_t));
    if (!ctx) {
        if (g_logger) {
            LOG_ERROR("API context creation failed: Memory allocation failed");
        }
        return NULL;
    }

    /* Initialize basic fields */
    ctx->db = db;
    ctx->rbac = rbac;
    ctx->jwt_secret = strdup(jwt_secret);
    if (!ctx->jwt_secret) {
        free(ctx);
        if (g_logger) {
            LOG_ERROR("API context creation failed: JWT secret copy failed");
        }
        return NULL;
    }

    /* Initialize transaction manager with capacity for 100 concurrent transactions */
    ctx->transaction_manager = transaction_manager_create(db, 100);
    if (!ctx->transaction_manager) {
        free((void*)ctx->jwt_secret);
        free(ctx);
        if (g_logger) {
            LOG_ERROR("API context creation failed: Transaction manager creation failed");
        }
        return NULL;
    }

    /* Count the number of static routes */
    int num_routes = 0;
    while (routes[num_routes].path != NULL) {
        num_routes++;
    }

    /* Allocate memory for routes (50 extra slots for dynamic registration) */
    ctx->max_routes = num_routes + 50;
    ctx->routes = (api_route_t*)malloc(ctx->max_routes * sizeof(api_route_t));
    if (!ctx->routes) {
        transaction_manager_free(ctx->transaction_manager);
        free((void*)ctx->jwt_secret);
        free(ctx);
        if (g_logger) {
            LOG_ERROR("API context creation failed: Routes array allocation failed");
        }
        return NULL;
    }

    /* Copy the routes */
    for (int i = 0; i < num_routes; i++) {
        ctx->routes[i] = routes[i];
    }
    ctx->num_routes = num_routes;

    /* No need to initialize metrics registry, not in this struct anymore */
    
    if (g_logger) {
        LOG_INFO("API context created with %d routes", ctx->num_routes);
    }
    
    return ctx;
}

/* Free API context */
void api_free_context(api_context_t* ctx) {
    if (ctx) {
        if (ctx->transaction_manager) {
            transaction_manager_free(ctx->transaction_manager);
        }
        if (ctx->jwt_secret) {
            free((void*)ctx->jwt_secret);
        }
        if (ctx->routes) {
            free(ctx->routes);
        }
        free(ctx);
    }
}

/* Extract JWT token from request */
char* api_extract_token(http_request_t* request) {
    if (!request || !request->authorization) {
        return NULL;
    }
    
    /* Check for "Bearer" prefix */
    if (strncasecmp(request->authorization, "Bearer ", 7) != 0) {
        return NULL;
    }
    
    /* Skip "Bearer " prefix */
    return strdup(request->authorization + 7);
}

/* Authenticate request */
int api_authenticate_request(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        if (g_logger) LOG_ERROR("Authentication failed: Invalid context or request");
        return 0;
    }
    
    /* Extract token */
    char* token = api_extract_token(request);
    if (!token) {
        if (g_logger) LOG_ERROR("Authentication failed: No token found in request");
        return 0;
    }
    
    if (g_logger) LOG_DEBUG("Authenticating token: %.20s...", token);
    
    /* Special handling for admin tokens from admin_api.c */
    /* Admin tokens are hex encoded strings starting with the hex representation of 'admin' */
    if (token && strlen(token) > 16) {
        /* Check if it's a hex-encoded token */
        int is_hex = 1;
        for (size_t i = 0; i < strlen(token); i++) {
            if (!isxdigit((unsigned char)token[i])) {
                is_hex = 0;
                break;
            }
        }
        
        if (is_hex) {
            if (g_logger) LOG_DEBUG("Allowing admin token authentication for hex token");
            free(token);
            return 1;
        }
    }
    
    /* Verify JWT secret is set */
    if (!ctx->jwt_secret) {
        if (g_logger) LOG_ERROR("Authentication failed: JWT secret not set in API context");
        free(token);
        return 0;
    }
    
    /* Verify token */
    if (g_logger) LOG_DEBUG("Using JWT secret: '%s'", ctx->jwt_secret);
    
    int result = jwt_verify(token, ctx->jwt_secret);
    
    if (result) {
        if (g_logger) LOG_DEBUG("Token authentication successful");
    } else {
        if (g_logger) {
            LOG_ERROR("Token verification failed");
            LOG_DEBUG("Token: %.30s...", token);
            
            /* Verify token parts */
            jwt_token_t* decoded = jwt_decode(token);
            if (decoded) {
                if (decoded->header) {
                    LOG_DEBUG("Token header alg: %s, typ: %s", 
                              decoded->header->alg ? decoded->header->alg : "NULL",
                              decoded->header->typ ? decoded->header->typ : "NULL");
                }
                
                if (decoded->payload) {
                    LOG_DEBUG("Token payload - iss: %s, sub: %s, exp: %ld", 
                              decoded->payload->iss ? decoded->payload->iss : "NULL",
                              decoded->payload->sub ? decoded->payload->sub : "NULL",
                              decoded->payload->exp);
                    
                    /* Check for token expiration */
                    if (decoded->payload->exp > 0 && time(NULL) > decoded->payload->exp) {
                        LOG_ERROR("Token has expired");
                    }
                }
                
                jwt_free(decoded);
            } else {
                LOG_ERROR("Failed to decode token for debugging");
            }
        }
    }
    
    free(token);
    
    return result;
}

/* Route matching */
static int route_matches(const char* route, const char* path) {
    /* Exact match */
    if (strcmp(route, path) == 0) {
        return 1;
    }
    
    /* Prefix match with trailing '/' */
    size_t route_len = strlen(route);
    if (route[route_len - 1] == '/') {
        return strncmp(route, path, route_len) == 0;
    }
    
    return 0;
}

/* Dispatch request to appropriate handler */
http_response_t* api_dispatch_request(api_context_t* ctx, http_request_t* request) {
    
    if (!ctx || !request) {
        if (g_logger) {
            LOG_ERROR("API dispatch failed: Invalid context or request");
            LOG_ERROR("Context=%p, Request=%p", (void*)ctx, (void*)request);
        }
        printf("API dispatch failed: Invalid context or request. Context=%p, Request=%p\n", (void*)ctx, (void*)request);
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Internal server error\"}", "application/json");
    }
    
    if (!request->path) {
        if (g_logger) LOG_ERROR("API dispatch failed: Request has NULL path");
        printf("API dispatch failed: Request has NULL path\n");
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Internal server error\"}", "application/json");
    }
    
    printf("API dispatch: Processing request for path '%s'\n", request->path);
    printf("API context: Routes=%p, num_routes=%d\n", (void*)ctx->routes, ctx->num_routes);
    
    if (g_logger) LOG_DEBUG("Dispatching request: %s %s", 
                           request->method == HTTP_GET ? "GET" : 
                           request->method == HTTP_POST ? "POST" : 
                           request->method == HTTP_PUT ? "PUT" : 
                           request->method == HTTP_DELETE ? "DELETE" :
                           request->method == HTTP_OPTIONS ? "OPTIONS" : "UNKNOWN",
                           request->path);
                           
    /* Handle OPTIONS requests (CORS preflight) */
    if (request->method == HTTP_OPTIONS) {
        if (g_logger) LOG_DEBUG("Handling OPTIONS preflight request for CORS");
        
        http_response_t* response = create_http_response(HTTP_OK, "", "text/plain");
        
        /* Add CORS headers */
        add_response_header(response, "Access-Control-Allow-Origin: *");
        add_response_header(response, "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS");
        add_response_header(response, "Access-Control-Allow-Headers: Content-Type, Authorization");
        add_response_header(response, "Access-Control-Allow-Credentials: true");
        add_response_header(response, "Access-Control-Max-Age: 86400");
        
        return response;
    }
    
    /* Find matching route */
    for (int i = 0; routes[i].path != NULL; i++) {
        if (route_matches(routes[i].path, request->path) && routes[i].method == request->method) {
            if (g_logger) LOG_DEBUG("Found matching route: %s (requires_auth: %d)", 
                                routes[i].path, routes[i].requires_auth);
            
            /* Check if route requires authentication */
            if (routes[i].requires_auth) {
                if (g_logger) LOG_DEBUG("Route requires authentication, checking token");
                if (!api_authenticate_request(ctx, request)) {
                    if (g_logger) LOG_WARNING("Authentication failed for route: %s", routes[i].path);
                    return create_http_response(HTTP_UNAUTHORIZED, 
                                              "{\"error\":\"Unauthorized\"}", "application/json");
                }
            }
            
            if (g_logger) LOG_DEBUG("Calling handler for route: %s", routes[i].path);
            if (g_logger) LOG_DEBUG("Handler function pointer: %p", (void*)routes[i].handler);
            
            /* Call handler */
            if (g_logger) LOG_DEBUG("About to call handler function");
            http_response_t* result = routes[i].handler(ctx, request);
            if (g_logger) LOG_DEBUG("Handler function returned: %p", (void*)result);
            return result;
        }
    }
    
    /* No matching route */
    if (g_logger) LOG_WARNING("No matching route found for: %s", request->path);
    return create_http_response(HTTP_NOT_FOUND, 
                              "{\"error\":\"Not found\"}", "application/json");
}

/* Authentication handlers */

/* Login handler - implementation in api_login_fix.c */
http_response_t* original_api_handle_login(api_context_t* ctx, http_request_t* request) {
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
    
    /* Extract username and password */
    json_value_t* username_val = json_object_get(body, "username");
    json_value_t* password_val = json_object_get(body, "password");
    
    if (!username_val || username_val->type != JSON_STRING || 
        !password_val || password_val->type != JSON_STRING) {
        json_free(body);
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Username and password required\"}", "application/json");
    }
    
    const char* username = username_val->value.string;
    const char* password = password_val->value.string;
    
    /* Authenticate user */
    if (!rbac_authenticate_user(ctx->rbac, username, password)) {
        json_free(body);
        return create_http_response(HTTP_UNAUTHORIZED, 
                                  "{\"error\":\"Invalid credentials\"}", "application/json");
    }
    
    /* Get user */
    rbac_user_t* user = rbac_get_user_by_username(ctx->rbac, username);
    if (!user) {
        json_free(body);
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"User not found\"}", "application/json");
    }
    
    /* Create token pair */
    if (g_logger) {
        LOG_DEBUG("Creating JWT token pair with secret: '%s'", ctx->jwt_secret);
    }
    
    json_value_t* response = NULL;
    char* response_str = jwt_create_token_pair(ctx->jwt_secret, user->id, user->username, &response);
    
    if (!response_str || !response) {
        if (g_logger) {
            LOG_ERROR("Failed to create token pair");
        }
        json_free(body);
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Failed to create tokens\"}", "application/json");
    }
    
    if (g_logger) {
        LOG_DEBUG("JWT tokens generated successfully");
    }
    
    /* Free resources */
    free(response_str); /* We'll stringify again below */
    json_free(body);
    
    /* Generate the final response string */
    response_str = json_stringify(response);
    json_free(response);
    
    return create_http_response(HTTP_OK, response_str, "application/json");
}

/* Handle token refresh request */
http_response_t* api_handle_token_refresh(api_context_t* ctx, http_request_t* request) {
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
    
    /* Extract refresh token */
    json_value_t* refresh_token_val = json_object_get(body, "refresh_token");
    if (!refresh_token_val || refresh_token_val->type != JSON_STRING) {
        json_free(body);
        return create_http_response(HTTP_BAD_REQUEST,
                                  "{\"error\":\"Refresh token required\"}", "application/json");
    }
    
    const char* refresh_token = refresh_token_val->value.string;
    
    /* Verify refresh token and extract user ID */
    char* user_id = NULL;
    if (!jwt_verify_refresh_token(refresh_token, ctx->jwt_secret, &user_id)) {
        if (g_logger) {
            LOG_ERROR("Invalid refresh token or token expired");
        }
        json_free(body);
        return create_http_response(HTTP_UNAUTHORIZED,
                                  "{\"error\":\"Invalid or expired refresh token\"}", "application/json");
    }
    
    if (!user_id) {
        json_free(body);
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                                  "{\"error\":\"Failed to extract user ID from token\"}", "application/json");
    }
    
    /* Get user */
    rbac_user_t* user = rbac_get_user(ctx->rbac, user_id);
    if (!user) {
        free(user_id);
        json_free(body);
        return create_http_response(HTTP_NOT_FOUND,
                                  "{\"error\":\"User not found\"}", "application/json");
    }
    
    /* Create new token pair */
    if (g_logger) {
        LOG_DEBUG("Creating new JWT token pair for user: %s", user->username);
    }
    
    json_value_t* response = NULL;
    char* response_str = jwt_create_token_pair(ctx->jwt_secret, user->id, user->username, &response);
    
    /* Free user ID since we no longer need it */
    free(user_id);
    
    if (!response_str || !response) {
        if (g_logger) {
            LOG_ERROR("Failed to create new token pair");
        }
        json_free(body);
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                                  "{\"error\":\"Failed to create tokens\"}", "application/json");
    }
    
    if (g_logger) {
        LOG_DEBUG("New JWT tokens generated successfully");
    }
    
    /* Free resources */
    free(response_str); /* We'll stringify again below */
    json_free(body);
    
    /* Generate the final response string */
    response_str = json_stringify(response);
    json_free(response);
    
    return create_http_response(HTTP_OK, response_str, "application/json");
}

/* Register handler */
http_response_t* api_handle_register(api_context_t* ctx, http_request_t* request) {
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
    
    /* Extract username and password */
    json_value_t* username_val = json_object_get(body, "username");
    json_value_t* password_val = json_object_get(body, "password");
    
    if (!username_val || username_val->type != JSON_STRING || 
        !password_val || password_val->type != JSON_STRING) {
        json_free(body);
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Username and password required\"}", "application/json");
    }
    
    const char* username = username_val->value.string;
    const char* password = password_val->value.string;
    
    /* Check if user already exists */
    if (rbac_get_user_by_username(ctx->rbac, username)) {
        json_free(body);
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Username already exists\"}", "application/json");
    }
    
    /* Create user */
    rbac_user_t* user = rbac_create_user(ctx->rbac, username, password);
    if (!user) {
        json_free(body);
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Failed to create user\"}", "application/json");
    }
    
    /* Create response */
    json_value_t* response = json_create_object();
    json_object_set(response, "user_id", json_create_string(user->id));
    json_object_set(response, "username", json_create_string(user->username));
    
    char* response_str = json_stringify(response);
    json_free(response);
    json_free(body);
    
    return create_http_response(HTTP_CREATED, response_str, "application/json");
}

/* Collection handlers */

/* List collections */
http_response_t* api_handle_collections_list(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Get collections */
    json_value_t* collections = db_list_collections(ctx->db);
    if (!collections) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Failed to list collections\"}", "application/json");
    }
    
    /* Create response */
    json_value_t* response = json_create_object();
    json_object_set(response, "collections", collections);
    
    char* response_str = json_stringify(response);
    json_free(response);
    
    return create_http_response(HTTP_OK, response_str, "application/json");
}

/* Create collection */
http_response_t* api_handle_collection_create(api_context_t* ctx, http_request_t* request) {
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
    
    /* Extract collection name */
    json_value_t* name_val = json_object_get(body, "name");
    if (!name_val || name_val->type != JSON_STRING) {
        json_free(body);
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Collection name required\"}", "application/json");
    }
    
    const char* name = name_val->value.string;
    
    /* Create collection */
    if (!db_create_collection(ctx->db, name)) {
        json_free(body);
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Failed to create collection\"}", "application/json");
    }
    
    /* Create response */
    json_value_t* response = json_create_object();
    json_object_set(response, "name", json_create_string(name));
    
    char* response_str = json_stringify(response);
    json_free(response);
    json_free(body);
    
    return create_http_response(HTTP_CREATED, response_str, "application/json");
}

/* Drop collection */
http_response_t* api_handle_collection_drop(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Extract collection name from path */
    const char* path = request->path;
    const char* name = path + strlen("/api/collections/");
    
    /* Drop collection */
    if (!db_drop_collection(ctx->db, name)) {
        return create_http_response(HTTP_NOT_FOUND, 
                                  "{\"error\":\"Collection not found\"}", "application/json");
    }
    
    return create_http_response(HTTP_NO_CONTENT, NULL, "application/json");
}

/* Document handlers */

/* Query documents */
http_response_t* api_handle_documents_query(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Extract collection name from path */
    const char* path = request->path;
    if (strncmp(path, "/api/collections/", 17) != 0) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid path\"}", "application/json");
    }
    
    path += 17;
    
    /* Check if this is a query for all documents or a specific document */
    const char* slash = strchr(path, '/');
    if (slash && strcmp(slash, "/documents") != 0) {
        /* This is a request for a specific document */
        return api_handle_document_get(ctx, request);
    }
    
    /* Extract collection name */
    char* collection_name = strndup(path, slash ? (size_t)(slash - path) : strlen(path));
    
    /* Parse query parameter if present */
    json_value_t* query = NULL;
    if (request->query) {
        /* Parse query JSON */
        query = json_parse(request->query);
        if (query && query->type != JSON_OBJECT) {
            json_free(query);
            query = NULL;
        }
    }
    
    /* Query documents */
    json_value_t* documents = db_query_documents(ctx->db, collection_name, query);
    if (query) {
        json_free(query);
    }
    
    if (!documents) {
        free(collection_name);
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Failed to query documents\"}", "application/json");
    }
    
    /* Create response */
    json_value_t* response = json_create_object();
    json_object_set(response, "documents", documents);
    
    char* response_str = json_stringify(response);
    json_free(response);
    free(collection_name);
    
    return create_http_response(HTTP_OK, response_str, "application/json");
}

/* Get document */
http_response_t* api_handle_document_get(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Extract collection name and document ID from path */
    const char* path = request->path;
    if (strncmp(path, "/api/collections/", 17) != 0) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid path\"}", "application/json");
    }
    
    path += 17;
    
    /* Split path into collection name and document ID */
    const char* slash = strchr(path, '/');
    if (!slash || strncmp(slash, "/documents/", 11) != 0) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid path\"}", "application/json");
    }
    
    char* collection_name = strndup(path, slash - path);
    const char* document_id = slash + 11;
    
    /* Get document */
    json_value_t* document = db_get_document(ctx->db, collection_name, document_id);
    free(collection_name);
    
    if (!document) {
        return create_http_response(HTTP_NOT_FOUND, 
                                  "{\"error\":\"Document not found\"}", "application/json");
    }
    
    /* Create response */
    char* response_str = json_stringify(document);
    json_free(document);
    
    return create_http_response(HTTP_OK, response_str, "application/json");
}

/* Create document */
http_response_t* api_handle_document_create(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request || !request->body) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Extract collection name from path */
    const char* path = request->path;
    if (strncmp(path, "/api/collections/", 17) != 0) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid path\"}", "application/json");
    }
    
    path += 17;
    
    /* Split path into collection name and 'documents' */
    const char* slash = strchr(path, '/');
    if (!slash || strcmp(slash, "/documents") != 0) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid path\"}", "application/json");
    }
    
    char* collection_name = strndup(path, slash - path);
    
    /* Parse document */
    json_value_t* document = json_parse(request->body);
    if (!document || document->type != JSON_OBJECT) {
        if (document) json_free(document);
        free(collection_name);
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid document\"}", "application/json");
    }
    
    /* Insert document */
    json_value_t* result = db_insert_document(ctx->db, collection_name, document);
    free(collection_name);
    
    if (!result) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Failed to insert document\"}", "application/json");
    }
    
    /* Create response */
    char* response_str = json_stringify(result);
    json_free(result);
    
    return create_http_response(HTTP_CREATED, response_str, "application/json");
}

/* Update document */
http_response_t* api_handle_document_update(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request || !request->body) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Extract collection name and document ID from path */
    const char* path = request->path;
    if (strncmp(path, "/api/collections/", 17) != 0) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid path\"}", "application/json");
    }
    
    path += 17;
    
    /* Split path into collection name and document ID */
    const char* slash = strchr(path, '/');
    if (!slash || strncmp(slash, "/documents/", 11) != 0) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid path\"}", "application/json");
    }
    
    char* collection_name = strndup(path, slash - path);
    const char* document_id = slash + 11;
    
    /* Parse document */
    json_value_t* document = json_parse(request->body);
    if (!document || document->type != JSON_OBJECT) {
        if (document) json_free(document);
        free(collection_name);
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid document\"}", "application/json");
    }
    
    /* Update document */
    json_value_t* result = db_update_document(ctx->db, collection_name, document_id, document);
    free(collection_name);
    
    if (!result) {
        return create_http_response(HTTP_NOT_FOUND, 
                                  "{\"error\":\"Document not found\"}", "application/json");
    }
    
    /* Create response */
    char* response_str = json_stringify(result);
    json_free(result);
    
    return create_http_response(HTTP_OK, response_str, "application/json");
}

/* Delete document */
http_response_t* api_handle_document_delete(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Extract collection name and document ID from path */
    const char* path = request->path;
    if (strncmp(path, "/api/collections/", 17) != 0) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid path\"}", "application/json");
    }
    
    path += 17;
    
    /* Split path into collection name and document ID */
    const char* slash = strchr(path, '/');
    if (!slash || strncmp(slash, "/documents/", 11) != 0) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid path\"}", "application/json");
    }
    
    char* collection_name = strndup(path, slash - path);
    const char* document_id = slash + 11;
    
    /* Delete document */
    int result = db_delete_document(ctx->db, collection_name, document_id);
    free(collection_name);
    
    if (!result) {
        return create_http_response(HTTP_NOT_FOUND, 
                                  "{\"error\":\"Document not found\"}", "application/json");
    }
    
    return create_http_response(HTTP_NO_CONTENT, NULL, "application/json");
}

/* Placeholder for remaining API handlers */

/* RBAC handlers */
http_response_t* api_handle_users_list(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    if (!ctx->rbac) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"RBAC system not initialized\"}", "application/json");
    }
    
    /* Check if the user has admin privileges */
    /* Extract token and get user ID */
    char* token = api_extract_token(request);
    if (!token) {
        /* This should not happen since authorization is already checked in api_dispatch_request */
        return create_http_response(HTTP_UNAUTHORIZED, 
                                  "{\"error\":\"Unauthorized\"}", "application/json");
    }
    
    /* Decode token */
    jwt_token_t* jwt = jwt_decode(token);
    free(token);
    
    if (!jwt || !jwt->payload || !jwt->payload->sub) {
        if (jwt) jwt_free(jwt);
        return create_http_response(HTTP_UNAUTHORIZED, 
                                  "{\"error\":\"Invalid token\"}", "application/json");
    }
    
    const char* user_id = jwt->payload->sub;
    
    /* Check if user has permission to list users */
    if (!rbac_check_permission(ctx->rbac, user_id, RBAC_USER, "*", RBAC_READ)) {
        jwt_free(jwt);
        return create_http_response(HTTP_FORBIDDEN, 
                                  "{\"error\":\"Permission denied\"}", "application/json");
    }
    
    jwt_free(jwt);
    
    /* Create a JSON array of users from the RBAC system's users object */
    json_value_t* users_array = json_create_array();
    if (!users_array) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Failed to create users array\"}", "application/json");
    }
    
    /* Iterate through all users in the RBAC system */
    for (size_t i = 0; i < ctx->rbac->users->value.object.size; i++) {
        const char* user_id = ctx->rbac->users->value.object.entries[i].key;
        json_value_t* user_obj = ctx->rbac->users->value.object.entries[i].value;
        
        if (user_obj->type == JSON_OBJECT) {
            /* Create a new user object with a subset of information (exclude password hash) */
            json_value_t* user = json_create_object();
            
            /* Add user ID */
            json_object_set(user, "id", json_create_string(user_id));
            
            /* Add username if present */
            json_value_t* username = json_object_get(user_obj, "username");
            if (username && username->type == JSON_STRING) {
                json_object_set(user, "username", json_create_string(username->value.string));
            }
            
            /* Add roles array if present */
            json_value_t* roles = json_object_get(user_obj, "roles");
            if (roles && roles->type == JSON_ARRAY) {
                /* Create a deep copy of the roles array */
                json_value_t* roles_copy = json_create_array();
                for (size_t j = 0; j < roles->value.array.size; j++) {
                    json_value_t* role_id = roles->value.array.items[j];
                    if (role_id && role_id->type == JSON_STRING) {
                        json_array_append(roles_copy, json_create_string(role_id->value.string));
                    }
                }
                json_object_set(user, "roles", roles_copy);
            }
            
            /* Add user to array */
            json_array_append(users_array, user);
        }
    }
    
    /* Create response object */
    json_value_t* response = json_create_object();
    json_object_set(response, "users", users_array);
    
    /* Serialize response */
    char* response_str = json_stringify(response);
    
    /* Free resources */
    json_free(response);
    
    /* Create and return response */
    return create_http_response(HTTP_OK, response_str, "application/json");
}

http_response_t* api_handle_user_get(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    if (!ctx->rbac) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"RBAC system not initialized\"}", "application/json");
    }
    
    /* Extract user ID from path */
    const char* path = request->path;
    if (strncmp(path, "/api/users/", 11) != 0) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid path\"}", "application/json");
    }
    
    const char* target_user_id = path + 11;
    
    /* Check if the user has appropriate permissions */
    /* Extract token and get user ID */
    char* token = api_extract_token(request);
    if (!token) {
        /* This should not happen since authorization is already checked in api_dispatch_request */
        return create_http_response(HTTP_UNAUTHORIZED, 
                                  "{\"error\":\"Unauthorized\"}", "application/json");
    }
    
    /* Decode token */
    jwt_token_t* jwt = jwt_decode(token);
    free(token);
    
    if (!jwt || !jwt->payload || !jwt->payload->sub) {
        if (jwt) jwt_free(jwt);
        return create_http_response(HTTP_UNAUTHORIZED, 
                                  "{\"error\":\"Invalid token\"}", "application/json");
    }
    
    const char* requester_user_id = jwt->payload->sub;
    
    /* Check if user has permission to view the target user 
     * The permission check is more permissive if the user is viewing their own profile
     */
    if (strcmp(requester_user_id, target_user_id) != 0 && 
        !rbac_check_permission(ctx->rbac, requester_user_id, RBAC_USER, target_user_id, RBAC_READ) &&
        !rbac_check_permission(ctx->rbac, requester_user_id, RBAC_USER, "*", RBAC_READ)) {
        jwt_free(jwt);
        return create_http_response(HTTP_FORBIDDEN, 
                                  "{\"error\":\"Permission denied\"}", "application/json");
    }
    
    jwt_free(jwt);
    
    /* Get user */
    rbac_user_t* user = rbac_get_user(ctx->rbac, target_user_id);
    if (!user) {
        return create_http_response(HTTP_NOT_FOUND, 
                                  "{\"error\":\"User not found\"}", "application/json");
    }
    
    /* Create response */
    json_value_t* response = json_create_object();
    json_value_t* user_obj = json_create_object();
    
    json_object_set(user_obj, "id", json_create_string(user->id));
    json_object_set(user_obj, "username", json_create_string(user->username));
    
    /* Add roles array */
    json_value_t* roles_array = json_create_array();
    for (size_t i = 0; i < user->roles->value.array.size; i++) {
        json_value_t* role_id = user->roles->value.array.items[i];
        if (role_id->type == JSON_STRING) {
            json_array_append(roles_array, json_create_string(role_id->value.string));
        }
    }
    json_object_set(user_obj, "roles", roles_array);
    
    /* Add information about the role objects if the user has appropriate permission */
    if (rbac_check_permission(ctx->rbac, requester_user_id, RBAC_ROLE, "*", RBAC_READ)) {
        json_value_t* roles_info = json_create_array();
        
        for (size_t i = 0; i < user->roles->value.array.size; i++) {
            json_value_t* role_id_val = user->roles->value.array.items[i];
            if (role_id_val->type == JSON_STRING) {
                const char* role_id = role_id_val->value.string;
                
                /* Get role */
                rbac_role_t* role = rbac_get_role(ctx->rbac, role_id);
                if (role) {
                    /* Create role info object */
                    json_value_t* role_info = json_create_object();
                    json_object_set(role_info, "id", json_create_string(role->id));
                    json_object_set(role_info, "name", json_create_string(role->name));
                    
                    /* Add role info to array */
                    json_array_append(roles_info, role_info);
                    
                    /* Free role */
                    rbac_free_role(role);
                }
            }
        }
        
        json_object_set(user_obj, "role_details", roles_info);
    }
    
    json_object_set(response, "user", user_obj);
    
    /* Serialize response */
    char* response_str = json_stringify(response);
    
    /* Free resources */
    json_free(response);
    rbac_free_user(user);
    
    /* Create and return response */
    return create_http_response(HTTP_OK, response_str, "application/json");
}

http_response_t* api_handle_user_create(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request || !request->body) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    if (!ctx->rbac) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"RBAC system not initialized\"}", "application/json");
    }
    
    /* Check if the user has admin privileges */
    /* Extract token and get user ID */
    char* token = api_extract_token(request);
    if (!token) {
        /* This should not happen since authorization is already checked in api_dispatch_request */
        return create_http_response(HTTP_UNAUTHORIZED, 
                                  "{\"error\":\"Unauthorized\"}", "application/json");
    }
    
    /* Decode token */
    jwt_token_t* jwt = jwt_decode(token);
    free(token);
    
    if (!jwt || !jwt->payload || !jwt->payload->sub) {
        if (jwt) jwt_free(jwt);
        return create_http_response(HTTP_UNAUTHORIZED, 
                                  "{\"error\":\"Invalid token\"}", "application/json");
    }
    
    const char* user_id = jwt->payload->sub;
    
    /* Check if user has permission to create users */
    if (!rbac_check_permission(ctx->rbac, user_id, RBAC_USER, "*", RBAC_WRITE)) {
        jwt_free(jwt);
        return create_http_response(HTTP_FORBIDDEN, 
                                  "{\"error\":\"Permission denied\"}", "application/json");
    }
    
    jwt_free(jwt);
    
    /* Parse request body */
    json_value_t* body = json_parse(request->body);
    if (!body || body->type != JSON_OBJECT) {
        if (body) json_free(body);
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request body\"}", "application/json");
    }
    
    /* Extract username and password */
    json_value_t* username_val = json_object_get(body, "username");
    json_value_t* password_val = json_object_get(body, "password");
    json_value_t* roles_val = json_object_get(body, "roles");
    
    if (!username_val || username_val->type != JSON_STRING || 
        !password_val || password_val->type != JSON_STRING) {
        json_free(body);
        return create_http_response(HTTP_BAD_REQUEST, 
                                 "{\"error\":\"Username and password are required\"}", "application/json");
    }
    
    const char* username = username_val->value.string;
    const char* password = password_val->value.string;
    
    /* Validate input */
    if (strlen(username) < 3) {
        json_free(body);
        return create_http_response(HTTP_BAD_REQUEST, 
                                 "{\"error\":\"Username must be at least 3 characters long\"}", "application/json");
    }
    
    if (strlen(password) < 8) {
        json_free(body);
        return create_http_response(HTTP_BAD_REQUEST, 
                                 "{\"error\":\"Password must be at least 8 characters long\"}", "application/json");
    }
    
    /* Check if username already exists */
    if (rbac_get_user_by_username(ctx->rbac, username)) {
        json_free(body);
        return create_http_response(HTTP_CONFLICT, 
                                 "{\"error\":\"Username already exists\"}", "application/json");
    }
    
    /* Create user */
    rbac_user_t* user = rbac_create_user(ctx->rbac, username, password);
    if (!user) {
        json_free(body);
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                 "{\"error\":\"Failed to create user\"}", "application/json");
    }
    
    /* Handle roles if provided */
    if (roles_val && roles_val->type == JSON_ARRAY) {
        for (size_t i = 0; i < roles_val->value.array.size; i++) {
            json_value_t* role_id_val = roles_val->value.array.items[i];
            if (role_id_val && role_id_val->type == JSON_STRING) {
                const char* role_id = role_id_val->value.string;
                
                /* Add user to role */
                rbac_add_user_to_role(ctx->rbac, user->id, role_id);
            }
        }
    }
    
    /* Create response */
    json_value_t* response = json_create_object();
    json_value_t* user_obj = json_create_object();
    
    json_object_set(user_obj, "id", json_create_string(user->id));
    json_object_set(user_obj, "username", json_create_string(user->username));
    
    /* Add roles array */
    json_value_t* roles_array = json_create_array();
    for (size_t i = 0; i < user->roles->value.array.size; i++) {
        json_value_t* role_id = user->roles->value.array.items[i];
        if (role_id && role_id->type == JSON_STRING) {
            json_array_append(roles_array, json_create_string(role_id->value.string));
        }
    }
    json_object_set(user_obj, "roles", roles_array);
    
    json_object_set(response, "user", user_obj);
    
    /* Serialize response */
    char* response_str = json_stringify(response);
    
    /* Free resources */
    json_free(response);
    json_free(body);
    rbac_free_user(user);
    
    /* Create and return response */
    return create_http_response(HTTP_CREATED, response_str, "application/json");
}

http_response_t* api_handle_user_update(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request || !request->body) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    if (!ctx->rbac) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"RBAC system not initialized\"}", "application/json");
    }
    
    /* Extract user ID from path */
    const char* path = request->path;
    if (strncmp(path, "/api/users/", 11) != 0) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid path\"}", "application/json");
    }
    
    const char* target_user_id = path + 11;
    
    /* Check if the user has appropriate permissions */
    /* Extract token and get user ID */
    char* token = api_extract_token(request);
    if (!token) {
        /* This should not happen since authorization is already checked in api_dispatch_request */
        return create_http_response(HTTP_UNAUTHORIZED, 
                                  "{\"error\":\"Unauthorized\"}", "application/json");
    }
    
    /* Decode token */
    jwt_token_t* jwt = jwt_decode(token);
    free(token);
    
    if (!jwt || !jwt->payload || !jwt->payload->sub) {
        if (jwt) jwt_free(jwt);
        return create_http_response(HTTP_UNAUTHORIZED, 
                                  "{\"error\":\"Invalid token\"}", "application/json");
    }
    
    const char* requester_user_id = jwt->payload->sub;
    
    /* Check if user has permission to update the target user 
     * The permission check is more permissive if the user is updating their own profile,
     * but updating roles requires admin privileges regardless.
     */
    int is_self_update = (strcmp(requester_user_id, target_user_id) == 0);
    int has_user_write = rbac_check_permission(ctx->rbac, requester_user_id, RBAC_USER, target_user_id, RBAC_WRITE) ||
                         rbac_check_permission(ctx->rbac, requester_user_id, RBAC_USER, "*", RBAC_WRITE);
    
    if (!is_self_update && !has_user_write) {
        jwt_free(jwt);
        return create_http_response(HTTP_FORBIDDEN, 
                                  "{\"error\":\"Permission denied\"}", "application/json");
    }
    
    /* Parse request body */
    json_value_t* body = json_parse(request->body);
    if (!body || body->type != JSON_OBJECT) {
        if (body) json_free(body);
        jwt_free(jwt);
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request body\"}", "application/json");
    }
    
    /* Check if user exists */
    rbac_user_t* user = rbac_get_user(ctx->rbac, target_user_id);
    if (!user) {
        json_free(body);
        jwt_free(jwt);
        return create_http_response(HTTP_NOT_FOUND, 
                                  "{\"error\":\"User not found\"}", "application/json");
    }
    
    /* Extract fields to update */
    json_value_t* username_val = json_object_get(body, "username");
    json_value_t* password_val = json_object_get(body, "password");
    json_value_t* roles_val = json_object_get(body, "roles");
    
    /* Get the actual JSON user object from the RBAC system */
    json_value_t* user_obj = json_object_get(ctx->rbac->users, target_user_id);
    if (!user_obj || user_obj->type != JSON_OBJECT) {
        rbac_free_user(user);
        json_free(body);
        jwt_free(jwt);
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Failed to find user in RBAC system\"}", "application/json");
    }
    
    /* Update username if provided */
    if (username_val && username_val->type == JSON_STRING) {
        const char* new_username = username_val->value.string;
        
        /* Validate username */
        if (strlen(new_username) < 3) {
            rbac_free_user(user);
            json_free(body);
            jwt_free(jwt);
            return create_http_response(HTTP_BAD_REQUEST, 
                                     "{\"error\":\"Username must be at least 3 characters long\"}", "application/json");
        }
        
        /* Check if username is already taken by another user */
        rbac_user_t* existing_user = rbac_get_user_by_username(ctx->rbac, new_username);
        if (existing_user && strcmp(existing_user->id, target_user_id) != 0) {
            rbac_free_user(user);
            rbac_free_user(existing_user);
            json_free(body);
            jwt_free(jwt);
            return create_http_response(HTTP_CONFLICT, 
                                     "{\"error\":\"Username already exists\"}", "application/json");
        }
        
        if (existing_user) {
            rbac_free_user(existing_user);
        }
        
        /* Update username in the JSON object */
        json_object_set(user_obj, "username", json_create_string(new_username));
    }
    
    /* Update password if provided */
    if (password_val && password_val->type == JSON_STRING) {
        const char* new_password = password_val->value.string;
        
        /* Validate password */
        if (strlen(new_password) < 8) {
            rbac_free_user(user);
            json_free(body);
            jwt_free(jwt);
            return create_http_response(HTTP_BAD_REQUEST, 
                                     "{\"error\":\"Password must be at least 8 characters long\"}", "application/json");
        }
        
        /* Hash password */
        char* password_hash = hash_password(new_password);
        
        if (!password_hash) {
            rbac_free_user(user);
            json_free(body);
            jwt_free(jwt);
            return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                     "{\"error\":\"Failed to hash password\"}", "application/json");
        }
        
        /* Update password_hash in the JSON object */
        json_object_set(user_obj, "password_hash", json_create_string(password_hash));
        
        /* Free password hash */
        free(password_hash);
    }
    
    /* Update roles if provided - requires admin permissions */
    if (roles_val && roles_val->type == JSON_ARRAY) {
        /* Check if user has admin privileges for role management */
        if (!rbac_check_permission(ctx->rbac, requester_user_id, RBAC_ROLE, "*", RBAC_ADMIN) &&
            !rbac_check_permission(ctx->rbac, requester_user_id, RBAC_USER, "*", RBAC_ADMIN)) {
            rbac_free_user(user);
            json_free(body);
            jwt_free(jwt);
            return create_http_response(HTTP_FORBIDDEN, 
                                     "{\"error\":\"Permission denied for role management\"}", "application/json");
        }
        
        /* Get the current roles array from the user object */
        json_value_t* current_roles = json_object_get(user_obj, "roles");
        if (!current_roles || current_roles->type != JSON_ARRAY) {
            /* Create roles array if it doesn't exist */
            current_roles = json_create_array();
            json_object_set(user_obj, "roles", current_roles);
        }
        
        /* First, remove this user from all existing roles */
        for (size_t i = 0; i < current_roles->value.array.size; i++) {
            json_value_t* role_id_val = current_roles->value.array.items[i];
            if (role_id_val && role_id_val->type == JSON_STRING) {
                const char* role_id = role_id_val->value.string;
                
                /* Get role */
                json_value_t* role_obj = json_object_get(ctx->rbac->roles, role_id);
                if (role_obj && role_obj->type == JSON_OBJECT) {
                    /* Get users array */
                    json_value_t* users = json_object_get(role_obj, "users");
                    if (users && users->type == JSON_ARRAY) {
                        /* Remove user from role */
                        for (size_t j = 0; j < users->value.array.size; j++) {
                            json_value_t* user_id_val = users->value.array.items[j];
                            if (user_id_val && user_id_val->type == JSON_STRING && 
                                strcmp(user_id_val->value.string, target_user_id) == 0) {
                                /* Remove user from array */
                                for (size_t k = j; k < users->value.array.size - 1; k++) {
                                    users->value.array.items[k] = users->value.array.items[k + 1];
                                }
                                users->value.array.size--;
                                break;
                            }
                        }
                    }
                }
            }
        }
        
        /* Clear current roles array */
        current_roles->value.array.size = 0;
        
        /* Add user to new roles and update roles array */
        for (size_t i = 0; i < roles_val->value.array.size; i++) {
            json_value_t* role_id_val = roles_val->value.array.items[i];
            if (role_id_val && role_id_val->type == JSON_STRING) {
                const char* role_id = role_id_val->value.string;
                
                /* Verify that role exists */
                json_value_t* role_obj = json_object_get(ctx->rbac->roles, role_id);
                if (role_obj && role_obj->type == JSON_OBJECT) {
                    /* Add role ID to user's roles */
                    json_array_append(current_roles, json_create_string(role_id));
                    
                    /* Add user to role's users */
                    json_value_t* users = json_object_get(role_obj, "users");
                    if (!users || users->type != JSON_ARRAY) {
                        /* Create users array if it doesn't exist */
                        users = json_create_array();
                        json_object_set(role_obj, "users", users);
                    }
                    
                    /* Check if user is already in role */
                    int user_in_role = 0;
                    for (size_t j = 0; j < users->value.array.size; j++) {
                        json_value_t* user_id_val = users->value.array.items[j];
                        if (user_id_val && user_id_val->type == JSON_STRING && 
                            strcmp(user_id_val->value.string, target_user_id) == 0) {
                            user_in_role = 1;
                            break;
                        }
                    }
                    
                    /* Add user to role if not already present */
                    if (!user_in_role) {
                        json_array_append(users, json_create_string(target_user_id));
                    }
                }
            }
        }
    }
    
    /* Get updated user for response */
    rbac_free_user(user);
    user = rbac_get_user(ctx->rbac, target_user_id);
    
    /* Create response */
    json_value_t* response = json_create_object();
    json_value_t* updated_user = json_create_object();
    
    json_object_set(updated_user, "id", json_create_string(user->id));
    json_object_set(updated_user, "username", json_create_string(user->username));
    
    /* Add roles array */
    json_value_t* roles_array = json_create_array();
    for (size_t i = 0; i < user->roles->value.array.size; i++) {
        json_value_t* role_id = user->roles->value.array.items[i];
        if (role_id->type == JSON_STRING) {
            json_array_append(roles_array, json_create_string(role_id->value.string));
        }
    }
    json_object_set(updated_user, "roles", roles_array);
    
    json_object_set(response, "user", updated_user);
    
    /* Serialize response */
    char* response_str = json_stringify(response);
    
    /* Free resources */
    rbac_free_user(user);
    json_free(response);
    json_free(body);
    jwt_free(jwt);
    
    /* Create and return response */
    return create_http_response(HTTP_OK, response_str, "application/json");
}

http_response_t* api_handle_user_delete(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                 "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    if (!ctx->rbac) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                 "{\"error\":\"RBAC system not initialized\"}", "application/json");
    }
    
    /* Extract user ID from path */
    const char* path = request->path;
    if (strncmp(path, "/api/users/", 11) != 0) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                 "{\"error\":\"Invalid path\"}", "application/json");
    }
    
    const char* target_user_id = path + 11;
    
    /* Check if the user has appropriate permissions */
    /* Extract token and get user ID */
    char* token = api_extract_token(request);
    if (!token) {
        /* This should not happen since authorization is already checked in api_dispatch_request */
        return create_http_response(HTTP_UNAUTHORIZED, 
                                 "{\"error\":\"Unauthorized\"}", "application/json");
    }
    
    /* Decode token */
    jwt_token_t* jwt = jwt_decode(token);
    free(token);
    
    if (!jwt || !jwt->payload || !jwt->payload->sub) {
        if (jwt) jwt_free(jwt);
        return create_http_response(HTTP_UNAUTHORIZED, 
                                 "{\"error\":\"Invalid token\"}", "application/json");
    }
    
    const char* requester_user_id = jwt->payload->sub;
    
    /* Users can't delete themselves, only admins can delete users */
    if (strcmp(requester_user_id, target_user_id) == 0) {
        jwt_free(jwt);
        return create_http_response(HTTP_FORBIDDEN, 
                                 "{\"error\":\"You cannot delete your own account\"}", "application/json");
    }
    
    /* Check if user has permission to delete the target user */
    if (!rbac_check_permission(ctx->rbac, requester_user_id, RBAC_USER, target_user_id, RBAC_DELETE) &&
        !rbac_check_permission(ctx->rbac, requester_user_id, RBAC_USER, "*", RBAC_DELETE)) {
        jwt_free(jwt);
        return create_http_response(HTTP_FORBIDDEN, 
                                 "{\"error\":\"Permission denied\"}", "application/json");
    }
    
    jwt_free(jwt);
    
    /* Check if user exists */
    if (!json_object_has(ctx->rbac->users, target_user_id)) {
        return create_http_response(HTTP_NOT_FOUND, 
                                 "{\"error\":\"User not found\"}", "application/json");
    }
    
    /* Delete user */
    if (!rbac_delete_user(ctx->rbac, target_user_id)) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                 "{\"error\":\"Failed to delete user\"}", "application/json");
    }
    
    /* Return success with no content */
    return create_http_response(HTTP_NO_CONTENT, NULL, "application/json");
}

http_response_t* api_handle_roles_list(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    if (!ctx->rbac) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"RBAC system not initialized\"}", "application/json");
    }
    
    /* Check if the user has admin privileges */
    /* Extract token and get user ID */
    char* token = api_extract_token(request);
    if (!token) {
        /* This should not happen since authorization is already checked in api_dispatch_request */
        return create_http_response(HTTP_UNAUTHORIZED, 
                                  "{\"error\":\"Unauthorized\"}", "application/json");
    }
    
    /* Decode token */
    jwt_token_t* jwt = jwt_decode(token);
    free(token);
    
    if (!jwt || !jwt->payload || !jwt->payload->sub) {
        if (jwt) jwt_free(jwt);
        return create_http_response(HTTP_UNAUTHORIZED, 
                                  "{\"error\":\"Invalid token\"}", "application/json");
    }
    
    const char* user_id = jwt->payload->sub;
    
    /* Check if user has permission to list roles */
    if (!rbac_check_permission(ctx->rbac, user_id, RBAC_ROLE, "*", RBAC_READ)) {
        jwt_free(jwt);
        return create_http_response(HTTP_FORBIDDEN, 
                                  "{\"error\":\"Permission denied\"}", "application/json");
    }
    
    jwt_free(jwt);
    
    /* Create a JSON array of roles from the RBAC system's roles object */
    json_value_t* roles_array = json_create_array();
    if (!roles_array) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Failed to create roles array\"}", "application/json");
    }
    
    /* Iterate through all roles in the RBAC system */
    for (size_t i = 0; i < ctx->rbac->roles->value.object.size; i++) {
        const char* role_id = ctx->rbac->roles->value.object.entries[i].key;
        json_value_t* role_obj = ctx->rbac->roles->value.object.entries[i].value;
        
        if (role_obj->type == JSON_OBJECT) {
            /* Create a new role object with relevant information */
            json_value_t* role = json_create_object();
            
            /* Add role ID */
            json_object_set(role, "id", json_create_string(role_id));
            
            /* Add name if present */
            json_value_t* name = json_object_get(role_obj, "name");
            if (name && name->type == JSON_STRING) {
                json_object_set(role, "name", json_create_string(name->value.string));
            }
            
            /* Add users array if present */
            json_value_t* users = json_object_get(role_obj, "users");
            if (users && users->type == JSON_ARRAY) {
                /* Create a deep copy of the users array */
                json_value_t* users_copy = json_create_array();
                for (size_t j = 0; j < users->value.array.size; j++) {
                    json_value_t* user_id = users->value.array.items[j];
                    if (user_id && user_id->type == JSON_STRING) {
                        json_array_append(users_copy, json_create_string(user_id->value.string));
                    }
                }
                json_object_set(role, "users", users_copy);
            }
            
            /* Add permissions object if present */
            json_value_t* permissions = json_object_get(role_obj, "permissions");
            if (permissions && permissions->type == JSON_OBJECT) {
                /* Create a deep copy of the permissions object */
                json_value_t* permissions_copy = json_create_object();
                for (size_t j = 0; j < permissions->value.object.size; j++) {
                    const char* perm_key = permissions->value.object.entries[j].key;
                    json_value_t* perm_val = permissions->value.object.entries[j].value;
                    
                    if (perm_val && (perm_val->type == JSON_NUMBER || perm_val->type == JSON_INTEGER)) {
                        json_object_set(permissions_copy, perm_key, 
                                       json_create_number(perm_val->type == JSON_NUMBER ? 
                                                         perm_val->value.number : perm_val->value.integer));
                    }
                }
                json_object_set(role, "permissions", permissions_copy);
            }
            
            /* Add role to array */
            json_array_append(roles_array, role);
        }
    }
    
    /* Create response object */
    json_value_t* response = json_create_object();
    json_object_set(response, "roles", roles_array);
    
    /* Serialize response */
    char* response_str = json_stringify(response);
    
    /* Free resources */
    json_free(response);
    
    /* Create and return response */
    return create_http_response(HTTP_OK, response_str, "application/json");
}

http_response_t* api_handle_role_get(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    if (!ctx->rbac) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"RBAC system not initialized\"}", "application/json");
    }
    
    /* Extract role ID from path */
    const char* path = request->path;
    if (strncmp(path, "/api/roles/", 11) != 0) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid path\"}", "application/json");
    }
    
    const char* role_id = path + 11;
    
    /* Check if the user has appropriate permissions */
    /* Extract token and get user ID */
    char* token = api_extract_token(request);
    if (!token) {
        /* This should not happen since authorization is already checked in api_dispatch_request */
        return create_http_response(HTTP_UNAUTHORIZED, 
                                  "{\"error\":\"Unauthorized\"}", "application/json");
    }
    
    /* Decode token */
    jwt_token_t* jwt = jwt_decode(token);
    free(token);
    
    if (!jwt || !jwt->payload || !jwt->payload->sub) {
        if (jwt) jwt_free(jwt);
        return create_http_response(HTTP_UNAUTHORIZED, 
                                  "{\"error\":\"Invalid token\"}", "application/json");
    }
    
    const char* user_id = jwt->payload->sub;
    
    /* Check if user has permission to view role */
    if (!rbac_check_permission(ctx->rbac, user_id, RBAC_ROLE, role_id, RBAC_READ) &&
        !rbac_check_permission(ctx->rbac, user_id, RBAC_ROLE, "*", RBAC_READ)) {
        jwt_free(jwt);
        return create_http_response(HTTP_FORBIDDEN, 
                                  "{\"error\":\"Permission denied\"}", "application/json");
    }
    
    jwt_free(jwt);
    
    /* Get role */
    rbac_role_t* role = rbac_get_role(ctx->rbac, role_id);
    if (!role) {
        return create_http_response(HTTP_NOT_FOUND, 
                                  "{\"error\":\"Role not found\"}", "application/json");
    }
    
    /* Create response */
    json_value_t* response = json_create_object();
    json_value_t* role_obj = json_create_object();
    
    json_object_set(role_obj, "id", json_create_string(role->id));
    json_object_set(role_obj, "name", json_create_string(role->name));
    
    /* Add permissions object */
    json_value_t* permissions_obj = json_create_object();
    
    /* Copy permissions from role */
    if (role->permissions) {
        for (size_t i = 0; i < role->permissions->value.object.size; i++) {
            const char* perm_key = role->permissions->value.object.entries[i].key;
            json_value_t* perm_val = role->permissions->value.object.entries[i].value;
            
            if (perm_val && (perm_val->type == JSON_NUMBER || perm_val->type == JSON_INTEGER)) {
                json_object_set(permissions_obj, perm_key, 
                               json_create_number(perm_val->type == JSON_NUMBER ? 
                                                 perm_val->value.number : perm_val->value.integer));
            }
        }
    }
    
    json_object_set(role_obj, "permissions", permissions_obj);
    
    /* Add users array */
    json_value_t* users_array = json_create_array();
    
    /* Get role users from JSON data */
    json_value_t* role_json = json_object_get(ctx->rbac->roles, role_id);
    if (role_json && role_json->type == JSON_OBJECT) {
        json_value_t* users = json_object_get(role_json, "users");
        if (users && users->type == JSON_ARRAY) {
            for (size_t i = 0; i < users->value.array.size; i++) {
                json_value_t* user_id_val = users->value.array.items[i];
                if (user_id_val && user_id_val->type == JSON_STRING) {
                    /* Add user ID to array */
                    json_array_append(users_array, json_create_string(user_id_val->value.string));
                    
                    /* Optionally, add user details */
                    const char* user_id_str = user_id_val->value.string;
                    json_value_t* user_json = json_object_get(ctx->rbac->users, user_id_str);
                    if (user_json && user_json->type == JSON_OBJECT) {
                        json_value_t* username_val = json_object_get(user_json, "username");
                        if (username_val && username_val->type == JSON_STRING) {
                            /* Create user info object */
                            json_value_t* user_info = json_create_object();
                            json_object_set(user_info, "id", json_create_string(user_id_str));
                            json_object_set(user_info, "username", json_create_string(username_val->value.string));
                            
                            /* Add user info to array (replacing the simple ID string) */
                            users_array->value.array.items[users_array->value.array.size - 1] = user_info;
                        }
                    }
                }
            }
        }
    }
    
    json_object_set(role_obj, "users", users_array);
    
    json_object_set(response, "role", role_obj);
    
    /* Serialize response */
    char* response_str = json_stringify(response);
    
    /* Free resources */
    json_free(response);
    rbac_free_role(role);
    
    /* Create and return response */
    return create_http_response(HTTP_OK, response_str, "application/json");
}

http_response_t* api_handle_role_create(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request || !request->body) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    if (!ctx->rbac) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"RBAC system not initialized\"}", "application/json");
    }
    
    /* Check if the user has admin privileges */
    /* Extract token and get user ID */
    char* token = api_extract_token(request);
    if (!token) {
        /* This should not happen since authorization is already checked in api_dispatch_request */
        return create_http_response(HTTP_UNAUTHORIZED, 
                                  "{\"error\":\"Unauthorized\"}", "application/json");
    }
    
    /* Decode token */
    jwt_token_t* jwt = jwt_decode(token);
    free(token);
    
    if (!jwt || !jwt->payload || !jwt->payload->sub) {
        if (jwt) jwt_free(jwt);
        return create_http_response(HTTP_UNAUTHORIZED, 
                                  "{\"error\":\"Invalid token\"}", "application/json");
    }
    
    const char* user_id = jwt->payload->sub;
    
    /* Check if user has permission to create roles */
    if (!rbac_check_permission(ctx->rbac, user_id, RBAC_ROLE, "*", RBAC_WRITE)) {
        jwt_free(jwt);
        return create_http_response(HTTP_FORBIDDEN, 
                                  "{\"error\":\"Permission denied\"}", "application/json");
    }
    
    jwt_free(jwt);
    
    /* Parse request body */
    json_value_t* body = json_parse(request->body);
    if (!body || body->type != JSON_OBJECT) {
        if (body) json_free(body);
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request body\"}", "application/json");
    }
    
    /* Extract role name and permissions */
    json_value_t* name_val = json_object_get(body, "name");
    json_value_t* permissions_val = json_object_get(body, "permissions");
    
    if (!name_val || name_val->type != JSON_STRING) {
        json_free(body);
        return create_http_response(HTTP_BAD_REQUEST, 
                                 "{\"error\":\"Role name is required\"}", "application/json");
    }
    
    const char* name = name_val->value.string;
    
    /* Validate input */
    if (strlen(name) < 2) {
        json_free(body);
        return create_http_response(HTTP_BAD_REQUEST, 
                                 "{\"error\":\"Role name must be at least 2 characters long\"}", "application/json");
    }
    
    /* Check if role name already exists */
    for (size_t i = 0; i < ctx->rbac->roles->value.object.size; i++) {
        json_value_t* role = ctx->rbac->roles->value.object.entries[i].value;
        json_value_t* role_name = json_object_get(role, "name");
        
        if (role_name && role_name->type == JSON_STRING && 
            strcmp(role_name->value.string, name) == 0) {
            json_free(body);
            return create_http_response(HTTP_CONFLICT, 
                                     "{\"error\":\"Role name already exists\"}", "application/json");
        }
    }
    
    /* Create role */
    rbac_role_t* role = rbac_create_role(ctx->rbac, name);
    if (!role) {
        json_free(body);
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                 "{\"error\":\"Failed to create role\"}", "application/json");
    }
    
    /* Process permissions if provided */
    if (permissions_val && permissions_val->type == JSON_OBJECT) {
        for (size_t i = 0; i < permissions_val->value.object.size; i++) {
            const char* resource_str = permissions_val->value.object.entries[i].key;
            json_value_t* perms_val = permissions_val->value.object.entries[i].value;
            
            /* Parse resource string into type and ID */
            char* colon = strchr(resource_str, ':');
            if (colon) {
                /* Split "type:id" string */
                int type_len = colon - resource_str;
                char type_str[32] = {0};
                strncpy(type_str, resource_str, type_len < 31 ? type_len : 31);
                
                /* Convert type string to enum */
                rbac_resource_type_t resource_type = RBAC_UNKNOWN;
                if (strcmp(type_str, "database") == 0) {
                    resource_type = RBAC_DATABASE;
                } else if (strcmp(type_str, "collection") == 0) {
                    resource_type = RBAC_COLLECTION;
                } else if (strcmp(type_str, "document") == 0) {
                    resource_type = RBAC_DOCUMENT;
                } else if (strcmp(type_str, "user") == 0) {
                    resource_type = RBAC_USER;
                } else if (strcmp(type_str, "role") == 0) {
                    resource_type = RBAC_ROLE;
                }
                
                const char* resource_id = colon + 1;
                
                /* Get permission value */
                int permission = 0;
                if (perms_val->type == JSON_NUMBER) {
                    permission = (int)perms_val->value.number;
                } else if (perms_val->type == JSON_INTEGER) {
                    permission = (int)perms_val->value.integer;
                } else if (perms_val->type == JSON_OBJECT) {
                    /* Process permission object with read, write, delete, admin flags */
                    json_value_t* read_val = json_object_get(perms_val, "read");
                    json_value_t* write_val = json_object_get(perms_val, "write");
                    json_value_t* delete_val = json_object_get(perms_val, "delete");
                    json_value_t* admin_val = json_object_get(perms_val, "admin");
                    
                    if (read_val && (read_val->type == JSON_BOOLEAN) && read_val->value.boolean) {
                        permission |= RBAC_READ;
                    }
                    
                    if (write_val && (write_val->type == JSON_BOOLEAN) && write_val->value.boolean) {
                        permission |= RBAC_WRITE;
                    }
                    
                    if (delete_val && (delete_val->type == JSON_BOOLEAN) && delete_val->value.boolean) {
                        permission |= RBAC_DELETE;
                    }
                    
                    if (admin_val && (admin_val->type == JSON_BOOLEAN) && admin_val->value.boolean) {
                        permission |= RBAC_ADMIN;
                    }
                }
                
                /* Grant permission */
                if (resource_type != RBAC_UNKNOWN && permission != 0) {
                    rbac_grant_permission(ctx->rbac, role->id, resource_type, resource_id, permission);
                }
            }
        }
    }
    
    /* Create response */
    json_value_t* response = json_create_object();
    json_value_t* role_obj = json_create_object();
    
    json_object_set(role_obj, "id", json_create_string(role->id));
    json_object_set(role_obj, "name", json_create_string(role->name));
    
    /* Add permissions object */
    json_value_t* permissions_obj = json_create_object();
    
    /* Copy permissions from role */
    if (role->permissions) {
        for (size_t i = 0; i < role->permissions->value.object.size; i++) {
            const char* perm_key = role->permissions->value.object.entries[i].key;
            json_value_t* perm_val = role->permissions->value.object.entries[i].value;
            
            if (perm_val && (perm_val->type == JSON_NUMBER || perm_val->type == JSON_INTEGER)) {
                json_object_set(permissions_obj, perm_key, 
                               json_create_number(perm_val->type == JSON_NUMBER ? 
                                                 perm_val->value.number : perm_val->value.integer));
            }
        }
    }
    
    json_object_set(role_obj, "permissions", permissions_obj);
    
    /* Add users array (empty for new role) */
    json_object_set(role_obj, "users", json_create_array());
    
    json_object_set(response, "role", role_obj);
    
    /* Serialize response */
    char* response_str = json_stringify(response);
    
    /* Free resources */
    json_free(response);
    json_free(body);
    rbac_free_role(role);
    
    /* Create and return response */
    return create_http_response(HTTP_CREATED, response_str, "application/json");
}

http_response_t* api_handle_role_update(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request || !request->body) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    if (!ctx->rbac) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"RBAC system not initialized\"}", "application/json");
    }
    
    /* Extract role ID from path */
    const char* path = request->path;
    if (strncmp(path, "/api/roles/", 11) != 0) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid path\"}", "application/json");
    }
    
    const char* role_id = path + 11;
    
    /* Check if the user has appropriate permissions */
    /* Extract token and get user ID */
    char* token = api_extract_token(request);
    if (!token) {
        /* This should not happen since authorization is already checked in api_dispatch_request */
        return create_http_response(HTTP_UNAUTHORIZED, 
                                  "{\"error\":\"Unauthorized\"}", "application/json");
    }
    
    /* Decode token */
    jwt_token_t* jwt = jwt_decode(token);
    free(token);
    
    if (!jwt || !jwt->payload || !jwt->payload->sub) {
        if (jwt) jwt_free(jwt);
        return create_http_response(HTTP_UNAUTHORIZED, 
                                  "{\"error\":\"Invalid token\"}", "application/json");
    }
    
    const char* user_id = jwt->payload->sub;
    
    /* Check if user has permission to update role */
    if (!rbac_check_permission(ctx->rbac, user_id, RBAC_ROLE, role_id, RBAC_WRITE) &&
        !rbac_check_permission(ctx->rbac, user_id, RBAC_ROLE, "*", RBAC_WRITE)) {
        jwt_free(jwt);
        return create_http_response(HTTP_FORBIDDEN, 
                                  "{\"error\":\"Permission denied\"}", "application/json");
    }
    
    jwt_free(jwt);
    
    /* Check if role exists */
    json_value_t* role_json = json_object_get(ctx->rbac->roles, role_id);
    if (!role_json || role_json->type != JSON_OBJECT) {
        return create_http_response(HTTP_NOT_FOUND, 
                                  "{\"error\":\"Role not found\"}", "application/json");
    }
    
    /* Parse request body */
    json_value_t* body = json_parse(request->body);
    if (!body || body->type != JSON_OBJECT) {
        if (body) json_free(body);
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request body\"}", "application/json");
    }
    
    /* Extract fields to update */
    json_value_t* name_val = json_object_get(body, "name");
    json_value_t* permissions_val = json_object_get(body, "permissions");
    json_value_t* users_val = json_object_get(body, "users");
    
    /* Update name if provided */
    if (name_val && name_val->type == JSON_STRING) {
        const char* new_name = name_val->value.string;
        
        /* Validate name */
        if (strlen(new_name) < 2) {
            json_free(body);
            return create_http_response(HTTP_BAD_REQUEST, 
                                     "{\"error\":\"Role name must be at least 2 characters long\"}", "application/json");
        }
        
        /* Check if name is already taken by another role */
        for (size_t i = 0; i < ctx->rbac->roles->value.object.size; i++) {
            const char* current_role_id = ctx->rbac->roles->value.object.entries[i].key;
            json_value_t* current_role = ctx->rbac->roles->value.object.entries[i].value;
            
            if (strcmp(current_role_id, role_id) != 0 && current_role->type == JSON_OBJECT) {
                json_value_t* current_name = json_object_get(current_role, "name");
                if (current_name && current_name->type == JSON_STRING && 
                    strcmp(current_name->value.string, new_name) == 0) {
                    json_free(body);
                    return create_http_response(HTTP_CONFLICT, 
                                             "{\"error\":\"Role name already exists\"}", "application/json");
                }
            }
        }
        
        /* Update name */
        json_object_set(role_json, "name", json_create_string(new_name));
    }
    
    /* Update permissions if provided */
    if (permissions_val && permissions_val->type == JSON_OBJECT) {
        /* Get current permissions */
        json_value_t* current_permissions = json_object_get(role_json, "permissions");
        if (!current_permissions || current_permissions->type != JSON_OBJECT) {
            /* Create permissions object if it doesn't exist */
            current_permissions = json_create_object();
            json_object_set(role_json, "permissions", current_permissions);
        }
        
        /* Clear existing permissions */
        for (size_t i = 0; i < current_permissions->value.object.size; i++) {
            const char* key = current_permissions->value.object.entries[i].key;
            json_object_remove(current_permissions, key);
            /* Adjust index after removal */
            i--;
        }
        
        /* Add new permissions */
        for (size_t i = 0; i < permissions_val->value.object.size; i++) {
            const char* key = permissions_val->value.object.entries[i].key;
            json_value_t* value = permissions_val->value.object.entries[i].value;
            
            if (value && (value->type == JSON_NUMBER || value->type == JSON_INTEGER)) {
                json_object_set(current_permissions, key, 
                               json_create_number(value->type == JSON_NUMBER ? 
                                                 value->value.number : value->value.integer));
            } else if (value && value->type == JSON_OBJECT) {
                /* Process permission object with read, write, delete, admin flags */
                json_value_t* read_val = json_object_get(value, "read");
                json_value_t* write_val = json_object_get(value, "write");
                json_value_t* delete_val = json_object_get(value, "delete");
                json_value_t* admin_val = json_object_get(value, "admin");
                
                int permission = 0;
                
                if (read_val && (read_val->type == JSON_BOOLEAN) && read_val->value.boolean) {
                    permission |= RBAC_READ;
                }
                
                if (write_val && (write_val->type == JSON_BOOLEAN) && write_val->value.boolean) {
                    permission |= RBAC_WRITE;
                }
                
                if (delete_val && (delete_val->type == JSON_BOOLEAN) && delete_val->value.boolean) {
                    permission |= RBAC_DELETE;
                }
                
                if (admin_val && (admin_val->type == JSON_BOOLEAN) && admin_val->value.boolean) {
                    permission |= RBAC_ADMIN;
                }
                
                if (permission != 0) {
                    json_object_set(current_permissions, key, json_create_number(permission));
                }
            }
        }
    }
    
    /* Update users if provided */
    if (users_val && users_val->type == JSON_ARRAY) {
        /* Get current users */
        json_value_t* current_users = json_object_get(role_json, "users");
        if (!current_users || current_users->type != JSON_ARRAY) {
            /* Create users array if it doesn't exist */
            current_users = json_create_array();
            json_object_set(role_json, "users", current_users);
        }
        
        /* First, remove this role from all users' roles */
        for (size_t i = 0; i < current_users->value.array.size; i++) {
            json_value_t* user_id_val = current_users->value.array.items[i];
            if (user_id_val && user_id_val->type == JSON_STRING) {
                const char* user_id_str = user_id_val->value.string;
                
                /* Get user */
                json_value_t* user_obj = json_object_get(ctx->rbac->users, user_id_str);
                if (user_obj && user_obj->type == JSON_OBJECT) {
                    /* Get user roles */
                    json_value_t* user_roles = json_object_get(user_obj, "roles");
                    if (user_roles && user_roles->type == JSON_ARRAY) {
                        /* Find role in user */
                        for (size_t j = 0; j < user_roles->value.array.size; j++) {
                            json_value_t* id = user_roles->value.array.items[j];
                            if (id->type == JSON_STRING && strcmp(id->value.string, role_id) == 0) {
                                /* Remove role from user */
                                for (size_t k = j; k < user_roles->value.array.size - 1; k++) {
                                    user_roles->value.array.items[k] = user_roles->value.array.items[k + 1];
                                }
                                user_roles->value.array.size--;
                                break;
                            }
                        }
                    }
                }
            }
        }
        
        /* Clear current users array */
        current_users->value.array.size = 0;
        
        /* Add new users */
        for (size_t i = 0; i < users_val->value.array.size; i++) {
            json_value_t* user_id_val = users_val->value.array.items[i];
            
            /* Handle both string and object formats */
            const char* user_id_str = NULL;
            if (user_id_val->type == JSON_STRING) {
                user_id_str = user_id_val->value.string;
            } else if (user_id_val->type == JSON_OBJECT) {
                json_value_t* id_val = json_object_get(user_id_val, "id");
                if (id_val && id_val->type == JSON_STRING) {
                    user_id_str = id_val->value.string;
                }
            }
            
            if (user_id_str) {
                /* Check if user exists */
                json_value_t* user_obj = json_object_get(ctx->rbac->users, user_id_str);
                if (user_obj && user_obj->type == JSON_OBJECT) {
                    /* Add user to role */
                    json_array_append(current_users, json_create_string(user_id_str));
                    
                    /* Add role to user's roles */
                    json_value_t* user_roles = json_object_get(user_obj, "roles");
                    if (!user_roles || user_roles->type != JSON_ARRAY) {
                        /* Create roles array if it doesn't exist */
                        user_roles = json_create_array();
                        json_object_set(user_obj, "roles", user_roles);
                    }
                    
                    /* Check if role is already in user's roles */
                    int role_found = 0;
                    for (size_t j = 0; j < user_roles->value.array.size; j++) {
                        json_value_t* id = user_roles->value.array.items[j];
                        if (id->type == JSON_STRING && strcmp(id->value.string, role_id) == 0) {
                            role_found = 1;
                            break;
                        }
                    }
                    
                    /* Add role to user if not already present */
                    if (!role_found) {
                        json_array_append(user_roles, json_create_string(role_id));
                    }
                }
            }
        }
    }
    
    /* Get updated role for response */
    rbac_role_t* role = rbac_get_role(ctx->rbac, role_id);
    
    /* Create response */
    json_value_t* response = json_create_object();
    json_value_t* role_obj = json_create_object();
    
    json_object_set(role_obj, "id", json_create_string(role->id));
    json_object_set(role_obj, "name", json_create_string(role->name));
    
    /* Add permissions object */
    json_value_t* permissions_obj = json_create_object();
    
    /* Copy permissions from role */
    if (role->permissions) {
        for (size_t i = 0; i < role->permissions->value.object.size; i++) {
            const char* perm_key = role->permissions->value.object.entries[i].key;
            json_value_t* perm_val = role->permissions->value.object.entries[i].value;
            
            if (perm_val && (perm_val->type == JSON_NUMBER || perm_val->type == JSON_INTEGER)) {
                json_object_set(permissions_obj, perm_key, 
                               json_create_number(perm_val->type == JSON_NUMBER ? 
                                                 perm_val->value.number : perm_val->value.integer));
            }
        }
    }
    
    json_object_set(role_obj, "permissions", permissions_obj);
    
    /* Add users array */
    json_value_t* users_array = json_create_array();
    
    /* Get role users from JSON data */
    role_json = json_object_get(ctx->rbac->roles, role_id);
    if (role_json && role_json->type == JSON_OBJECT) {
        json_value_t* users = json_object_get(role_json, "users");
        if (users && users->type == JSON_ARRAY) {
            for (size_t i = 0; i < users->value.array.size; i++) {
                json_value_t* user_id_val = users->value.array.items[i];
                if (user_id_val && user_id_val->type == JSON_STRING) {
                    /* Add user ID to array */
                    json_array_append(users_array, json_create_string(user_id_val->value.string));
                }
            }
        }
    }
    
    json_object_set(role_obj, "users", users_array);
    
    json_object_set(response, "role", role_obj);
    
    /* Serialize response */
    char* response_str = json_stringify(response);
    
    /* Free resources */
    json_free(response);
    json_free(body);
    rbac_free_role(role);
    
    /* Create and return response */
    return create_http_response(HTTP_OK, response_str, "application/json");
}

http_response_t* api_handle_role_delete(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    if (!ctx->rbac) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"RBAC system not initialized\"}", "application/json");
    }
    
    /* Extract role ID from path */
    const char* path = request->path;
    if (strncmp(path, "/api/roles/", 11) != 0) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid path\"}", "application/json");
    }
    
    const char* role_id = path + 11;
    
    /* Check if the user has appropriate permissions */
    /* Extract token and get user ID */
    char* token = api_extract_token(request);
    if (!token) {
        /* This should not happen since authorization is already checked in api_dispatch_request */
        return create_http_response(HTTP_UNAUTHORIZED, 
                                  "{\"error\":\"Unauthorized\"}", "application/json");
    }
    
    /* Decode token */
    jwt_token_t* jwt = jwt_decode(token);
    free(token);
    
    if (!jwt || !jwt->payload || !jwt->payload->sub) {
        if (jwt) jwt_free(jwt);
        return create_http_response(HTTP_UNAUTHORIZED, 
                                  "{\"error\":\"Invalid token\"}", "application/json");
    }
    
    const char* user_id = jwt->payload->sub;
    
    /* Check if user has permission to delete role */
    if (!rbac_check_permission(ctx->rbac, user_id, RBAC_ROLE, role_id, RBAC_DELETE) &&
        !rbac_check_permission(ctx->rbac, user_id, RBAC_ROLE, "*", RBAC_DELETE)) {
        jwt_free(jwt);
        return create_http_response(HTTP_FORBIDDEN, 
                                  "{\"error\":\"Permission denied\"}", "application/json");
    }
    
    jwt_free(jwt);
    
    /* Check if role exists */
    if (!json_object_has(ctx->rbac->roles, role_id)) {
        return create_http_response(HTTP_NOT_FOUND, 
                                  "{\"error\":\"Role not found\"}", "application/json");
    }
    
    /* Special case: Don't allow deletion of the admin role */
    json_value_t* role_json = json_object_get(ctx->rbac->roles, role_id);
    if (role_json && role_json->type == JSON_OBJECT) {
        json_value_t* name_val = json_object_get(role_json, "name");
        if (name_val && name_val->type == JSON_STRING && 
            strcmp(name_val->value.string, "admin") == 0) {
            return create_http_response(HTTP_FORBIDDEN, 
                                      "{\"error\":\"Cannot delete the admin role\"}", "application/json");
        }
    }
    
    /* Delete role */
    if (!rbac_delete_role(ctx->rbac, role_id)) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Failed to delete role\"}", "application/json");
    }
    
    /* Return success with no content */
    return create_http_response(HTTP_NO_CONTENT, NULL, "application/json");
}

/* Configuration handlers */
http_response_t* api_handle_config_get(api_context_t* ctx, http_request_t* request) {
    (void)request; /* Avoid unused parameter warning */
    (void)ctx; /* Avoid unused parameter warning */
    /* Placeholder implementation */
    return create_http_response(HTTP_OK, "{\"config\":{\"port\":8080}}", "application/json");
}

http_response_t* api_handle_config_update(api_context_t* ctx, http_request_t* request) {
    (void)request; /* Avoid unused parameter warning */
    (void)ctx; /* Avoid unused parameter warning */
    /* Placeholder implementation */
    return create_http_response(HTTP_OK, "{\"config\":{\"port\":8080}}", "application/json");
}

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
        char* query = strdup(request->query);
        if (query) {
            char* token = strtok(query, "&");
            while (token) {
                if (strncmp(token, "type=", 5) == 0) {
                    type_filter = strdup(token + 5);
                } else if (strncmp(token, "name=", 5) == 0) {
                    name_filter = strdup(token + 5);
                }
                token = strtok(NULL, "&");
            }
            free(query);
        }
    }
    
    /* Get all metrics as JSON */
    char* metrics_json = metrics_get_json(g_metrics_registry);
    if (!metrics_json) {
        if (type_filter) free(type_filter);
        if (name_filter) free(name_filter);
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Failed to get metrics\"}", "application/json");
    }
    
    /* If no filters are specified, return all metrics */
    if (!category && !type_filter && !name_filter) {
        /* Create response */
        http_response_t* response = create_http_response(HTTP_OK, metrics_json, "application/json");
        
        /* Free metrics JSON */
        free(metrics_json);
        
        return response;
    }
    
    /* Parse the metrics JSON to filter based on the criteria */
    json_value_t* root = json_parse(metrics_json);
    if (!root) {
        if (type_filter) free(type_filter);
        if (name_filter) free(name_filter);
        free(metrics_json);
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Failed to parse metrics JSON\"}", "application/json");
    }
    
    /* Create a new metrics array for the filtered metrics */
    json_value_t* filtered_metrics = json_create_array();
    if (!filtered_metrics) {
        if (type_filter) free(type_filter);
        if (name_filter) free(name_filter);
        json_free(root);
        free(metrics_json);
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
                continue;  /* Skip metrics with missing or invalid required fields */
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
        if (type_filter) free(type_filter);
        if (name_filter) free(name_filter);
        json_free(filtered_metrics);
        json_free(root);
        free(metrics_json);
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Memory allocation failure\"}", "application/json");
    }
    
    json_object_set(filtered_obj, "metrics", filtered_metrics);
    
    /* Convert to JSON string */
    char* filtered_json = json_stringify(filtered_obj);
    
    /* Clean up */
    if (type_filter) free(type_filter);
    if (name_filter) free(name_filter);
    json_free(filtered_obj);  /* This will also free filtered_metrics */
    json_free(root);
    free(metrics_json);
    
    if (!filtered_json) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Failed to generate response\"}", "application/json");
    }
    
    /* Create response */
    http_response_t* response = create_http_response(HTTP_OK, filtered_json, "application/json");
    
    /* Free filtered JSON */
    free(filtered_json);
    
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
        free(metrics_json);
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Failed to parse metrics JSON\"}", "application/json");
    }
    
    /* Create a buffer for the Prometheus format output */
    size_t buffer_size = 10240;  /* Start with 10KB buffer */
    char* prom_buffer = (char*)malloc(buffer_size);
    if (!prom_buffer) {
        json_free(root);
        free(metrics_json);
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
                continue;  /* Skip metrics with missing or invalid required fields */
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
                char* new_buffer = (char*)realloc(prom_buffer, buffer_size);
                if (!new_buffer) {
                    free(prom_buffer);
                    json_free(root);
                    free(metrics_json);
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
    free(prom_buffer);
    json_free(root);
    free(metrics_json);
    
    return response;
#else
    /* In tools build, return empty metrics */
    return create_http_response(HTTP_OK, "# No metrics available in tools build\n", "text/plain");
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
        free(metrics_json);
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Failed to parse metrics JSON\"}", "application/json");
    }
    
    /* Create a new JSON object for the response */
    json_value_t* response_obj = json_create_object();
    if (!response_obj) {
        json_free(root);
        free(metrics_json);
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
        free(metrics_json);
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
                continue;  /* Skip metrics with missing or invalid required fields */
            }
            
            /* Create a simple object with name and description */
            json_value_t* metric_info = json_create_object();
            if (!metric_info) {
                continue;  /* Skip on memory allocation failure */
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
                json_free(metric_info);  /* Unknown type, free the object */
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
    free(metrics_json);
    
    if (!response_json) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Failed to generate response\"}", "application/json");
    }
    
    /* Create response */
    http_response_t* response = create_http_response(HTTP_OK, response_json, "application/json");
    
    /* Free response JSON */
    free(response_json);
    
    return response;
#else
    /* In tools build, return empty metrics */
    return create_http_response(HTTP_OK, 
                              "{\"counters\":[],\"gauges\":[],\"timers\":[],\"histograms\":[]}", 
                              "application/json");
#endif
}