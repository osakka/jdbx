#include "api/api.h"
#include "api/rbac_api.h"
#include "rbac/rbac.h"
#include "rbac/rbac_db.h"
#include "rbac/rbac_enhanced.h"
#include "utils/json.h"
#include "utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Helper function to create a JSON error response */
static http_response_t* create_error_response(const char* message, int status_code) {
    json_value_t* error = json_create_object();
    json_object_set(error, "error", json_create_string(message));
    
    char* error_str = json_stringify(error);
    http_response_t* response = (http_response_t*)malloc(sizeof(http_response_t));
    if (!response) {
        free(error_str);
        json_free(error);
        return NULL;
    }
    
    /* Initialize response */
    response->status = (http_status_t)status_code;
    response->body = error_str;
    response->content_type = strdup("application/json");
    response->content_length = strlen(error_str);
    response->headers = NULL;
    response->num_headers = 0;
    
    json_free(error);
    
    return response;
}

/* Helper function to extract user ID from request */
static const char* get_request_user_id(http_request_t* request) {
    /* In a real implementation, this would extract the user ID from the JWT */
    /* For now, we'll use a dummy ID */
    return "admin";
}

/**
 * Handle get users request
 * 
 * GET /api/rbac/users
 */
http_response_t* api_handle_rbac_get_users(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !ctx->db || !ctx->rbac) {
        return create_error_response("RBAC not initialized", 500);
    }
    
    /* Check if user has admin permission */
    const char* user_id = get_request_user_id(request);
    if (!user_id || !rbac_db_check_permission(ctx->db, user_id, RBAC_USER, "*", RBAC_ADMIN)) {
        return create_error_response("Unauthorized", 403);
    }
    
    /* Query all users */
    json_value_t* query = json_create_object();
    json_value_t* result = db_query_documents(ctx->db, RBAC_USERS_COLLECTION, query);
    json_free(query);
    
    if (!result || result->type != JSON_ARRAY) {
        if (result) json_free(result);
        return create_error_response("Failed to query users", 500);
    }
    
    /* Create sanitized response (remove password hashes) */
    json_value_t* users = json_create_array();
    for (size_t i = 0; i < result->value.array.size; i++) {
        json_value_t* user = result->value.array.items[i];
        if (user->type == JSON_OBJECT) {
            json_value_t* sanitized = json_create_object();
            
            /* Copy id and username */
            json_value_t* id = json_object_get(user, "id");
            json_value_t* username = json_object_get(user, "username");
            json_value_t* roles = json_object_get(user, "roles");
            
            if (id && id->type == JSON_STRING) {
                json_object_set(sanitized, "id", json_create_string(id->value.string));
            }
            
            if (username && username->type == JSON_STRING) {
                json_object_set(sanitized, "username", json_create_string(username->value.string));
            }
            
            if (roles && roles->type == JSON_ARRAY) {
                json_value_t* roles_copy = json_create_array();
                for (size_t j = 0; j < roles->value.array.size; j++) {
                    json_value_t* role = roles->value.array.items[j];
                    if (role->type == JSON_STRING) {
                        json_array_append(roles_copy, json_create_string(role->value.string));
                    }
                }
                json_object_set(sanitized, "roles", roles_copy);
            } else {
                json_object_set(sanitized, "roles", json_create_array());
            }
            
            json_array_append(users, sanitized);
        }
    }
    
    json_free(result);
    
    /* Create response */
    char* users_str = json_stringify(users);
    http_response_t* response = (http_response_t*)malloc(sizeof(http_response_t));
    if (!response) {
        free(users_str);
        json_free(users);
        return create_error_response("Memory allocation failed", 500);
    }
    
    response->status = HTTP_OK;
    response->body = users_str;
    response->content_type = strdup("application/json");
    response->content_length = strlen(users_str);
    response->headers = NULL;
    response->num_headers = 0;
    
    json_free(users);
    
    return response;
}

/* Register RBAC API routes */
int rbac_api_register_routes(api_route_t* api_routes, int num_routes, database_t* db, 
                            rbac_system_t* rbac, const char* rbac_file_path) {
    /* Register routes - just one for now to simplify build fixes */
    api_routes[num_routes++] = (api_route_t){"/api/rbac/users", HTTP_GET, api_handle_rbac_get_users, 1};
    
    return num_routes;
}