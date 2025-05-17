#include "api/api.h"
#include "rbac/rbac.h"
#include "rbac/rbac_db.h"
#include "rbac/rbac_enhanced.h"
#include "utils/json.h"
#include "utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * API context extension for RBAC
 */
typedef struct {
    database_t* db;
    rbac_system_t* rbac;
    const char* rbac_file_path;
} rbac_api_context_t;

/**
 * Helper function to get RBAC API context from API context
 */
static rbac_api_context_t* get_rbac_api_context(api_context_t* ctx) {
    if (!ctx || !ctx->user_data) {
        return NULL;
    }
    
    return (rbac_api_context_t*)ctx->user_data;
}

/**
 * Helper function to create a JSON error response
 */
static http_response_t* create_error_response(const char* message, int status_code) {
    json_value_t* error = json_create_object();
    json_object_set(error, "error", json_create_string(message));
    
    char* error_str = json_stringify(error);
    http_response_t* response = http_response_create(status_code, error_str);
    free(error_str);
    json_free(error);
    
    http_response_add_header(response, "Content-Type", "application/json");
    
    return response;
}

/**
 * Handle get users request
 * 
 * GET /api/rbac/users
 */
http_response_t* api_handle_rbac_get_users(api_context_t* ctx, http_request_t* request) {
    rbac_api_context_t* rbac_ctx = get_rbac_api_context(ctx);
    if (!rbac_ctx || !rbac_ctx->db || !rbac_ctx->rbac) {
        return create_error_response("RBAC not initialized", 500);
    }
    
    /* Check if user has admin permission */
    const char* user_id = http_request_get_user_id(request);
    if (!user_id || !rbac_db_check_permission(rbac_ctx->db, user_id, RBAC_USER, "*", RBAC_ADMIN)) {
        return create_error_response("Unauthorized", 403);
    }
    
    /* Query all users */
    json_value_t* query = json_create_object();
    json_value_t* result = db_query_documents(rbac_ctx->db, RBAC_USERS_COLLECTION, query);
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
    http_response_t* response = http_response_create(200, users_str);
    free(users_str);
    json_free(users);
    
    http_response_add_header(response, "Content-Type", "application/json");
    
    return response;
}

/**
 * Handle get user request
 * 
 * GET /api/rbac/users/:id
 */
http_response_t* api_handle_rbac_get_user(api_context_t* ctx, http_request_t* request) {
    rbac_api_context_t* rbac_ctx = get_rbac_api_context(ctx);
    if (!rbac_ctx || !rbac_ctx->db || !rbac_ctx->rbac) {
        return create_error_response("RBAC not initialized", 500);
    }
    
    /* Check if user has admin permission or is requesting their own info */
    const char* requester_id = http_request_get_user_id(request);
    if (!requester_id) {
        return create_error_response("Unauthorized", 403);
    }
    
    /* Get user ID from URL */
    const char* user_id = http_request_get_param(request, "id");
    if (!user_id) {
        return create_error_response("User ID not specified", 400);
    }
    
    /* Check if requester has permission */
    int is_self = strcmp(requester_id, user_id) == 0;
    int is_admin = rbac_db_check_permission(rbac_ctx->db, requester_id, RBAC_USER, "*", RBAC_ADMIN);
    
    if (!is_self && !is_admin) {
        return create_error_response("Unauthorized", 403);
    }
    
    /* Get user document */
    json_value_t* user_doc = db_get_document(rbac_ctx->db, RBAC_USERS_COLLECTION, user_id);
    if (!user_doc) {
        return create_error_response("User not found", 404);
    }
    
    /* Create sanitized response (remove password hash) */
    json_value_t* sanitized = json_create_object();
    
    /* Copy id and username */
    json_value_t* id = json_object_get(user_doc, "id");
    json_value_t* username = json_object_get(user_doc, "username");
    json_value_t* roles = json_object_get(user_doc, "roles");
    
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
    
    json_free(user_doc);
    
    /* Create response */
    char* user_str = json_stringify(sanitized);
    http_response_t* response = http_response_create(200, user_str);
    free(user_str);
    json_free(sanitized);
    
    http_response_add_header(response, "Content-Type", "application/json");
    
    return response;
}

/**
 * Handle create user request
 * 
 * POST /api/rbac/users
 * 
 * Body: { "username": "...", "password": "..." }
 */
http_response_t* api_handle_rbac_create_user(api_context_t* ctx, http_request_t* request) {
    rbac_api_context_t* rbac_ctx = get_rbac_api_context(ctx);
    if (!rbac_ctx || !rbac_ctx->db || !rbac_ctx->rbac) {
        return create_error_response("RBAC not initialized", 500);
    }
    
    /* Check if user has admin permission */
    const char* user_id = http_request_get_user_id(request);
    if (!user_id || !rbac_db_check_permission(rbac_ctx->db, user_id, RBAC_USER, "*", RBAC_ADMIN)) {
        return create_error_response("Unauthorized", 403);
    }
    
    /* Parse request body */
    const char* body = http_request_get_body(request);
    if (!body) {
        return create_error_response("Request body is empty", 400);
    }
    
    json_value_t* request_json = json_parse(body);
    if (!request_json || request_json->type != JSON_OBJECT) {
        if (request_json) json_free(request_json);
        return create_error_response("Invalid JSON body", 400);
    }
    
    /* Get username and password */
    json_value_t* username_val = json_object_get(request_json, "username");
    json_value_t* password_val = json_object_get(request_json, "password");
    
    if (!username_val || username_val->type != JSON_STRING ||
        !password_val || password_val->type != JSON_STRING) {
        json_free(request_json);
        return create_error_response("Username and password are required", 400);
    }
    
    const char* username = username_val->value.string;
    const char* password = password_val->value.string;
    
    /* Create user */
    rbac_user_t* user = rbac_db_create_user(rbac_ctx->db, username, password);
    json_free(request_json);
    
    if (!user) {
        return create_error_response("Failed to create user", 500);
    }
    
    /* Create response */
    json_value_t* response_json = json_create_object();
    json_object_set(response_json, "id", json_create_string(user->id));
    json_object_set(response_json, "username", json_create_string(user->username));
    json_object_set(response_json, "roles", json_create_array());
    
    char* response_str = json_stringify(response_json);
    http_response_t* response = http_response_create(201, response_str);
    free(response_str);
    json_free(response_json);
    
    /* Free user */
    rbac_free_user(user);
    
    http_response_add_header(response, "Content-Type", "application/json");
    
    return response;
}

/**
 * Handle delete user request
 * 
 * DELETE /api/rbac/users/:id
 */
http_response_t* api_handle_rbac_delete_user(api_context_t* ctx, http_request_t* request) {
    rbac_api_context_t* rbac_ctx = get_rbac_api_context(ctx);
    if (!rbac_ctx || !rbac_ctx->db || !rbac_ctx->rbac) {
        return create_error_response("RBAC not initialized", 500);
    }
    
    /* Check if user has admin permission */
    const char* requester_id = http_request_get_user_id(request);
    if (!requester_id || !rbac_db_check_permission(rbac_ctx->db, requester_id, RBAC_USER, "*", RBAC_ADMIN)) {
        return create_error_response("Unauthorized", 403);
    }
    
    /* Get user ID from URL */
    const char* user_id = http_request_get_param(request, "id");
    if (!user_id) {
        return create_error_response("User ID not specified", 400);
    }
    
    /* Prevent deleting own account */
    if (strcmp(requester_id, user_id) == 0) {
        return create_error_response("Cannot delete your own account", 400);
    }
    
    /* Delete user */
    int result = rbac_db_delete_user(rbac_ctx->db, user_id);
    if (!result) {
        return create_error_response("Failed to delete user", 500);
    }
    
    /* Create response */
    json_value_t* response_json = json_create_object();
    json_object_set(response_json, "success", json_create_bool(1));
    
    char* response_str = json_stringify(response_json);
    http_response_t* response = http_response_create(200, response_str);
    free(response_str);
    json_free(response_json);
    
    http_response_add_header(response, "Content-Type", "application/json");
    
    return response;
}

/**
 * Handle update user request
 * 
 * PUT /api/rbac/users/:id
 * 
 * Body: { "username": "...", "password": "..." }
 */
http_response_t* api_handle_rbac_update_user(api_context_t* ctx, http_request_t* request) {
    rbac_api_context_t* rbac_ctx = get_rbac_api_context(ctx);
    if (!rbac_ctx || !rbac_ctx->db || !rbac_ctx->rbac) {
        return create_error_response("RBAC not initialized", 500);
    }
    
    /* Check if user has admin permission or is updating their own info */
    const char* requester_id = http_request_get_user_id(request);
    if (!requester_id) {
        return create_error_response("Unauthorized", 403);
    }
    
    /* Get user ID from URL */
    const char* user_id = http_request_get_param(request, "id");
    if (!user_id) {
        return create_error_response("User ID not specified", 400);
    }
    
    /* Check if requester has permission */
    int is_self = strcmp(requester_id, user_id) == 0;
    int is_admin = rbac_db_check_permission(rbac_ctx->db, requester_id, RBAC_USER, "*", RBAC_ADMIN);
    
    if (!is_self && !is_admin) {
        return create_error_response("Unauthorized", 403);
    }
    
    /* Parse request body */
    const char* body = http_request_get_body(request);
    if (!body) {
        return create_error_response("Request body is empty", 400);
    }
    
    json_value_t* request_json = json_parse(body);
    if (!request_json || request_json->type != JSON_OBJECT) {
        if (request_json) json_free(request_json);
        return create_error_response("Invalid JSON body", 400);
    }
    
    /* Get user document */
    json_value_t* user_doc = db_get_document(rbac_ctx->db, RBAC_USERS_COLLECTION, user_id);
    if (!user_doc) {
        json_free(request_json);
        return create_error_response("User not found", 404);
    }
    
    /* Update fields */
    int updated = 0;
    
    /* Update username if provided and user is admin */
    json_value_t* username_val = json_object_get(request_json, "username");
    if (username_val && username_val->type == JSON_STRING && is_admin) {
        json_object_set(user_doc, "username", json_create_string(username_val->value.string));
        updated = 1;
    }
    
    /* Update password if provided */
    json_value_t* password_val = json_object_get(request_json, "password");
    if (password_val && password_val->type == JSON_STRING) {
        char* password_hash = hash_password(password_val->value.string);
        if (password_hash) {
            json_object_set(user_doc, "password_hash", json_create_string(password_hash));
            free(password_hash);
            updated = 1;
        }
    }
    
    json_free(request_json);
    
    if (!updated) {
        json_free(user_doc);
        return create_error_response("No fields to update", 400);
    }
    
    /* Update user document */
    json_value_t* result = db_update_document(rbac_ctx->db, RBAC_USERS_COLLECTION, user_id, user_doc);
    if (!result) {
        json_free(user_doc);
        return create_error_response("Failed to update user", 500);
    }
    
    json_free(result);
    
    /* Create sanitized response */
    json_value_t* sanitized = json_create_object();
    
    /* Copy id and username */
    json_value_t* id = json_object_get(user_doc, "id");
    json_value_t* username = json_object_get(user_doc, "username");
    json_value_t* roles = json_object_get(user_doc, "roles");
    
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
    
    json_free(user_doc);
    
    /* Create response */
    char* user_str = json_stringify(sanitized);
    http_response_t* response = http_response_create(200, user_str);
    free(user_str);
    json_free(sanitized);
    
    http_response_add_header(response, "Content-Type", "application/json");
    
    return response;
}

/**
 * Handle get roles request
 * 
 * GET /api/rbac/roles
 */
http_response_t* api_handle_rbac_get_roles(api_context_t* ctx, http_request_t* request) {
    rbac_api_context_t* rbac_ctx = get_rbac_api_context(ctx);
    if (!rbac_ctx || !rbac_ctx->db || !rbac_ctx->rbac) {
        return create_error_response("RBAC not initialized", 500);
    }
    
    /* Check if user has admin permission */
    const char* user_id = http_request_get_user_id(request);
    if (!user_id || !rbac_db_check_permission(rbac_ctx->db, user_id, RBAC_ROLE, "*", RBAC_READ)) {
        return create_error_response("Unauthorized", 403);
    }
    
    /* Query all roles */
    json_value_t* query = json_create_object();
    json_value_t* result = db_query_documents(rbac_ctx->db, RBAC_ROLES_COLLECTION, query);
    json_free(query);
    
    if (!result || result->type != JSON_ARRAY) {
        if (result) json_free(result);
        return create_error_response("Failed to query roles", 500);
    }
    
    /* Create response */
    char* roles_str = json_stringify(result);
    http_response_t* response = http_response_create(200, roles_str);
    free(roles_str);
    json_free(result);
    
    http_response_add_header(response, "Content-Type", "application/json");
    
    return response;
}

/**
 * Handle get role request
 * 
 * GET /api/rbac/roles/:id
 */
http_response_t* api_handle_rbac_get_role(api_context_t* ctx, http_request_t* request) {
    rbac_api_context_t* rbac_ctx = get_rbac_api_context(ctx);
    if (!rbac_ctx || !rbac_ctx->db || !rbac_ctx->rbac) {
        return create_error_response("RBAC not initialized", 500);
    }
    
    /* Check if user has admin permission */
    const char* user_id = http_request_get_user_id(request);
    if (!user_id || !rbac_db_check_permission(rbac_ctx->db, user_id, RBAC_ROLE, "*", RBAC_READ)) {
        return create_error_response("Unauthorized", 403);
    }
    
    /* Get role ID from URL */
    const char* role_id = http_request_get_param(request, "id");
    if (!role_id) {
        return create_error_response("Role ID not specified", 400);
    }
    
    /* Get role document */
    json_value_t* role_doc = db_get_document(rbac_ctx->db, RBAC_ROLES_COLLECTION, role_id);
    if (!role_doc) {
        return create_error_response("Role not found", 404);
    }
    
    /* Create response */
    char* role_str = json_stringify(role_doc);
    http_response_t* response = http_response_create(200, role_str);
    free(role_str);
    json_free(role_doc);
    
    http_response_add_header(response, "Content-Type", "application/json");
    
    return response;
}

/**
 * Handle create role request
 * 
 * POST /api/rbac/roles
 * 
 * Body: { "name": "..." }
 */
http_response_t* api_handle_rbac_create_role(api_context_t* ctx, http_request_t* request) {
    rbac_api_context_t* rbac_ctx = get_rbac_api_context(ctx);
    if (!rbac_ctx || !rbac_ctx->db || !rbac_ctx->rbac) {
        return create_error_response("RBAC not initialized", 500);
    }
    
    /* Check if user has admin permission */
    const char* user_id = http_request_get_user_id(request);
    if (!user_id || !rbac_db_check_permission(rbac_ctx->db, user_id, RBAC_ROLE, "*", RBAC_ADMIN)) {
        return create_error_response("Unauthorized", 403);
    }
    
    /* Parse request body */
    const char* body = http_request_get_body(request);
    if (!body) {
        return create_error_response("Request body is empty", 400);
    }
    
    json_value_t* request_json = json_parse(body);
    if (!request_json || request_json->type != JSON_OBJECT) {
        if (request_json) json_free(request_json);
        return create_error_response("Invalid JSON body", 400);
    }
    
    /* Get name */
    json_value_t* name_val = json_object_get(request_json, "name");
    
    if (!name_val || name_val->type != JSON_STRING) {
        json_free(request_json);
        return create_error_response("Role name is required", 400);
    }
    
    const char* name = name_val->value.string;
    
    /* Create role */
    rbac_role_t* role = rbac_db_create_role(rbac_ctx->db, name);
    json_free(request_json);
    
    if (!role) {
        return create_error_response("Failed to create role", 500);
    }
    
    /* Create response */
    json_value_t* response_json = json_create_object();
    json_object_set(response_json, "id", json_create_string(role->id));
    json_object_set(response_json, "name", json_create_string(role->name));
    json_object_set(response_json, "permissions", json_create_object());
    json_object_set(response_json, "users", json_create_array());
    
    char* response_str = json_stringify(response_json);
    http_response_t* response = http_response_create(201, response_str);
    free(response_str);
    json_free(response_json);
    
    /* Free role */
    rbac_free_role(role);
    
    http_response_add_header(response, "Content-Type", "application/json");
    
    return response;
}

/**
 * Handle delete role request
 * 
 * DELETE /api/rbac/roles/:id
 */
http_response_t* api_handle_rbac_delete_role(api_context_t* ctx, http_request_t* request) {
    rbac_api_context_t* rbac_ctx = get_rbac_api_context(ctx);
    if (!rbac_ctx || !rbac_ctx->db || !rbac_ctx->rbac) {
        return create_error_response("RBAC not initialized", 500);
    }
    
    /* Check if user has admin permission */
    const char* user_id = http_request_get_user_id(request);
    if (!user_id || !rbac_db_check_permission(rbac_ctx->db, user_id, RBAC_ROLE, "*", RBAC_ADMIN)) {
        return create_error_response("Unauthorized", 403);
    }
    
    /* Get role ID from URL */
    const char* role_id = http_request_get_param(request, "id");
    if (!role_id) {
        return create_error_response("Role ID not specified", 400);
    }
    
    /* Delete role */
    int result = rbac_db_delete_role(rbac_ctx->db, role_id);
    if (!result) {
        return create_error_response("Failed to delete role", 500);
    }
    
    /* Create response */
    json_value_t* response_json = json_create_object();
    json_object_set(response_json, "success", json_create_bool(1));
    
    char* response_str = json_stringify(response_json);
    http_response_t* response = http_response_create(200, response_str);
    free(response_str);
    json_free(response_json);
    
    http_response_add_header(response, "Content-Type", "application/json");
    
    return response;
}

/**
 * Handle add user to role request
 * 
 * POST /api/rbac/roles/:id/users
 * 
 * Body: { "user_id": "..." }
 */
http_response_t* api_handle_rbac_add_user_to_role(api_context_t* ctx, http_request_t* request) {
    rbac_api_context_t* rbac_ctx = get_rbac_api_context(ctx);
    if (!rbac_ctx || !rbac_ctx->db || !rbac_ctx->rbac) {
        return create_error_response("RBAC not initialized", 500);
    }
    
    /* Check if user has admin permission */
    const char* requester_id = http_request_get_user_id(request);
    if (!requester_id || !rbac_db_check_permission(rbac_ctx->db, requester_id, RBAC_ROLE, "*", RBAC_ADMIN)) {
        return create_error_response("Unauthorized", 403);
    }
    
    /* Get role ID from URL */
    const char* role_id = http_request_get_param(request, "id");
    if (!role_id) {
        return create_error_response("Role ID not specified", 400);
    }
    
    /* Parse request body */
    const char* body = http_request_get_body(request);
    if (!body) {
        return create_error_response("Request body is empty", 400);
    }
    
    json_value_t* request_json = json_parse(body);
    if (!request_json || request_json->type != JSON_OBJECT) {
        if (request_json) json_free(request_json);
        return create_error_response("Invalid JSON body", 400);
    }
    
    /* Get user ID */
    json_value_t* user_id_val = json_object_get(request_json, "user_id");
    
    if (!user_id_val || user_id_val->type != JSON_STRING) {
        json_free(request_json);
        return create_error_response("User ID is required", 400);
    }
    
    const char* user_id = user_id_val->value.string;
    json_free(request_json);
    
    /* Add user to role */
    int result = rbac_db_add_user_to_role(rbac_ctx->db, user_id, role_id);
    if (!result) {
        return create_error_response("Failed to add user to role", 500);
    }
    
    /* Create response */
    json_value_t* response_json = json_create_object();
    json_object_set(response_json, "success", json_create_bool(1));
    
    char* response_str = json_stringify(response_json);
    http_response_t* response = http_response_create(200, response_str);
    free(response_str);
    json_free(response_json);
    
    http_response_add_header(response, "Content-Type", "application/json");
    
    return response;
}

/**
 * Handle remove user from role request
 * 
 * DELETE /api/rbac/roles/:id/users/:user_id
 */
http_response_t* api_handle_rbac_remove_user_from_role(api_context_t* ctx, http_request_t* request) {
    rbac_api_context_t* rbac_ctx = get_rbac_api_context(ctx);
    if (!rbac_ctx || !rbac_ctx->db || !rbac_ctx->rbac) {
        return create_error_response("RBAC not initialized", 500);
    }
    
    /* Check if user has admin permission */
    const char* requester_id = http_request_get_user_id(request);
    if (!requester_id || !rbac_db_check_permission(rbac_ctx->db, requester_id, RBAC_ROLE, "*", RBAC_ADMIN)) {
        return create_error_response("Unauthorized", 403);
    }
    
    /* Get role ID and user ID from URL */
    const char* role_id = http_request_get_param(request, "id");
    const char* user_id = http_request_get_param(request, "user_id");
    
    if (!role_id) {
        return create_error_response("Role ID not specified", 400);
    }
    
    if (!user_id) {
        return create_error_response("User ID not specified", 400);
    }
    
    /* Remove user from role */
    int result = rbac_db_remove_user_from_role(rbac_ctx->db, user_id, role_id);
    if (!result) {
        return create_error_response("Failed to remove user from role", 500);
    }
    
    /* Create response */
    json_value_t* response_json = json_create_object();
    json_object_set(response_json, "success", json_create_bool(1));
    
    char* response_str = json_stringify(response_json);
    http_response_t* response = http_response_create(200, response_str);
    free(response_str);
    json_free(response_json);
    
    http_response_add_header(response, "Content-Type", "application/json");
    
    return response;
}

/**
 * Handle grant permission request
 * 
 * POST /api/rbac/roles/:id/permissions
 * 
 * Body: { "resource_type": 0, "resource_id": "...", "permission": 1 }
 */
http_response_t* api_handle_rbac_grant_permission(api_context_t* ctx, http_request_t* request) {
    rbac_api_context_t* rbac_ctx = get_rbac_api_context(ctx);
    if (!rbac_ctx || !rbac_ctx->db || !rbac_ctx->rbac) {
        return create_error_response("RBAC not initialized", 500);
    }
    
    /* Check if user has admin permission */
    const char* requester_id = http_request_get_user_id(request);
    if (!requester_id || !rbac_db_check_permission(rbac_ctx->db, requester_id, RBAC_PERMISSION, "*", RBAC_ADMIN)) {
        return create_error_response("Unauthorized", 403);
    }
    
    /* Get role ID from URL */
    const char* role_id = http_request_get_param(request, "id");
    if (!role_id) {
        return create_error_response("Role ID not specified", 400);
    }
    
    /* Parse request body */
    const char* body = http_request_get_body(request);
    if (!body) {
        return create_error_response("Request body is empty", 400);
    }
    
    json_value_t* request_json = json_parse(body);
    if (!request_json || request_json->type != JSON_OBJECT) {
        if (request_json) json_free(request_json);
        return create_error_response("Invalid JSON body", 400);
    }
    
    /* Get resource type, resource ID, and permission */
    json_value_t* resource_type_val = json_object_get(request_json, "resource_type");
    json_value_t* resource_id_val = json_object_get(request_json, "resource_id");
    json_value_t* permission_val = json_object_get(request_json, "permission");
    
    if (!resource_type_val || resource_type_val->type != JSON_NUMBER ||
        !resource_id_val || resource_id_val->type != JSON_STRING ||
        !permission_val || permission_val->type != JSON_NUMBER) {
        json_free(request_json);
        return create_error_response("Resource type, resource ID, and permission are required", 400);
    }
    
    rbac_resource_type_t resource_type = (rbac_resource_type_t)(int)resource_type_val->value.number;
    const char* resource_id = resource_id_val->value.string;
    rbac_permission_t permission = (rbac_permission_t)(int)permission_val->value.number;
    
    json_free(request_json);
    
    /* Grant permission */
    int result = rbac_db_grant_permission(rbac_ctx->db, role_id, resource_type, resource_id, permission);
    if (!result) {
        return create_error_response("Failed to grant permission", 500);
    }
    
    /* Create response */
    json_value_t* response_json = json_create_object();
    json_object_set(response_json, "success", json_create_bool(1));
    
    char* response_str = json_stringify(response_json);
    http_response_t* response = http_response_create(200, response_str);
    free(response_str);
    json_free(response_json);
    
    http_response_add_header(response, "Content-Type", "application/json");
    
    return response;
}

/**
 * Handle revoke permission request
 * 
 * DELETE /api/rbac/roles/:id/permissions
 * 
 * Body: { "resource_type": 0, "resource_id": "...", "permission": 1 }
 */
http_response_t* api_handle_rbac_revoke_permission(api_context_t* ctx, http_request_t* request) {
    rbac_api_context_t* rbac_ctx = get_rbac_api_context(ctx);
    if (!rbac_ctx || !rbac_ctx->db || !rbac_ctx->rbac) {
        return create_error_response("RBAC not initialized", 500);
    }
    
    /* Check if user has admin permission */
    const char* requester_id = http_request_get_user_id(request);
    if (!requester_id || !rbac_db_check_permission(rbac_ctx->db, requester_id, RBAC_PERMISSION, "*", RBAC_ADMIN)) {
        return create_error_response("Unauthorized", 403);
    }
    
    /* Get role ID from URL */
    const char* role_id = http_request_get_param(request, "id");
    if (!role_id) {
        return create_error_response("Role ID not specified", 400);
    }
    
    /* Parse request body */
    const char* body = http_request_get_body(request);
    if (!body) {
        return create_error_response("Request body is empty", 400);
    }
    
    json_value_t* request_json = json_parse(body);
    if (!request_json || request_json->type != JSON_OBJECT) {
        if (request_json) json_free(request_json);
        return create_error_response("Invalid JSON body", 400);
    }
    
    /* Get resource type, resource ID, and permission */
    json_value_t* resource_type_val = json_object_get(request_json, "resource_type");
    json_value_t* resource_id_val = json_object_get(request_json, "resource_id");
    json_value_t* permission_val = json_object_get(request_json, "permission");
    
    if (!resource_type_val || resource_type_val->type != JSON_NUMBER ||
        !resource_id_val || resource_id_val->type != JSON_STRING ||
        !permission_val || permission_val->type != JSON_NUMBER) {
        json_free(request_json);
        return create_error_response("Resource type, resource ID, and permission are required", 400);
    }
    
    rbac_resource_type_t resource_type = (rbac_resource_type_t)(int)resource_type_val->value.number;
    const char* resource_id = resource_id_val->value.string;
    rbac_permission_t permission = (rbac_permission_t)(int)permission_val->value.number;
    
    json_free(request_json);
    
    /* Revoke permission */
    int result = rbac_db_revoke_permission(rbac_ctx->db, role_id, resource_type, resource_id, permission);
    if (!result) {
        return create_error_response("Failed to revoke permission", 500);
    }
    
    /* Create response */
    json_value_t* response_json = json_create_object();
    json_object_set(response_json, "success", json_create_bool(1));
    
    char* response_str = json_stringify(response_json);
    http_response_t* response = http_response_create(200, response_str);
    free(response_str);
    json_free(response_json);
    
    http_response_add_header(response, "Content-Type", "application/json");
    
    return response;
}

/**
 * Register RBAC API routes
 * 
 * @param api_routes API routes array
 * @param num_routes Number of routes
 * @param db Database instance
 * @param rbac RBAC system
 * @param rbac_file_path Path to RBAC file
 * @return Updated number of routes
 */
int rbac_api_register_routes(api_route_t* api_routes, int num_routes, database_t* db, 
                            rbac_system_t* rbac, const char* rbac_file_path) {
    /* Create RBAC API context */
    rbac_api_context_t* rbac_ctx = (rbac_api_context_t*)malloc(sizeof(rbac_api_context_t));
    if (!rbac_ctx) {
        LOG_ERROR("Failed to allocate RBAC API context");
        return num_routes;
    }
    
    rbac_ctx->db = db;
    rbac_ctx->rbac = rbac;
    rbac_ctx->rbac_file_path = rbac_file_path;
    
    /* Register routes */
    api_routes[num_routes++] = (api_route_t){"/api/rbac/users", HTTP_GET, api_handle_rbac_get_users, 1, rbac_ctx};
    api_routes[num_routes++] = (api_route_t){"/api/rbac/users/:id", HTTP_GET, api_handle_rbac_get_user, 1, rbac_ctx};
    api_routes[num_routes++] = (api_route_t){"/api/rbac/users", HTTP_POST, api_handle_rbac_create_user, 1, rbac_ctx};
    api_routes[num_routes++] = (api_route_t){"/api/rbac/users/:id", HTTP_PUT, api_handle_rbac_update_user, 1, rbac_ctx};
    api_routes[num_routes++] = (api_route_t){"/api/rbac/users/:id", HTTP_DELETE, api_handle_rbac_delete_user, 1, rbac_ctx};
    
    api_routes[num_routes++] = (api_route_t){"/api/rbac/roles", HTTP_GET, api_handle_rbac_get_roles, 1, rbac_ctx};
    api_routes[num_routes++] = (api_route_t){"/api/rbac/roles/:id", HTTP_GET, api_handle_rbac_get_role, 1, rbac_ctx};
    api_routes[num_routes++] = (api_route_t){"/api/rbac/roles", HTTP_POST, api_handle_rbac_create_role, 1, rbac_ctx};
    api_routes[num_routes++] = (api_route_t){"/api/rbac/roles/:id", HTTP_DELETE, api_handle_rbac_delete_role, 1, rbac_ctx};
    
    api_routes[num_routes++] = (api_route_t){"/api/rbac/roles/:id/users", HTTP_POST, api_handle_rbac_add_user_to_role, 1, rbac_ctx};
    api_routes[num_routes++] = (api_route_t){"/api/rbac/roles/:id/users/:user_id", HTTP_DELETE, api_handle_rbac_remove_user_from_role, 1, rbac_ctx};
    
    api_routes[num_routes++] = (api_route_t){"/api/rbac/roles/:id/permissions", HTTP_POST, api_handle_rbac_grant_permission, 1, rbac_ctx};
    api_routes[num_routes++] = (api_route_t){"/api/rbac/roles/:id/permissions", HTTP_DELETE, api_handle_rbac_revoke_permission, 1, rbac_ctx};
    
    return num_routes;
}