#include "src/include/api/api.h"
#include "src/include/core/server.h"
#include "src/include/database/database.h"
#include "src/include/rbac/rbac.h"
#include "src/include/rbac/jwt.h"
#include "src/include/utils/metrics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Global metrics registry declaration */
#ifndef TOOLS_BUILD
extern metrics_registry_t* g_metrics_registry;
#else
metrics_registry_t* g_metrics_registry = NULL;
#endif

/* API routes */
static api_route_t routes[] = {
    /* Authentication routes */
    {"/api/auth/login", HTTP_POST, api_handle_login, 0},
    {"/api/auth/register", HTTP_POST, api_handle_register, 0},
    
    /* Collection routes */
    {"/api/collections", HTTP_GET, api_handle_collections_list, 1},
    {"/api/collections", HTTP_POST, api_handle_collection_create, 1},
    {"/api/collections/", HTTP_DELETE, api_handle_collection_drop, 1},
    
    /* Document routes */
    {"/api/collections/", HTTP_GET, api_handle_documents_query, 1},
    {"/api/collections/", HTTP_POST, api_handle_document_create, 1},
    {"/api/collections/", HTTP_GET, api_handle_document_get, 1},
    {"/api/collections/", HTTP_PUT, api_handle_document_update, 1},
    {"/api/collections/", HTTP_DELETE, api_handle_document_delete, 1},
    
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
    {"/api/metrics", HTTP_GET, api_handle_metrics_get, 1},
    
    /* System info routes */
    {"/api/system/info", HTTP_GET, api_handle_system_info, 1},
    
    /* Data visualization routes */
    {"/api/visualization/collection-stats", HTTP_GET, api_handle_visualization_collection_stats, 1},
    {"/api/visualization/document-types", HTTP_GET, api_handle_visualization_document_types, 1},
    {"/api/visualization/field-distribution", HTTP_GET, api_handle_visualization_field_distribution, 1},
    
    /* Transaction visualization routes - commented out until implemented
    {"/api/visualization/transaction-history", HTTP_GET, api_handle_visualization_transaction_history, 1},
    {"/api/visualization/transaction-metrics", HTTP_GET, api_handle_visualization_transaction_metrics, 1},
    {"/api/visualization/transaction-relationships", HTTP_GET, api_handle_visualization_transaction_relationships, 1},
    */
    
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
    {"/metrics", HTTP_GET, api_handle_metrics, 0},
    {"/metrics/available", HTTP_GET, api_handle_metrics_available, 0},
    
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
    
    /* Transaction status and metrics routes - Not implemented yet - will be added in future updates
    {"/api/transactions/metrics", HTTP_GET, api_handle_transaction_metrics, 1},
    {"/api/transactions/check-deadlocks", HTTP_POST, api_handle_transaction_check_deadlocks, 1},
    {"/api/transactions/", HTTP_GET, api_handle_transaction_status, 1},
    */
    
    /* Transaction log and audit trail routes - Not implemented yet - will be added in future updates 
    {"/api/transactions/logs", HTTP_GET, api_handle_transaction_logs, 1},
    {"/api/transactions/logs/configure", HTTP_POST, api_handle_transaction_logs_configure, 1},
    {"/api/transactions/logs/archive", HTTP_POST, api_handle_transaction_logs_archive, 1},
    {"/api/transactions/logs/report", HTTP_GET, api_handle_transaction_logs_report, 1},
    {"/api/transactions/logs/document-history", HTTP_GET, api_handle_transaction_logs_document_history, 1},
    */
    
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
        return NULL;
    }
    
    api_context_t* ctx = (api_context_t*)malloc(sizeof(api_context_t));
    if (!ctx) {
        return NULL;
    }
    
    ctx->db = db;
    ctx->rbac = rbac;
    ctx->jwt_secret = jwt_secret;
    
    /* Initialize transaction manager with capacity for 100 concurrent transactions */
    ctx->transaction_manager = transaction_manager_create(db, 100);
    if (!ctx->transaction_manager) {
        free(ctx);
        return NULL;
    }
    
    return ctx;
}

/* Free API context */
void api_free_context(api_context_t* ctx) {
    if (ctx) {
        if (ctx->transaction_manager) {
            transaction_manager_free(ctx->transaction_manager);
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
        return 0;
    }
    
    /* Extract token */
    char* token = api_extract_token(request);
    if (!token) {
        return 0;
    }
    
    /* Verify token */
    int result = jwt_verify(token, ctx->jwt_secret);
    
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
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Internal server error\"}", "application/json");
    }
    
    /* Find matching route */
    for (int i = 0; routes[i].path != NULL; i++) {
        if (route_matches(routes[i].path, request->path) && routes[i].method == request->method) {
            /* Check if route requires authentication */
            if (routes[i].requires_auth && !api_authenticate_request(ctx, request)) {
                return create_http_response(HTTP_UNAUTHORIZED, 
                                          "{\"error\":\"Unauthorized\"}", "application/json");
            }
            
            /* Call handler */
            return routes[i].handler(ctx, request);
        }
    }
    
    /* No matching route */
    return create_http_response(HTTP_NOT_FOUND, 
                              "{\"error\":\"Not found\"}", "application/json");
}

/* Authentication handlers */

/* Login handler */
http_response_t* api_handle_login(api_context_t* ctx, http_request_t* request) {
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
    
    /* Create JWT token */
    jwt_token_t* token = jwt_create(ctx->jwt_secret);
    if (!token) {
        json_free(body);
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Failed to create token\"}", "application/json");
    }
    
    /* Set token claims */
    jwt_set_subject(token, user->id);
    jwt_set_issuer(token, "jsondb");
    
    /* Set expiration (24 hours) */
    jwt_set_expiration(token, time(NULL) + 86400);
    
    /* Add username claim */
    jwt_add_claim(token, "username", json_create_string(user->username));
    
    /* Encode token */
    char* jwt_str = jwt_encode(token, ctx->jwt_secret);
    jwt_free(token);
    
    if (!jwt_str) {
        json_free(body);
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Failed to encode token\"}", "application/json");
    }
    
    /* Create response */
    json_value_t* response = json_create_object();
    json_object_set(response, "token", json_create_string(jwt_str));
    json_object_set(response, "user_id", json_create_string(user->id));
    json_object_set(response, "username", json_create_string(user->username));
    
    char* response_str = json_stringify(response);
    json_free(response);
    free(jwt_str);
    json_free(body);
    
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
    (void)request; /* Avoid unused parameter warning */
    (void)ctx; /* Avoid unused parameter warning */
    /* Placeholder implementation */
    return create_http_response(HTTP_OK, "{\"users\":[]}", "application/json");
}

http_response_t* api_handle_user_get(api_context_t* ctx, http_request_t* request) {
    (void)request; /* Avoid unused parameter warning */
    (void)ctx; /* Avoid unused parameter warning */
    /* Placeholder implementation */
    return create_http_response(HTTP_OK, "{\"user\":{\"id\":\"1\",\"username\":\"admin\"}}", "application/json");
}

http_response_t* api_handle_user_create(api_context_t* ctx, http_request_t* request) {
    (void)request; /* Avoid unused parameter warning */
    (void)ctx; /* Avoid unused parameter warning */
    /* Placeholder implementation */
    return create_http_response(HTTP_CREATED, "{\"user\":{\"id\":\"1\",\"username\":\"admin\"}}", "application/json");
}

http_response_t* api_handle_user_update(api_context_t* ctx, http_request_t* request) {
    (void)request; /* Avoid unused parameter warning */
    (void)ctx; /* Avoid unused parameter warning */
    /* Placeholder implementation */
    return create_http_response(HTTP_OK, "{\"user\":{\"id\":\"1\",\"username\":\"admin\"}}", "application/json");
}

http_response_t* api_handle_user_delete(api_context_t* ctx, http_request_t* request) {
    (void)request; /* Avoid unused parameter warning */
    (void)ctx; /* Avoid unused parameter warning */
    /* Placeholder implementation */
    return create_http_response(HTTP_NO_CONTENT, NULL, "application/json");
}

http_response_t* api_handle_roles_list(api_context_t* ctx, http_request_t* request) {
    (void)request; /* Avoid unused parameter warning */
    (void)ctx; /* Avoid unused parameter warning */
    /* Placeholder implementation */
    return create_http_response(HTTP_OK, "{\"roles\":[]}", "application/json");
}

http_response_t* api_handle_role_get(api_context_t* ctx, http_request_t* request) {
    (void)request; /* Avoid unused parameter warning */
    (void)ctx; /* Avoid unused parameter warning */
    /* Placeholder implementation */
    return create_http_response(HTTP_OK, "{\"role\":{\"id\":\"1\",\"name\":\"admin\"}}", "application/json");
}

http_response_t* api_handle_role_create(api_context_t* ctx, http_request_t* request) {
    (void)request; /* Avoid unused parameter warning */
    (void)ctx; /* Avoid unused parameter warning */
    /* Placeholder implementation */
    return create_http_response(HTTP_CREATED, "{\"role\":{\"id\":\"1\",\"name\":\"admin\"}}", "application/json");
}

http_response_t* api_handle_role_update(api_context_t* ctx, http_request_t* request) {
    (void)request; /* Avoid unused parameter warning */
    (void)ctx; /* Avoid unused parameter warning */
    /* Placeholder implementation */
    return create_http_response(HTTP_OK, "{\"role\":{\"id\":\"1\",\"name\":\"admin\"}}", "application/json");
}

http_response_t* api_handle_role_delete(api_context_t* ctx, http_request_t* request) {
    (void)request; /* Avoid unused parameter warning */
    (void)ctx; /* Avoid unused parameter warning */
    /* Placeholder implementation */
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

/* Metrics handler */
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
    
    /* Get metrics JSON representation */
    char* metrics_json = metrics_get_json(g_metrics_registry);
    if (!metrics_json) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Failed to get metrics\"}", "application/json");
    }
    
    /* Create response */
    http_response_t* response = create_http_response(HTTP_OK, metrics_json, "application/json");
    
    /* Free metrics JSON */
    free(metrics_json);
    
    return response;
#else
    /* In tools build, return empty metrics */
    return create_http_response(HTTP_OK, "{\"metrics\":[]}", "application/json");
#endif
}