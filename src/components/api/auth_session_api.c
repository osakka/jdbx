#include "api/api.h"
#include "database/document_storage.h"
#include "database/database.h"
#include "database/virtual_layer.h"
#include "rbac/jwt.h"
#include "rbac/rbac_database.h"
#include "rbac/rbac.h"
#include "utils/json.h"
#include "utils/logger.h"
#include "utils/buffer_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/**
 * Get current session information
 * Returns the session details for the authenticated user
 */
http_response_t* api_handle_get_current_session(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !ctx->db || !request) {
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Extract token */
    char* token = api_extract_token(request);
    if (!token) {
        return create_http_response(HTTP_UNAUTHORIZED,
                     "{\"error\":\"No authorization token\"}", "application/json");
    }
    
    /* Decode JWT */
    jwt_token_t* jwt = jwt_decode(token);
    if (!jwt || !jwt->payload) {
        BUFFER_FREE(token);
        if (jwt) jwt_free(jwt);
        return create_http_response(HTTP_UNAUTHORIZED,
                     "{\"error\":\"Invalid token\"}", "application/json");
    }
    
    /* Extract user info from JWT payload */
    char username[256] = {0};
    char library[256] = "default";
    char user_uuid[256] = {0};
    char session_id[256] = {0};
    
    /* Get user ID (subject) */
    if (jwt->payload->sub) {
        strncpy(user_uuid, jwt->payload->sub, sizeof(user_uuid) - 1);
    }
    
    /* Get username from custom claims */
    if (jwt->payload->claims) {
        json_value_t* username_val = json_object_get(jwt->payload->claims, "username");
        if (username_val && username_val->type == JSON_STRING) {
            strncpy(username, username_val->value.string, sizeof(username) - 1);
        }
        
        json_value_t* library_val = json_object_get(jwt->payload->claims, "library");
        if (library_val && library_val->type == JSON_STRING) {
            strncpy(library, library_val->value.string, sizeof(library) - 1);
        }
        
        json_value_t* session_val = json_object_get(jwt->payload->claims, "session_id");
        if (session_val && session_val->type == JSON_STRING) {
            strncpy(session_id, session_val->value.string, sizeof(session_id) - 1);
        }
    }
    
    /* Query the session document */
    /* Use virtual layer to get session by token - single source of truth */
    json_value_t* session_doc = virtual_get_session_by_token(ctx->db, token);
    BUFFER_FREE(token);
    
    if (!session_doc) {
        jwt_free(jwt);
        return create_http_response(HTTP_NOT_FOUND,
                     "{\"error\":\"Session not found\"}", "application/json");
    }
    
    /* Build response with session info */
    json_value_t* response = json_create_object();
    json_object_set(response, "session_id", json_create_string(session_id));
    json_object_set(response, "username", json_create_string(username));
    json_object_set(response, "user_uuid", json_create_string(user_uuid));
    json_object_set(response, "library", json_create_string(library));
    
    /* Add session timestamps */
    json_value_t* created_at = json_object_get(session_doc, "created_at");
    json_value_t* expires_at = json_object_get(session_doc, "expires_at");
    if (created_at) {
        json_object_set(response, "created_at", json_clone(created_at));
    }
    if (expires_at) {
        json_object_set(response, "expires_at", json_clone(expires_at));
    }
    
    /* Add session metadata */
    json_value_t* ip_address = json_object_get(session_doc, "ip_address");
    json_value_t* user_agent = json_object_get(session_doc, "user_agent");
    if (ip_address) {
        json_object_set(response, "ip_address", json_clone(ip_address));
    }
    if (user_agent) {
        json_object_set(response, "user_agent", json_clone(user_agent));
    }
    
    json_free(session_doc);
    jwt_free(jwt);
    
    /* Wrap in standard envelope */
    json_value_t* envelope = json_create_object();
    json_object_set(envelope, "data", response);
    
    char* response_str = json_stringify(envelope);
    json_free(envelope);
    
    return create_http_response(HTTP_OK, response_str, "application/json");
}

/**
 * Get current library context
 * Returns the library the user is currently working in
 */
http_response_t* api_handle_get_library_context(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Extract token */
    char* token = api_extract_token(request);
    if (!token) {
        return create_http_response(HTTP_UNAUTHORIZED,
                     "{\"error\":\"No authorization token\"}", "application/json");
    }
    
    /* Decode JWT */
    jwt_token_t* jwt = jwt_decode(token);
    BUFFER_FREE(token);
    
    if (!jwt || !jwt->payload) {
        if (jwt) jwt_free(jwt);
        return create_http_response(HTTP_UNAUTHORIZED,
                     "{\"error\":\"Invalid token\"}", "application/json");
    }
    
    /* Extract library and username from JWT */
    char library[256] = "default";
    char username[256] = {0};
    
    if (jwt->payload->claims) {
        json_value_t* library_val = json_object_get(jwt->payload->claims, "library");
        if (library_val && library_val->type == JSON_STRING) {
            strncpy(library, library_val->value.string, sizeof(library) - 1);
        }
        
        json_value_t* username_val = json_object_get(jwt->payload->claims, "username");
        if (username_val && username_val->type == JSON_STRING) {
            strncpy(username, username_val->value.string, sizeof(username) - 1);
        }
    }
    
    /* Build response */
    json_value_t* response = json_create_object();
    json_object_set(response, "library", json_create_string(library));
    
    /* Add available libraries for this user */
    json_value_t* available = json_create_array();
    
    /* Always have access to default library */
    json_array_append(available, json_create_string("default"));
    
    /* User's personal library */
    if (strlen(username) > 0) {
        json_array_append(available, json_create_string(username));
    }
    
    /* TODO: Query user's library permissions from RBAC */
    
    json_object_set(response, "available_libraries", available);
    
    jwt_free(jwt);
    
    /* Wrap in standard envelope */
    json_value_t* envelope = json_create_object();
    json_object_set(envelope, "data", response);
    
    char* response_str = json_stringify(envelope);
    json_free(envelope);
    
    return create_http_response(HTTP_OK, response_str, "application/json");
}

/**
 * Switch library context
 * Changes the active library for the user's session
 */
http_response_t* api_handle_switch_library(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !ctx->db || !request) {
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Extract target library from path */
    const char* path = request->path;
    const char* lib_prefix = "/api/auth/library/";
    if (strncmp(path, lib_prefix, strlen(lib_prefix)) != 0) {
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Invalid library path\"}", "application/json");
    }
    
    const char* target_library = path + strlen(lib_prefix);
    if (!target_library || strlen(target_library) == 0) {
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Library name required\"}", "application/json");
    }
    
    /* Extract token */
    char* token = api_extract_token(request);
    if (!token) {
        return create_http_response(HTTP_UNAUTHORIZED,
                     "{\"error\":\"No authorization token\"}", "application/json");
    }
    
    /* Decode JWT */
    jwt_token_t* jwt = jwt_decode(token);
    if (!jwt || !jwt->payload) {
        BUFFER_FREE(token);
        if (jwt) jwt_free(jwt);
        return create_http_response(HTTP_UNAUTHORIZED,
                     "{\"error\":\"Invalid token\"}", "application/json");
    }
    
    /* Extract user info */
    char username[256] = {0};
    const char* user_uuid = jwt->payload->sub;
    
    if (jwt->payload->claims) {
        json_value_t* username_val = json_object_get(jwt->payload->claims, "username");
        if (username_val && username_val->type == JSON_STRING) {
            strncpy(username, username_val->value.string, sizeof(username) - 1);
        }
    }
    
    /* Check if user has access to target library */
    int has_access = 0;
    
    /* Users always have access to default library */
    if (strcmp(target_library, "default") == 0) {
        has_access = 1;
    }
    /* Users have access to their personal library */
    else if (strlen(username) > 0 && strcmp(target_library, username) == 0) {
        has_access = 1;
    }
    /* Check permissions for other libraries */
    else {
        /* Check RBAC permissions for library access */
        if (ctx->rbac && user_uuid) {
            /* Check if user has read permission on library as a collection */
            if (rbac_check_permission(ctx->rbac, user_uuid, RBAC_COLLECTION, target_library, RBAC_READ)) {
                has_access = 1;
            }
        }
        
        /* Also check if library exists */
        if (has_access) {
            /* Use virtual layer to query libraries - single source of truth */
            json_value_t* filters = json_create_object();
            json_object_set(filters, "name", json_create_string(target_library));
            json_value_t* results = virtual_query(ctx->db, "library", "system", "libraries", filters);
            json_free(filters);
            
            if (results) {
                json_value_t* documents = json_object_get(results, "documents");
                if (!documents || documents->type != JSON_ARRAY || json_array_size(documents) == 0) {
                    has_access = 0;  /* Library doesn't exist */
                }
                json_free(results);
            } else {
                has_access = 0;  /* Query failed */
            }
        }
    }
    
    if (!has_access) {
        jwt_free(jwt);
        BUFFER_FREE(token);
        return create_http_response(HTTP_FORBIDDEN,
                     "{\"error\":\"Access denied to library\"}", "application/json");
    }
    
    /* Update JWT with new library context */
    if (jwt->payload->claims) {
        json_object_set(jwt->payload->claims, "library", json_create_string(target_library));
    } else {
        jwt->payload->claims = json_create_object();
        json_object_set(jwt->payload->claims, "library", json_create_string(target_library));
    }
    
    /* Generate new token */
    char* new_token = jwt_encode(jwt, ctx->jwt_secret);
    jwt_free(jwt);
    BUFFER_FREE(token);
    
    if (!new_token) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                     "{\"error\":\"Failed to generate new token\"}", "application/json");
    }
    
    /* Update session document with new library context */
    /* TODO: Update session document in database with new library */
    
    /* Build response */
    json_value_t* response = json_create_object();
    json_object_set(response, "token", json_create_string(new_token));
    json_object_set(response, "library", json_create_string(target_library));
    json_object_set(response, "message", json_create_string("Library context switched successfully"));
    
    BUFFER_FREE(new_token);
    
    /* Wrap in standard envelope */
    json_value_t* envelope = json_create_object();
    json_object_set(envelope, "data", response);
    
    char* response_str = json_stringify(envelope);
    json_free(envelope);
    
    return create_http_response(HTTP_OK, response_str, "application/json");
}

/**
 * Change user password
 * PUT /api/auth/password
 */
http_response_t* api_handle_change_password(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !ctx->db || !request || !request->body) {
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Invalid request - body required\"}", "application/json");
    }
    
    /* Extract token */
    char* token = api_extract_token(request);
    if (!token) {
        return create_http_response(HTTP_UNAUTHORIZED,
                     "{\"error\":\"No authorization token\"}", "application/json");
    }
    
    /* Decode JWT */
    jwt_token_t* jwt = jwt_decode(token);
    BUFFER_FREE(token);
    
    if (!jwt || !jwt->payload || !jwt->payload->sub) {
        if (jwt) jwt_free(jwt);
        return create_http_response(HTTP_UNAUTHORIZED,
                     "{\"error\":\"Invalid token\"}", "application/json");
    }
    
    const char* user_uuid = jwt->payload->sub;
    
    /* Extract username from JWT claims */
    char username[256] = {0};
    if (jwt->payload->claims) {
        json_value_t* username_val = json_object_get(jwt->payload->claims, "username");
        if (username_val && username_val->type == JSON_STRING) {
            strncpy(username, username_val->value.string, sizeof(username) - 1);
        }
    }
    
    /* Parse request body */
    json_value_t* body = json_parse(request->body);
    if (!body || body->type != JSON_OBJECT) {
        jwt_free(jwt);
        if (body) json_free(body);
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Invalid JSON body\"}", "application/json");
    }
    
    /* Extract current and new passwords */
    json_value_t* current_password_val = json_object_get(body, "current_password");
    json_value_t* new_password_val = json_object_get(body, "new_password");
    
    if (!current_password_val || current_password_val->type != JSON_STRING ||
        !new_password_val || new_password_val->type != JSON_STRING) {
        jwt_free(jwt);
        json_free(body);
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Current password and new password required\"}", "application/json");
    }
    
    const char* current_password = current_password_val->value.string;
    const char* new_password = new_password_val->value.string;
    
    /* Validate new password length */
    if (strlen(new_password) < 12) {
        jwt_free(jwt);
        json_free(body);
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"New password must be at least 12 characters\"}", "application/json");
    }
    
    /* Get user document to verify current password */
    json_value_t* user_doc = virtual_get(ctx->db, user_uuid);
    if (!user_doc) {
        jwt_free(jwt);
        json_free(body);
        return create_http_response(HTTP_NOT_FOUND,
                     "{\"error\":\"User not found\"}", "application/json");
    }
    
    /* Verify current password */
    json_value_t* stored_password_hash = json_object_get(user_doc, "password_hash");
    if (!stored_password_hash || stored_password_hash->type != JSON_STRING) {
        jwt_free(jwt);
        json_free(body);
        json_free(user_doc);
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                     "{\"error\":\"User password data corrupted\"}", "application/json");
    }
    
    /* Verify current password using PBKDF2 */
    if (!verify_password(current_password, stored_password_hash->value.string)) {
        jwt_free(jwt);
        json_free(body);
        json_free(user_doc);
        return create_http_response(HTTP_UNAUTHORIZED,
                     "{\"error\":\"Current password is incorrect\"}", "application/json");
    }
    
    /* Hash new password using PBKDF2 */
    char* new_password_hash = hash_password(new_password);
    if (!new_password_hash) {
        jwt_free(jwt);
        json_free(body);
        json_free(user_doc);
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                     "{\"error\":\"Failed to hash new password\"}", "application/json");
    }
    
    /* Update user document with new password hash */
    json_value_t* update_doc = json_create_object();
    json_object_set(update_doc, "password_hash", json_create_string(new_password_hash));
    
    /* Add modified timestamp */
    time_t now = time(NULL);
    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "%ld", now);
    json_object_set(update_doc, "modified_at", json_create_string(timestamp));
    
    /* Preserve required fields from existing document */
    json_value_t* owner_val = json_object_get(user_doc, "owner");
    if (owner_val && owner_val->type == JSON_STRING) {
        json_object_set(update_doc, "owner", json_create_string(owner_val->value.string));
    }
    
    json_value_t* type_val = json_object_get(user_doc, "type");
    if (type_val && type_val->type == JSON_STRING) {
        json_object_set(update_doc, "type", json_create_string(type_val->value.string));
    }
    
    json_value_t* library_val = json_object_get(user_doc, "library");
    if (library_val && library_val->type == JSON_STRING) {
        json_object_set(update_doc, "library", json_create_string(library_val->value.string));
    }
    
    /* Update the user document using virtual layer */
    json_value_t* updated_doc = virtual_update(ctx->db, user_uuid, update_doc);
    
    BUFFER_FREE(new_password_hash);
    json_free(update_doc);
    json_free(user_doc);
    jwt_free(jwt);
    json_free(body);
    
    if (!updated_doc) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                     "{\"error\":\"Failed to update password\"}", "application/json");
    }
    
    json_free(updated_doc);
    
    /* Build success response */
    json_value_t* response = json_create_object();
    json_object_set(response, "message", json_create_string("Password changed successfully"));
    json_object_set(response, "username", json_create_string(username));
    json_object_set(response, "changed_at", json_create_string(timestamp));
    
    /* Wrap in standard envelope */
    json_value_t* envelope = json_create_object();
    json_object_set(envelope, "data", response);
    
    char* response_str = json_stringify(envelope);
    json_free(envelope);
    
    return create_http_response(HTTP_OK, response_str, "application/json");
}

/**
 * Terminate a specific session
 * DELETE /api/sessions/:id
 */
http_response_t* api_handle_terminate_session(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !ctx->db || !request) {
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Extract session ID from path */
    const char* path = request->path;
    const char* session_prefix = "/api/sessions/";
    if (strncmp(path, session_prefix, strlen(session_prefix)) != 0) {
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Invalid session path\"}", "application/json");
    }
    
    const char* session_id = path + strlen(session_prefix);
    if (!session_id || strlen(session_id) == 0) {
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Session ID required\"}", "application/json");
    }
    
    /* Extract token */
    char* token = api_extract_token(request);
    if (!token) {
        return create_http_response(HTTP_UNAUTHORIZED,
                     "{\"error\":\"No authorization token\"}", "application/json");
    }
    
    /* Decode JWT */
    jwt_token_t* jwt = jwt_decode(token);
    BUFFER_FREE(token);
    
    if (!jwt || !jwt->payload || !jwt->payload->sub) {
        if (jwt) jwt_free(jwt);
        return create_http_response(HTTP_UNAUTHORIZED,
                     "{\"error\":\"Invalid token\"}", "application/json");
    }
    
    const char* user_uuid = jwt->payload->sub;
    
    /* Get the session using virtual layer - single source of truth */
    json_value_t* session_doc = virtual_get(ctx->db, session_id);
    
    if (!session_doc) {
        jwt_free(jwt);
        return create_http_response(HTTP_NOT_FOUND,
                     "{\"error\":\"Session not found\"}", "application/json");
    }
    
    json_value_t* session_user = json_object_get(session_doc, "user_id");
    
    /* Check if user owns this session or is admin */
    int can_terminate = 0;
    if (session_user && session_user->type == JSON_STRING) {
        if (strcmp(session_user->value.string, user_uuid) == 0) {
            can_terminate = 1;
        } else {
            /* Check if user is admin */
            if (ctx->rbac && rbac_check_permission(ctx->rbac, user_uuid, RBAC_DOCUMENT, session_id, RBAC_DELETE)) {
                can_terminate = 1;
            }
        }
    }
    
    json_free(session_doc);
    jwt_free(jwt);
    
    if (!can_terminate) {
        return create_http_response(HTTP_FORBIDDEN,
                     "{\"error\":\"Permission denied\"}", "application/json");
    }
    
    /* Delete the session using virtual layer - single source of truth */
    int result = virtual_delete(ctx->db, session_id);
    if (result == 0) {  /* virtual_delete returns 1 for success, 0 for failure */
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                     "{\"error\":\"Failed to terminate session\"}", "application/json");
    }
    
    /* Build response */
    json_value_t* response = json_create_object();
    json_object_set(response, "message", json_create_string("Session terminated successfully"));
    json_object_set(response, "session_id", json_create_string(session_id));
    
    /* Wrap in standard envelope */
    json_value_t* envelope = json_create_object();
    json_object_set(envelope, "data", response);
    
    char* response_str = json_stringify(envelope);
    json_free(envelope);
    
    return create_http_response(HTTP_OK, response_str, "application/json");
}