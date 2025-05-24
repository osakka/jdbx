#include "api/api.h"
#include "api/rbac_api.h"
#include "rbac/rbac.h"
#include "rbac/rbac_db.h"
#include "rbac/rbac_enhanced.h"
#include "rbac/jwt.h"
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

/* Helper function to extract a parameter from the URL path */
static char* extract_path_parameter(const char* path, const char* param_name) {
    if (!path || !param_name) {
        return NULL;
    }
    
    /* For now, we'll use a simple approach for role/user IDs
     * Path pattern: /api/rbac/roles/:id or /api/rbac/users/:id
     * We'll extract the last segment after the last '/'
     */
    
    /* Find the last '/' in the path */
    const char* last_slash = strrchr(path, '/');
    if (!last_slash || *(last_slash + 1) == '\0') {
        return NULL;
    }
    
    /* Extract everything after the last slash */
    const char* id_start = last_slash + 1;
    size_t id_len = strlen(id_start);
    
    /* Check if there's another path segment (shouldn't be for :id parameter) */
    const char* next_slash = strchr(id_start, '/');
    if (next_slash) {
        id_len = next_slash - id_start;
    }
    
    /* Allocate and copy the ID */
    char* id = (char*)malloc(id_len + 1);
    if (!id) {
        return NULL;
    }
    
    strncpy(id, id_start, id_len);
    id[id_len] = '\0';
    
    return id;
}


/* Helper function to extract user ID from request */
static const char* get_request_user_id(http_request_t* request) {
    static char user_id_buffer[256]; /* Static buffer to hold the user ID */
    
    if (!request || !request->authorization) {
        LOG_TRACE("RBAC_API: No authorization header in request");
        return NULL;
    }
    
    /* Extract token from Authorization header */
    const char* auth = request->authorization;
    const char* token_str = NULL;
    
    /* Check for "Bearer " prefix */
    if (strncmp(auth, "Bearer ", 7) == 0) {
        token_str = auth + 7;
    } else {
        token_str = auth;
    }
    
    LOG_TRACE("RBAC_API: Extracting user_id from token");
    
    /* Decode the JWT to get the user_id from the 'sub' claim */
    jwt_token_t* token = jwt_decode(token_str);
    if (!token) {
        LOG_TRACE("RBAC_API: Failed to decode JWT token");
        return NULL;
    }
    
    /* Get the subject (user_id) from the token */
    if (!token->payload || !token->payload->sub) {
        LOG_TRACE("RBAC_API: No subject (user_id) in JWT token");
        jwt_free(token);
        return NULL;
    }
    
    /* Copy the user_id to our static buffer */
    strncpy(user_id_buffer, token->payload->sub, sizeof(user_id_buffer) - 1);
    user_id_buffer[sizeof(user_id_buffer) - 1] = '\0';
    
    LOG_TRACE("RBAC_API: Extracted user_id from token: %s", user_id_buffer);
    
    jwt_free(token);
    return user_id_buffer;
}

/**
 * Helper function to create a JSON response
 */
static http_response_t* create_json_response(json_value_t* json_data, int status_code) {
    if (!json_data) {
        return create_error_response("Invalid JSON data", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    char* json_str = json_stringify(json_data);
    http_response_t* response = (http_response_t*)malloc(sizeof(http_response_t));
    if (!response) {
        free(json_str);
        return create_error_response("Memory allocation failed", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    response->status = (http_status_t)status_code;
    response->body = json_str;
    response->content_type = strdup("application/json");
    response->content_length = strlen(json_str);
    response->headers = NULL;
    response->num_headers = 0;
    
    json_free(json_data);
    
    return response;
}

/**
 * Handle get users request
 * 
 * GET /api/rbac/users
 */
http_response_t* api_handle_rbac_get_users(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !ctx->db || !ctx->rbac) {
        return create_error_response("RBAC not initialized", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Check if user has admin permission */
    const char* user_id = get_request_user_id(request);
    LOG_TRACE("RBAC_API: Checking admin permission for user_id: %s", user_id ? user_id : "NULL");
    
    if (!user_id) {
        LOG_ERROR("RBAC_API: No user_id available for permission check");
        return create_error_response("Unauthorized", HTTP_FORBIDDEN);
    }
    
    /* Check admin permission */
    int has_permission = rbac_db_check_permission(ctx->db, user_id, RBAC_USER, "*", RBAC_ADMIN);
    LOG_TRACE("RBAC_API: Permission check result: %d", has_permission);
    
    if (!has_permission) {
        LOG_TRACE("RBAC_API: User %s does not have admin permission", user_id);
        return create_error_response("Unauthorized", HTTP_FORBIDDEN);
    }
    
    /* Query all users */
    json_value_t* query = json_create_object();
    json_value_t* result = db_query_documents(ctx->db, RBAC_USERS_COLLECTION, query);
    json_free(query);
    
    if (!result || result->type != JSON_OBJECT) {
        if (result) json_free(result);
        return create_error_response("Failed to query users", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Get documents array from result */
    json_value_t* documents = json_object_get(result, "documents");
    if (!documents || documents->type != JSON_ARRAY) {
        json_free(result);
        return create_error_response("Invalid query result format", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Create sanitized response (remove password hashes) */
    json_value_t* users = json_create_array();
    for (size_t i = 0; i < documents->value.array.size; i++) {
        json_value_t* user = documents->value.array.items[i];
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
    
    return create_json_response(users, HTTP_OK);
}

/**
 * Handle get user request
 * 
 * GET /api/rbac/users/:id
 */
http_response_t* api_handle_rbac_get_user(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !ctx->db || !ctx->rbac) {
        return create_error_response("RBAC not initialized", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Check if user has admin permission or is requesting their own info */
    const char* requester_id = get_request_user_id(request);
    if (!requester_id) {
        return create_error_response("Unauthorized", HTTP_FORBIDDEN);
    }
    
    /* Get user ID from URL */
    char* user_id = extract_path_parameter(request->path, "id");
    if (!user_id) {
        return create_error_response("User ID not specified", HTTP_BAD_REQUEST);
    }
    
    /* Check if requester has permission */
    int is_self = strcmp(requester_id, user_id) == 0;
    int is_admin = rbac_db_check_permission(ctx->db, requester_id, RBAC_USER, "*", RBAC_ADMIN);
    
    if (!is_self && !is_admin) {
        free(user_id);
        return create_error_response("Unauthorized", HTTP_FORBIDDEN);
    }
    
    /* Get user document */
    json_value_t* user_doc = db_get_document(ctx->db, RBAC_USERS_COLLECTION, user_id);
    free(user_id);
    
    if (!user_doc) {
        return create_error_response("User not found", HTTP_NOT_FOUND);
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
    
    return create_json_response(sanitized, HTTP_OK);
}

/**
 * Handle create user request
 * 
 * POST /api/rbac/users
 * 
 * Body: { "username": "...", "password": "..." }
 */
http_response_t* api_handle_rbac_create_user(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !ctx->db || !ctx->rbac) {
        return create_error_response("RBAC not initialized", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Check if user has admin permission */
    const char* user_id = get_request_user_id(request);
    LOG_TRACE("RBAC_API: Checking admin permission for user_id: %s", user_id ? user_id : "NULL");
    
    if (!user_id) {
        LOG_ERROR("RBAC_API: No user_id available for permission check");
        return create_error_response("Unauthorized", HTTP_FORBIDDEN);
    }
    
    /* Check admin permission */
    int has_permission = rbac_db_check_permission(ctx->db, user_id, RBAC_USER, "*", RBAC_ADMIN);
    LOG_TRACE("RBAC_API: Permission check result: %d", has_permission);
    
    if (!has_permission) {
        LOG_TRACE("RBAC_API: User %s does not have admin permission", user_id);
        return create_error_response("Unauthorized", HTTP_FORBIDDEN);
    }
    
    /* Parse request body */
    if (!request->body) {
        return create_error_response("Request body is empty", HTTP_BAD_REQUEST);
    }
    
    json_value_t* request_json = json_parse(request->body);
    if (!request_json || request_json->type != JSON_OBJECT) {
        if (request_json) json_free(request_json);
        return create_error_response("Invalid JSON body", HTTP_BAD_REQUEST);
    }
    
    /* Get username and password */
    json_value_t* username_val = json_object_get(request_json, "username");
    json_value_t* password_val = json_object_get(request_json, "password");
    
    if (!username_val || username_val->type != JSON_STRING ||
        !password_val || password_val->type != JSON_STRING) {
        json_free(request_json);
        return create_error_response("Username and password are required", HTTP_BAD_REQUEST);
    }
    
    const char* username = username_val->value.string;
    const char* password = password_val->value.string;
    
    /* Create user */
    rbac_user_t* user = rbac_db_create_user(ctx->db, username, password);
    json_free(request_json);
    
    if (!user) {
        return create_error_response("Failed to create user", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Create response */
    json_value_t* response_json = json_create_object();
    json_object_set(response_json, "id", json_create_string(user->id));
    json_object_set(response_json, "username", json_create_string(user->username));
    json_object_set(response_json, "roles", json_create_array());
    
    /* Free user */
    rbac_free_user(user);
    
    return create_json_response(response_json, HTTP_CREATED);
}

/**
 * Handle update user request
 * 
 * PUT /api/rbac/users/:id
 * 
 * Body: { "username": "...", "password": "..." }
 */
http_response_t* api_handle_rbac_update_user(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !ctx->db || !ctx->rbac) {
        return create_error_response("RBAC not initialized", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Check if user has admin permission or is updating their own info */
    const char* requester_id = get_request_user_id(request);
    if (!requester_id) {
        return create_error_response("Unauthorized", HTTP_FORBIDDEN);
    }
    
    /* Get user ID from URL */
    char* user_id = extract_path_parameter(request->path, "id");
    if (!user_id) {
        return create_error_response("User ID not specified", HTTP_BAD_REQUEST);
    }
    
    /* Check if requester has permission */
    int is_self = strcmp(requester_id, user_id) == 0;
    int is_admin = rbac_db_check_permission(ctx->db, requester_id, RBAC_USER, "*", RBAC_ADMIN);
    
    if (!is_self && !is_admin) {
        free(user_id);
        return create_error_response("Unauthorized", HTTP_FORBIDDEN);
    }
    
    /* Parse request body */
    if (!request->body) {
        free(user_id);
        return create_error_response("Request body is empty", HTTP_BAD_REQUEST);
    }
    
    json_value_t* request_json = json_parse(request->body);
    if (!request_json || request_json->type != JSON_OBJECT) {
        if (request_json) json_free(request_json);
        free(user_id);
        return create_error_response("Invalid JSON body", HTTP_BAD_REQUEST);
    }
    
    /* Get user - first check if they exist */
    rbac_user_t* user = rbac_db_get_user(ctx->db, user_id);
    if (!user) {
        json_free(request_json);
        free(user_id);
        return create_error_response("User not found", HTTP_NOT_FOUND);
    }
    
    /* Get the actual user document for updating */
    json_value_t* user_doc = db_get_document(ctx->db, RBAC_USERS_COLLECTION, user->id);
    if (!user_doc) {
        rbac_free_user(user);
        json_free(request_json);
        free(user_id);
        return create_error_response("User document not found", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Store the actual document ID */
    char* actual_doc_id = strdup(user->id);
    rbac_free_user(user);
    
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
        free(user_id);
        free(actual_doc_id);
        return create_error_response("No fields to update", HTTP_BAD_REQUEST);
    }
    
    /* Update user document using actual document ID */
    json_value_t* result = db_update_document(ctx->db, RBAC_USERS_COLLECTION, actual_doc_id, user_doc);
    if (!result) {
        json_free(user_doc);
        free(user_id);
        free(actual_doc_id);
        return create_error_response("Failed to update user", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    free(actual_doc_id);
    
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
    free(user_id);
    
    return create_json_response(sanitized, HTTP_OK);
}

/**
 * Handle delete user request
 * 
 * DELETE /api/rbac/users/:id
 */
http_response_t* api_handle_rbac_delete_user(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !ctx->db || !ctx->rbac) {
        return create_error_response("RBAC not initialized", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Check if user has admin permission */
    const char* requester_id = get_request_user_id(request);
    if (!requester_id || !rbac_db_check_permission(ctx->db, requester_id, RBAC_USER, "*", RBAC_ADMIN)) {
        return create_error_response("Unauthorized", HTTP_FORBIDDEN);
    }
    
    /* Get user ID from URL */
    char* user_id = extract_path_parameter(request->path, "id");
    if (!user_id) {
        return create_error_response("User ID not specified", HTTP_BAD_REQUEST);
    }
    
    /* Prevent deleting own account */
    if (strcmp(requester_id, user_id) == 0) {
        free(user_id);
        return create_error_response("Cannot delete your own account", HTTP_BAD_REQUEST);
    }
    
    /* Delete user */
    int result = rbac_db_delete_user(ctx->db, user_id);
    free(user_id);
    
    if (!result) {
        return create_error_response("Failed to delete user", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Create response */
    json_value_t* response_json = json_create_object();
    json_object_set(response_json, "success", json_create_boolean(1));
    
    return create_json_response(response_json, HTTP_OK);
}

/**
 * Handle get roles request
 * 
 * GET /api/rbac/roles
 */
http_response_t* api_handle_rbac_get_roles(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !ctx->db || !ctx->rbac) {
        return create_error_response("RBAC not initialized", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Check if user has admin permission */
    const char* user_id = get_request_user_id(request);
    LOG_TRACE("RBAC_API: GET /api/rbac/roles - checking permission for user: %s", user_id ? user_id : "NULL");
    
    if (!user_id) {
        LOG_ERROR("RBAC_API: No user_id available for permission check");
        return create_error_response("Unauthorized", HTTP_FORBIDDEN);
    }
    
    /* Check admin permission on roles */
    int has_permission = rbac_db_check_permission(ctx->db, user_id, RBAC_ROLE, "*", RBAC_ADMIN);
    LOG_TRACE("RBAC_API: Role permission check result: %d", has_permission);
    
    if (!has_permission) {
        LOG_TRACE("RBAC_API: User %s does not have role admin permission", user_id);
        return create_error_response("Unauthorized", HTTP_FORBIDDEN);
    }
    
    /* Query all roles */
    json_value_t* query = json_create_object();
    json_value_t* result = db_query_documents(ctx->db, RBAC_ROLES_COLLECTION, query);
    json_free(query);
    
    if (!result || result->type != JSON_OBJECT) {
        if (result) json_free(result);
        return create_error_response("Failed to query roles", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Get documents array from result */
    json_value_t* documents = json_object_get(result, "documents");
    if (!documents || documents->type != JSON_ARRAY) {
        json_free(result);
        return create_error_response("Failed to query roles - invalid result format", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Create response array */
    json_value_t* roles = json_create_array();
    for (size_t i = 0; i < documents->value.array.size; i++) {
        json_value_t* role = documents->value.array.items[i];
        if (role->type == JSON_OBJECT) {
            json_value_t* role_data = json_create_object();
            
            /* Copy id and name */
            json_value_t* id = json_object_get(role, "id");
            json_value_t* name = json_object_get(role, "name");
            json_value_t* permissions = json_object_get(role, "permissions");
            
            if (id && id->type == JSON_STRING) {
                json_object_set(role_data, "id", json_create_string(id->value.string));
            }
            
            if (name && name->type == JSON_STRING) {
                json_object_set(role_data, "name", json_create_string(name->value.string));
            }
            
            if (permissions && permissions->type == JSON_OBJECT) {
                json_object_set(role_data, "permissions", json_clone(permissions));
            } else {
                json_object_set(role_data, "permissions", json_create_object());
            }
            
            json_array_append(roles, role_data);
        }
    }
    
    json_free(result);
    
    return create_json_response(roles, HTTP_OK);
}

/**
 * Handle get role request
 * 
 * GET /api/rbac/roles/:id
 */
http_response_t* api_handle_rbac_get_role(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !ctx->db || !ctx->rbac) {
        return create_error_response("RBAC not initialized", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Check if user has admin permission */
    const char* user_id = get_request_user_id(request);
    if (!user_id || !rbac_db_check_permission(ctx->db, user_id, RBAC_ROLE, "*", RBAC_READ)) {
        return create_error_response("Unauthorized", HTTP_FORBIDDEN);
    }
    
    /* Get role ID from URL */
    char* role_id = extract_path_parameter(request->path, "id");
    if (!role_id) {
        return create_error_response("Role ID not specified", HTTP_BAD_REQUEST);
    }
    
    /* Get role document */
    json_value_t* role_doc = db_get_document(ctx->db, RBAC_ROLES_COLLECTION, role_id);
    free(role_id);
    
    if (!role_doc) {
        return create_error_response("Role not found", HTTP_NOT_FOUND);
    }
    
    /* Create response */
    json_value_t* role_data = json_create_object();
    
    /* Copy id and name */
    json_value_t* id = json_object_get(role_doc, "id");
    json_value_t* name = json_object_get(role_doc, "name");
    json_value_t* permissions = json_object_get(role_doc, "permissions");
    
    if (id && id->type == JSON_STRING) {
        json_object_set(role_data, "id", json_create_string(id->value.string));
    }
    
    if (name && name->type == JSON_STRING) {
        json_object_set(role_data, "name", json_create_string(name->value.string));
    }
    
    if (permissions && permissions->type == JSON_OBJECT) {
        json_object_set(role_data, "permissions", json_clone(permissions));
    } else {
        json_object_set(role_data, "permissions", json_create_object());
    }
    
    json_free(role_doc);
    
    return create_json_response(role_data, HTTP_OK);
}

/**
 * Handle create role request
 * 
 * POST /api/rbac/roles
 * 
 * Body: { "name": "..." }
 */
http_response_t* api_handle_rbac_create_role(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !ctx->db || !ctx->rbac) {
        return create_error_response("RBAC not initialized", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Check if user has admin permission */
    const char* user_id = get_request_user_id(request);
    if (!user_id || !rbac_db_check_permission(ctx->db, user_id, RBAC_ROLE, "*", RBAC_ADMIN)) {
        return create_error_response("Unauthorized", HTTP_FORBIDDEN);
    }
    
    /* Parse request body */
    if (!request->body) {
        return create_error_response("Request body is empty", HTTP_BAD_REQUEST);
    }
    
    json_value_t* request_json = json_parse(request->body);
    if (!request_json || request_json->type != JSON_OBJECT) {
        if (request_json) json_free(request_json);
        return create_error_response("Invalid JSON body", HTTP_BAD_REQUEST);
    }
    
    /* Get role name */
    json_value_t* name_val = json_object_get(request_json, "name");
    if (!name_val || name_val->type != JSON_STRING) {
        json_free(request_json);
        return create_error_response("Role name is required", HTTP_BAD_REQUEST);
    }
    
    const char* name = name_val->value.string;
    
    /* Get permissions from request */
    json_value_t* permissions_val = json_object_get(request_json, "permissions");
    
    /* Create role */
    rbac_role_t* role = rbac_db_create_role(ctx->db, name);
    
    if (!role) {
        json_free(request_json);
        return create_error_response("Failed to create role", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Set permissions if provided */  
    if (permissions_val && permissions_val->type == JSON_OBJECT) {
        /* TODO: Currently rbac_db_create_role creates roles with empty permissions.
         * We need to implement permission updates separately. For now, we'll
         * include them in the response but they won't persist. */
        
        /* Update the role's permissions in memory for the response */
        if (role->permissions) {
            json_free(role->permissions);
        }
        role->permissions = json_clone(permissions_val);
    }
    
    json_free(request_json);
    
    /* Create response */
    json_value_t* response_json = json_create_object();
    json_object_set(response_json, "id", json_create_string(role->id));
    json_object_set(response_json, "name", json_create_string(role->name));
    
    /* Include permissions in response */
    if (role->permissions) {
        json_object_set(response_json, "permissions", json_clone(role->permissions));
    } else {
        json_object_set(response_json, "permissions", json_create_object());
    }
    
    /* Free role */
    rbac_free_role(role);
    
    return create_json_response(response_json, HTTP_CREATED);
}

/**
 * Handle delete role request
 * 
 * DELETE /api/rbac/roles/:id
 */
http_response_t* api_handle_rbac_delete_role(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !ctx->db || !ctx->rbac) {
        return create_error_response("RBAC not initialized", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Check if user has admin permission */
    const char* user_id = get_request_user_id(request);
    if (!user_id || !rbac_db_check_permission(ctx->db, user_id, RBAC_ROLE, "*", RBAC_ADMIN)) {
        return create_error_response("Unauthorized", HTTP_FORBIDDEN);
    }
    
    /* Get role ID from URL */
    char* role_id = extract_path_parameter(request->path, "id");
    if (!role_id) {
        return create_error_response("Role ID not specified", HTTP_BAD_REQUEST);
    }
    
    /* Delete role */
    int result = rbac_db_delete_role(ctx->db, role_id);
    free(role_id);
    
    if (!result) {
        return create_error_response("Failed to delete role", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Create response */
    json_value_t* response_json = json_create_object();
    json_object_set(response_json, "success", json_create_boolean(1));
    
    return create_json_response(response_json, HTTP_OK);
}

/**
 * Handle update role request
 * 
 * PUT /api/rbac/roles/:id
 * 
 * Body: { "name": "...", "permissions": {...} }
 */
http_response_t* api_handle_rbac_update_role(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !ctx->db || !ctx->rbac) {
        return create_error_response("RBAC not initialized", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Check if user has admin permission */
    const char* requester_id = get_request_user_id(request);
    if (!requester_id || !rbac_db_check_permission(ctx->db, requester_id, RBAC_ROLE, "*", RBAC_ADMIN)) {
        return create_error_response("Unauthorized", HTTP_FORBIDDEN);
    }
    
    /* Get role ID from URL */
    char* role_id = extract_path_parameter(request->path, "id");
    if (!role_id) {
        return create_error_response("Role ID not specified", HTTP_BAD_REQUEST);
    }
    
    /* Parse request body */
    if (!request->body) {
        free(role_id);
        return create_error_response("Request body required", HTTP_BAD_REQUEST);
    }
    
    json_value_t* body = json_parse(request->body);
    if (!body || body->type != JSON_OBJECT) {
        free(role_id);
        if (body) json_free(body);
        return create_error_response("Invalid JSON body", HTTP_BAD_REQUEST);
    }
    
    /* Check if role exists first */
    rbac_role_t* existing_role_obj = rbac_db_get_role(ctx->db, role_id);
    if (!existing_role_obj) {
        free(role_id);
        json_free(body);
        return create_error_response("Role not found", HTTP_NOT_FOUND);
    }
    
    /* Extract name from body (optional) */
    json_value_t* name_val = json_object_get(body, "name");
    const char* name = NULL;
    if (name_val && name_val->type == JSON_STRING) {
        name = name_val->value.string;
    } else {
        /* Keep existing name if not provided */
        name = existing_role_obj->name;
    }
    
    /* Extract permissions from body (optional) */
    json_value_t* permissions_val = json_object_get(body, "permissions");
    json_value_t* permissions = NULL;
    if (permissions_val && permissions_val->type == JSON_OBJECT) {
        permissions = json_clone(permissions_val);
    } else {
        /* Keep existing permissions if not provided */
        if (existing_role_obj->permissions) {
            permissions = json_clone(existing_role_obj->permissions);
        } else {
            permissions = json_create_object();
        }
    }
    
    /* Update role */
    int result = rbac_db_update_role(ctx->db, role_id, name, permissions);
    
    /* Clean up */
    rbac_free_role(existing_role_obj);
    json_free(body);
    json_free(permissions);
    free(role_id);
    
    if (!result) {
        return create_error_response("Failed to update role", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Create response */
    json_value_t* response_json = json_create_object();
    json_object_set(response_json, "success", json_create_boolean(1));
    
    return create_json_response(response_json, HTTP_OK);
}

/**
 * Handle add user to role request
 * 
 * POST /api/rbac/roles/:id/users/:user_id
 */
http_response_t* api_handle_rbac_add_user_to_role(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !ctx->db || !ctx->rbac) {
        return create_error_response("RBAC not initialized", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Check if user has admin permission */
    const char* requester_id = get_request_user_id(request);
    if (!requester_id || !rbac_db_check_permission(ctx->db, requester_id, RBAC_ROLE, "*", RBAC_ADMIN)) {
        return create_error_response("Unauthorized", HTTP_FORBIDDEN);
    }
    
    /* Get role ID and user ID from URL */
    char* role_id = extract_path_parameter(request->path, "id");
    char* user_id = extract_path_parameter(request->path, "user_id");
    
    if (!role_id || !user_id) {
        if (role_id) free(role_id);
        if (user_id) free(user_id);
        return create_error_response("Role ID and User ID are required", HTTP_BAD_REQUEST);
    }
    
    /* Add user to role */
    int result = rbac_db_add_user_to_role(ctx->db, user_id, role_id);
    free(role_id);
    free(user_id);
    
    if (!result) {
        return create_error_response("Failed to add user to role", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Create response */
    json_value_t* response_json = json_create_object();
    json_object_set(response_json, "success", json_create_boolean(1));
    
    return create_json_response(response_json, HTTP_OK);
}

/**
 * Handle remove user from role request
 * 
 * DELETE /api/rbac/roles/:id/users/:user_id
 */
http_response_t* api_handle_rbac_remove_user_from_role(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !ctx->db || !ctx->rbac) {
        return create_error_response("RBAC not initialized", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Check if user has admin permission */
    const char* requester_id = get_request_user_id(request);
    if (!requester_id || !rbac_db_check_permission(ctx->db, requester_id, RBAC_ROLE, "*", RBAC_ADMIN)) {
        return create_error_response("Unauthorized", HTTP_FORBIDDEN);
    }
    
    /* Get role ID and user ID from URL */
    char* role_id = extract_path_parameter(request->path, "id");
    char* user_id = extract_path_parameter(request->path, "user_id");
    
    if (!role_id || !user_id) {
        if (role_id) free(role_id);
        if (user_id) free(user_id);
        return create_error_response("Role ID and User ID are required", HTTP_BAD_REQUEST);
    }
    
    /* Remove user from role */
    int result = rbac_db_remove_user_from_role(ctx->db, user_id, role_id);
    free(role_id);
    free(user_id);
    
    if (!result) {
        return create_error_response("Failed to remove user from role", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Create response */
    json_value_t* response_json = json_create_object();
    json_object_set(response_json, "success", json_create_boolean(1));
    
    return create_json_response(response_json, HTTP_OK);
}

/**
 * Handle grant permission request
 * 
 * POST /api/rbac/roles/:id/permissions
 * 
 * Body: { "resource_type": "...", "resource_id": "...", "permission": "..." }
 */
http_response_t* api_handle_rbac_grant_permission(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !ctx->db || !ctx->rbac) {
        return create_error_response("RBAC not initialized", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Check if user has admin permission */
    const char* user_id = get_request_user_id(request);
    if (!user_id || !rbac_db_check_permission(ctx->db, user_id, RBAC_PERMISSION, "*", RBAC_ADMIN)) {
        return create_error_response("Unauthorized", HTTP_FORBIDDEN);
    }
    
    /* Get role ID from URL */
    char* role_id = extract_path_parameter(request->path, "id");
    if (!role_id) {
        return create_error_response("Role ID not specified", HTTP_BAD_REQUEST);
    }
    
    /* Parse request body */
    if (!request->body) {
        free(role_id);
        return create_error_response("Request body is empty", HTTP_BAD_REQUEST);
    }
    
    json_value_t* request_json = json_parse(request->body);
    if (!request_json || request_json->type != JSON_OBJECT) {
        if (request_json) json_free(request_json);
        free(role_id);
        return create_error_response("Invalid JSON body", HTTP_BAD_REQUEST);
    }
    
    /* Get permission details */
    json_value_t* resource_type_val = json_object_get(request_json, "resource_type");
    json_value_t* resource_id_val = json_object_get(request_json, "resource_id");
    json_value_t* permission_val = json_object_get(request_json, "permission");
    
    if (!resource_type_val || resource_type_val->type != JSON_STRING ||
        !resource_id_val || resource_id_val->type != JSON_STRING ||
        !permission_val || permission_val->type != JSON_STRING) {
        json_free(request_json);
        free(role_id);
        return create_error_response("Resource type, resource ID, and permission are required", HTTP_BAD_REQUEST);
    }
    
    /* Convert resource type string to enum */
    rbac_resource_type_t resource_type = RBAC_UNKNOWN;
    const char* resource_type_str = resource_type_val->value.string;
    if (strcmp(resource_type_str, "database") == 0) {
        resource_type = RBAC_DATABASE;
    } else if (strcmp(resource_type_str, "collection") == 0) {
        resource_type = RBAC_COLLECTION;
    } else if (strcmp(resource_type_str, "document") == 0) {
        resource_type = RBAC_DOCUMENT;
    } else if (strcmp(resource_type_str, "user") == 0) {
        resource_type = RBAC_USER;
    } else if (strcmp(resource_type_str, "role") == 0) {
        resource_type = RBAC_ROLE;
    } else if (strcmp(resource_type_str, "permission") == 0) {
        resource_type = RBAC_PERMISSION;
    } else {
        json_free(request_json);
        free(role_id);
        return create_error_response("Invalid resource type", HTTP_BAD_REQUEST);
    }
    
    /* Convert permission string to enum */
    rbac_permission_t permission = 0;
    const char* permission_str = permission_val->value.string;
    if (strcmp(permission_str, "read") == 0) {
        permission = RBAC_READ;
    } else if (strcmp(permission_str, "write") == 0) {
        permission = RBAC_WRITE;
    } else if (strcmp(permission_str, "delete") == 0) {
        permission = RBAC_DELETE;
    } else if (strcmp(permission_str, "admin") == 0) {
        permission = RBAC_ADMIN;
    } else {
        json_free(request_json);
        free(role_id);
        return create_error_response("Invalid permission", HTTP_BAD_REQUEST);
    }
    
    const char* resource_id = resource_id_val->value.string;
    
    /* Grant permission */
    int result = rbac_db_grant_permission(ctx->db, role_id, resource_type, resource_id, permission);
    json_free(request_json);
    free(role_id);
    
    if (!result) {
        return create_error_response("Failed to grant permission", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Create response */
    json_value_t* response_json = json_create_object();
    json_object_set(response_json, "success", json_create_boolean(1));
    
    return create_json_response(response_json, HTTP_OK);
}

/**
 * Handle revoke permission request
 * 
 * DELETE /api/rbac/roles/:id/permissions
 * 
 * Body: { "resource_type": "...", "resource_id": "...", "permission": "..." }
 */
http_response_t* api_handle_rbac_revoke_permission(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !ctx->db || !ctx->rbac) {
        return create_error_response("RBAC not initialized", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Check if user has admin permission */
    const char* user_id = get_request_user_id(request);
    if (!user_id || !rbac_db_check_permission(ctx->db, user_id, RBAC_PERMISSION, "*", RBAC_ADMIN)) {
        return create_error_response("Unauthorized", HTTP_FORBIDDEN);
    }
    
    /* Get role ID from URL */
    char* role_id = extract_path_parameter(request->path, "id");
    if (!role_id) {
        return create_error_response("Role ID not specified", HTTP_BAD_REQUEST);
    }
    
    /* Parse request body */
    if (!request->body) {
        free(role_id);
        return create_error_response("Request body is empty", HTTP_BAD_REQUEST);
    }
    
    json_value_t* request_json = json_parse(request->body);
    if (!request_json || request_json->type != JSON_OBJECT) {
        if (request_json) json_free(request_json);
        free(role_id);
        return create_error_response("Invalid JSON body", HTTP_BAD_REQUEST);
    }
    
    /* Get permission details */
    json_value_t* resource_type_val = json_object_get(request_json, "resource_type");
    json_value_t* resource_id_val = json_object_get(request_json, "resource_id");
    json_value_t* permission_val = json_object_get(request_json, "permission");
    
    if (!resource_type_val || resource_type_val->type != JSON_STRING ||
        !resource_id_val || resource_id_val->type != JSON_STRING ||
        !permission_val || permission_val->type != JSON_STRING) {
        json_free(request_json);
        free(role_id);
        return create_error_response("Resource type, resource ID, and permission are required", HTTP_BAD_REQUEST);
    }
    
    /* Convert resource type string to enum */
    rbac_resource_type_t resource_type = RBAC_UNKNOWN;
    const char* resource_type_str = resource_type_val->value.string;
    if (strcmp(resource_type_str, "database") == 0) {
        resource_type = RBAC_DATABASE;
    } else if (strcmp(resource_type_str, "collection") == 0) {
        resource_type = RBAC_COLLECTION;
    } else if (strcmp(resource_type_str, "document") == 0) {
        resource_type = RBAC_DOCUMENT;
    } else if (strcmp(resource_type_str, "user") == 0) {
        resource_type = RBAC_USER;
    } else if (strcmp(resource_type_str, "role") == 0) {
        resource_type = RBAC_ROLE;
    } else if (strcmp(resource_type_str, "permission") == 0) {
        resource_type = RBAC_PERMISSION;
    } else {
        json_free(request_json);
        free(role_id);
        return create_error_response("Invalid resource type", HTTP_BAD_REQUEST);
    }
    
    /* Convert permission string to enum */
    rbac_permission_t permission = 0;
    const char* permission_str = permission_val->value.string;
    if (strcmp(permission_str, "read") == 0) {
        permission = RBAC_READ;
    } else if (strcmp(permission_str, "write") == 0) {
        permission = RBAC_WRITE;
    } else if (strcmp(permission_str, "delete") == 0) {
        permission = RBAC_DELETE;
    } else if (strcmp(permission_str, "admin") == 0) {
        permission = RBAC_ADMIN;
    } else {
        json_free(request_json);
        free(role_id);
        return create_error_response("Invalid permission", HTTP_BAD_REQUEST);
    }
    
    const char* resource_id = resource_id_val->value.string;
    
    /* Revoke permission */
    int result = rbac_db_revoke_permission(ctx->db, role_id, resource_type, resource_id, permission);
    json_free(request_json);
    free(role_id);
    
    if (!result) {
        return create_error_response("Failed to revoke permission", HTTP_INTERNAL_SERVER_ERROR);
    }
    
    /* Create response */
    json_value_t* response_json = json_create_object();
    json_object_set(response_json, "success", json_create_boolean(1));
    
    return create_json_response(response_json, HTTP_OK);
}

/* Register RBAC API routes */
int rbac_api_register_routes(api_route_t* api_routes, int num_routes, database_t* db, 
                            rbac_system_t* rbac) {
    /* Suppress unused parameter warnings */
    (void)db;
    (void)rbac;
    
    /* Register user management routes */
    api_routes[num_routes++] = (api_route_t){"/api/rbac/users", HTTP_GET, api_handle_rbac_get_users, 1};
    api_routes[num_routes++] = (api_route_t){"/api/rbac/users/:id", HTTP_GET, api_handle_rbac_get_user, 1};
    api_routes[num_routes++] = (api_route_t){"/api/rbac/users", HTTP_POST, api_handle_rbac_create_user, 1};
    api_routes[num_routes++] = (api_route_t){"/api/rbac/users/:id", HTTP_PUT, api_handle_rbac_update_user, 1};
    api_routes[num_routes++] = (api_route_t){"/api/rbac/users/:id", HTTP_DELETE, api_handle_rbac_delete_user, 1};
    
    /* Register role management routes */
    api_routes[num_routes++] = (api_route_t){"/api/rbac/roles", HTTP_GET, api_handle_rbac_get_roles, 1};
    api_routes[num_routes++] = (api_route_t){"/api/rbac/roles/:id", HTTP_GET, api_handle_rbac_get_role, 1};
    api_routes[num_routes++] = (api_route_t){"/api/rbac/roles", HTTP_POST, api_handle_rbac_create_role, 1};
    api_routes[num_routes++] = (api_route_t){"/api/rbac/roles/:id", HTTP_PUT, api_handle_rbac_update_role, 1};
    api_routes[num_routes++] = (api_route_t){"/api/rbac/roles/:id", HTTP_DELETE, api_handle_rbac_delete_role, 1};
    
    /* Register role-user management routes */
    api_routes[num_routes++] = (api_route_t){"/api/rbac/roles/:id/users/:user_id", HTTP_POST, api_handle_rbac_add_user_to_role, 1};
    api_routes[num_routes++] = (api_route_t){"/api/rbac/roles/:id/users/:user_id", HTTP_DELETE, api_handle_rbac_remove_user_from_role, 1};
    
    /* Register permission management routes */
    api_routes[num_routes++] = (api_route_t){"/api/rbac/roles/:id/permissions", HTTP_POST, api_handle_rbac_grant_permission, 1};
    api_routes[num_routes++] = (api_route_t){"/api/rbac/roles/:id/permissions", HTTP_DELETE, api_handle_rbac_revoke_permission, 1};
    
    return num_routes;
}