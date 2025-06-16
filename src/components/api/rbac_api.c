#include "api/api.h"
#include "api/rbac_api.h"
#include "rbac/rbac.h"
#include "rbac/rbac_db.h"
#include "rbac/rbac_permissions.h"
#include "rbac/jwt.h"
#include "utils/json.h"
#include "utils/logger.h"
#include "utils/buffer_pool.h"
#include "database/document_storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Helper function to create a JSON error response */
static http_response_t* create_error_response(const char* message, int status_code) {
  json_value_t* error = json_create_object();
  json_object_set(error, "error", json_create_string(message));
  
  char* error_str = json_stringify(error);
  http_response_t* response = (http_response_t*)BUFFER_ALLOC(sizeof(http_response_t));
  if (!response) {
    BUFFER_FREE(error_str);
    json_free(error);
    return NULL;
  }
  
  /* Initialize response */
  response->status = (http_status_t)status_code;
  response->body = error_str;
  response->content_type = BUFFER_STRDUP("application/json");
  response->content_length = strlen(error_str);
  response->headers = NULL;
  response->num_headers = 0;
  
  json_free(error);
  
  return response;
}

/* Helper function to extract a parameter from the URL path by name */
static char* extract_path_parameter(const char* path, const char* param_name) {
  if (!path || !param_name) {
    return NULL;
  }
  
  /* Handle different parameter extraction patterns */
  
  /* Pattern: /api/rbac/roles/:id/users/:user_id */
  if (strstr(path, "/users/") && strcmp(param_name, "user_id") == 0) {
    /* Extract user_id: everything after "/users/" */
    const char* users_pos = strstr(path, "/users/");
    if (users_pos) {
      const char* user_id_start = users_pos + 7; /* length of "/users/" */
      size_t user_id_len = strlen(user_id_start);
      
      /* Check for additional path segments */
      const char* next_slash = strchr(user_id_start, '/');
      if (next_slash) {
        user_id_len = next_slash - user_id_start;
      }
      
      if (user_id_len > 0) {
        char* user_id = (char*)BUFFER_ALLOC(user_id_len + 1);
        if (user_id) {
          strncpy(user_id, user_id_start, user_id_len);
          user_id[user_id_len] = '\0';
          return user_id;
        }
      }
    }
    return NULL;
  }
  
  /* Pattern: /api/rbac/roles/:id (role ID) or /api/rbac/users/:id (user ID) */
  if (strcmp(param_name, "id") == 0) {
    /* For role ID in /api/rbac/roles/:id/users/:user_id pattern */
    if (strstr(path, "/users/")) {
      /* Extract role ID: between "/roles/" and "/users/" */
      const char* roles_pos = strstr(path, "/roles/");
      const char* users_pos = strstr(path, "/users/");
      
      if (roles_pos && users_pos && users_pos > roles_pos) {
        const char* role_id_start = roles_pos + 7; /* length of "/roles/" */
        size_t role_id_len = users_pos - role_id_start;
        
        if (role_id_len > 0) {
          char* role_id = (char*)BUFFER_ALLOC(role_id_len + 1);
          if (role_id) {
            strncpy(role_id, role_id_start, role_id_len);
            role_id[role_id_len] = '\0';
            return role_id;
          }
        }
      }
      return NULL;
    }
    
    /* For role ID in /api/rbac/roles/:id/permissions pattern */
    if (strstr(path, "/permissions")) {
      /* Extract role ID: between "/roles/" and "/permissions" */
      const char* roles_pos = strstr(path, "/roles/");
      const char* permissions_pos = strstr(path, "/permissions");
      
      if (roles_pos && permissions_pos && permissions_pos > roles_pos) {
        const char* role_id_start = roles_pos + 7; /* length of "/roles/" */
        size_t role_id_len = permissions_pos - role_id_start;
        
        if (role_id_len > 0) {
          char* role_id = (char*)BUFFER_ALLOC(role_id_len + 1);
          if (role_id) {
            strncpy(role_id, role_id_start, role_id_len);
            role_id[role_id_len] = '\0';
            return role_id;
          }
        }
      }
      return NULL;
    }
    
    /* For simple patterns: extract the last segment */
    const char* last_slash = strrchr(path, '/');
    if (!last_slash || *(last_slash + 1) == '\0') {
      return NULL;
    }
    
    const char* id_start = last_slash + 1;
    size_t id_len = strlen(id_start);
    
    /* Check for additional path segments */
    const char* next_slash = strchr(id_start, '/');
    if (next_slash) {
      id_len = next_slash - id_start;
    }
    
    if (id_len > 0) {
      char* id = (char*)BUFFER_ALLOC(id_len + 1);
      if (id) {
        strncpy(id, id_start, id_len);
        id[id_len] = '\0';
        return id;
      }
    }
  }
  
  return NULL;
}


/* Helper function to extract user ID from request */
static const char* get_request_user_id(http_request_t* request) {
  static char user_id_buffer[256]; /* Static buffer to hold the user ID */
  
  if (!request || !request->authorization) {
    TRACE_API("RBAC_API: No authorization header in request.");
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
  
  TRACE_API("RBAC_API: Extracting user_id from token.");
  
  /* Decode the JWT to get the user_id from the 'sub' claim */
  jwt_token_t* token = jwt_decode(token_str);
  if (!token) {
    TRACE_API("RBAC_API: Failed to decode JWT token.");
    return NULL;
  }
  
  /* Get the subject (user_id) from the token */
  if (!token->payload || !token->payload->sub) {
    TRACE_API("RBAC_API: No subject (user_id) in JWT token.");
    jwt_free(token);
    return NULL;
  }
  
  /* Copy the user_id to our static buffer */
  strncpy(user_id_buffer, token->payload->sub, sizeof(user_id_buffer) - 1);
  user_id_buffer[sizeof(user_id_buffer) - 1] = '\0';
  
  TRACE_API("RBAC_API: Extracted user_id from token: %s", user_id_buffer);
  
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
  http_response_t* response = (http_response_t*)BUFFER_ALLOC(sizeof(http_response_t));
  if (!response) {
    BUFFER_FREE(json_str);
    return create_error_response("Out of memory", HTTP_INTERNAL_SERVER_ERROR);
  }
  
  response->status = (http_status_t)status_code;
  response->body = json_str;
  response->content_type = BUFFER_STRDUP("application/json");
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
  TRACE_API("RBAC_API: Checking admin permission for user_id: %s", user_id ? user_id : "NULL");
  
  if (!user_id) {
    LOG_DEBUG("No user_id available for permission check");
    return create_error_response("Unauthorized", HTTP_FORBIDDEN);
  }
  
  /* Check admin permission */
  int has_permission = rbac_db_check_permission(ctx->db, user_id, RBAC_USER, "*", RBAC_ADMIN);
  TRACE_API("RBAC_API: Permission check result: %d", has_permission);
  
  if (!has_permission) {
    TRACE_API("RBAC_API: User %s does not have admin permission", user_id);
    return create_error_response("Unauthorized", HTTP_FORBIDDEN);
  }
  
  /* Query all users - filter by type="user" in unified storage */
  json_value_t* query = json_create_object();
  json_object_set(query, "type", json_create_string("user"));
  json_object_set(query, "library", json_create_string("system"));
  json_value_t* result = db_query_documents(ctx->db, STORAGE_LIBRARY, STORAGE_COLLECTION, query);
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
      
      /* Copy user fields */
      json_value_t* id = json_object_get(user, "uuid");
      json_value_t* username = json_object_get(user, "username");
      json_value_t* cn = json_object_get(user, "cn");
      json_value_t* email = json_object_get(user, "email");
      json_value_t* active = json_object_get(user, "active");
      json_value_t* created_at = json_object_get(user, "created_at");
      json_value_t* updated_at = json_object_get(user, "modified_at");
      json_value_t* roles = json_object_get(user, "roles");
      
      if (id && id->type == JSON_STRING) {
        json_object_set(sanitized, "id", json_create_string(id->value.string));
      }
      
      if (username && username->type == JSON_STRING) {
        json_object_set(sanitized, "username", json_create_string(username->value.string));
      }
      
      if (cn && cn->type == JSON_STRING) {
        json_object_set(sanitized, "cn", json_create_string(cn->value.string));
      }
      
      if (email && email->type == JSON_STRING) {
        json_object_set(sanitized, "email", json_create_string(email->value.string));
      }
      
      if (active) {
        json_object_set(sanitized, "active", json_clone(active));
      }
      
      if (created_at && created_at->type == JSON_STRING) {
        json_object_set(sanitized, "created_at", json_create_string(created_at->value.string));
      }
      
      if (updated_at && updated_at->type == JSON_STRING) {
        json_object_set(sanitized, "modified_at", json_create_string(updated_at->value.string));
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
    BUFFER_FREE(user_id);
    return create_error_response("Unauthorized", HTTP_FORBIDDEN);
  }
  
  /* Get user document */
  json_value_t* user_doc = db_get_document(ctx->db, STORAGE_LIBRARY, STORAGE_COLLECTION, user_id);
  BUFFER_FREE(user_id);
  
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
  TRACE_API("RBAC_API: Checking admin permission for user_id: %s", user_id ? user_id : "NULL");
  
  if (!user_id) {
    LOG_DEBUG("No user_id available for permission check");
    return create_error_response("Unauthorized", HTTP_FORBIDDEN);
  }
  
  /* Check admin permission */
  int has_permission = rbac_db_check_permission(ctx->db, user_id, RBAC_USER, "*", RBAC_ADMIN);
  TRACE_API("RBAC_API: Permission check result: %d", has_permission);
  
  if (!has_permission) {
    TRACE_API("RBAC_API: User %s does not have admin permission", user_id);
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
    BUFFER_FREE(user_id);
    return create_error_response("Unauthorized", HTTP_FORBIDDEN);
  }
  
  /* Parse request body */
  if (!request->body) {
    BUFFER_FREE(user_id);
    return create_error_response("Request body is empty", HTTP_BAD_REQUEST);
  }
  
  json_value_t* request_json = json_parse(request->body);
  if (!request_json || request_json->type != JSON_OBJECT) {
    if (request_json) json_free(request_json);
    BUFFER_FREE(user_id);
    return create_error_response("Invalid JSON body", HTTP_BAD_REQUEST);
  }
  
  /* Get user - first check if they exist */
  rbac_user_t* user = rbac_db_get_user(ctx->db, user_id);
  if (!user) {
    json_free(request_json);
    BUFFER_FREE(user_id);
    return create_error_response("User not found", HTTP_NOT_FOUND);
  }
  
  /* Get the actual user document for updating */
  json_value_t* user_doc = db_get_document(ctx->db, STORAGE_LIBRARY, STORAGE_COLLECTION, user->id);
  if (!user_doc) {
    rbac_free_user(user);
    json_free(request_json);
    BUFFER_FREE(user_id);
    return create_error_response("User document not found", HTTP_INTERNAL_SERVER_ERROR);
  }
  
  /* Store the actual document ID */
  char* actual_doc_id = BUFFER_STRDUP(user->id);
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
      BUFFER_FREE(password_hash);
      updated = 1;
    }
  }
  
  json_free(request_json);
  
  if (!updated) {
    json_free(user_doc);
    BUFFER_FREE(user_id);
    BUFFER_FREE(actual_doc_id);
    return create_error_response("No fields to update", HTTP_BAD_REQUEST);
  }
  
  /* Update user document using actual document ID */
  json_value_t* result = storage_update_document(ctx->db, actual_doc_id, user_doc);
  if (!result) {
    json_free(user_doc);
    BUFFER_FREE(user_id);
    BUFFER_FREE(actual_doc_id);
    return create_error_response("Failed to update user", HTTP_INTERNAL_SERVER_ERROR);
  }
  
  BUFFER_FREE(actual_doc_id);
  
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
  BUFFER_FREE(user_id);
  
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
    BUFFER_FREE(user_id);
    return create_error_response("Cannot delete your own account", HTTP_BAD_REQUEST);
  }
  
  /* Delete user */
  int result = rbac_db_delete_user(ctx->db, user_id);
  BUFFER_FREE(user_id);
  
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
  TRACE_API("RBAC_API: GET /api/rbac/roles - checking permission for user: %s", user_id ? user_id : "NULL");
  
  if (!user_id) {
    LOG_DEBUG("No user_id available for permission check");
    return create_error_response("Unauthorized", HTTP_FORBIDDEN);
  }
  
  /* Check read permission on roles */
  int has_permission = rbac_db_check_permission(ctx->db, user_id, RBAC_ROLE, "*", RBAC_READ);
  TRACE_API("RBAC_API: Role permission check result: %d", has_permission);
  
  if (!has_permission) {
    TRACE_API("RBAC_API: User %s does not have role read permission", user_id);
    return create_error_response("Unauthorized", HTTP_FORBIDDEN);
  }
  
  /* Query all roles - filter by type="role" in unified storage */
  json_value_t* query = json_create_object();
  json_object_set(query, "type", json_create_string("role"));
  json_object_set(query, "library", json_create_string("system"));
  json_value_t* result = db_query_documents(ctx->db, STORAGE_LIBRARY, STORAGE_COLLECTION, query);
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
  
  /* Query all users to compute role membership */
  json_value_t* users_query = json_create_object();
  json_object_set(users_query, "type", json_create_string("user"));
  json_object_set(users_query, "library", json_create_string("system"));
  json_value_t* users_result = db_query_documents(ctx->db, STORAGE_LIBRARY, STORAGE_COLLECTION, users_query);
  json_free(users_query);
  
  json_value_t* users_docs = NULL;
  if (users_result && users_result->type == JSON_OBJECT) {
    users_docs = json_object_get(users_result, "documents");
  }
  
  /* Create response array */
  json_value_t* roles = json_create_array();
  for (size_t i = 0; i < documents->value.array.size; i++) {
    json_value_t* role = documents->value.array.items[i];
    if (role->type == JSON_OBJECT) {
      json_value_t* role_data = json_create_object();
      
      /* Copy role fields */
      json_value_t* id = json_object_get(role, "uuid");
      json_value_t* name = json_object_get(role, "name");
      json_value_t* cn = json_object_get(role, "cn");
      json_value_t* description = json_object_get(role, "description");
      json_value_t* permissions = json_object_get(role, "permissions");
      json_value_t* created_at = json_object_get(role, "created_at");
      json_value_t* updated_at = json_object_get(role, "updated_at");
      
      if (id && id->type == JSON_STRING) {
        json_object_set(role_data, "id", json_create_string(id->value.string));
      }
      
      if (name && name->type == JSON_STRING) {
        json_object_set(role_data, "name", json_create_string(name->value.string));
      }
      
      if (cn && cn->type == JSON_STRING) {
        json_object_set(role_data, "cn", json_create_string(cn->value.string));
      }
      
      if (description && description->type == JSON_STRING) {
        json_object_set(role_data, "description", json_create_string(description->value.string));
      }
      
      if (created_at && created_at->type == JSON_STRING) {
        json_object_set(role_data, "created_at", json_create_string(created_at->value.string));
      }
      
      if (updated_at && updated_at->type == JSON_STRING) {
        json_object_set(role_data, "updated_at", json_create_string(updated_at->value.string));
      }
      
      if (permissions && permissions->type == JSON_OBJECT) {
        json_object_set(role_data, "permissions", json_clone(permissions));
      } else {
        json_object_set(role_data, "permissions", json_create_object());
      }
      
      /* Compute users array by checking which users have this role */
      json_value_t* computed_users = json_create_array();
      if (id && id->type == JSON_STRING && users_docs && users_docs->type == JSON_ARRAY) {
        const char* role_id = id->value.string;
        
        /* Check each user to see if they have this role */
        for (size_t j = 0; j < users_docs->value.array.size; j++) {
          json_value_t* user = users_docs->value.array.items[j];
          if (user && user->type == JSON_OBJECT) {
            json_value_t* user_roles = json_object_get(user, "roles");
            if (user_roles && user_roles->type == JSON_ARRAY) {
              /* Check if user has this role */
              for (size_t k = 0; k < user_roles->value.array.size; k++) {
                json_value_t* user_role = user_roles->value.array.items[k];
                if (user_role && user_role->type == JSON_STRING &&
                  strcmp(user_role->value.string, role_id) == 0) {
                  /* User has this role, add user ID to array */
                  json_value_t* user_id = json_object_get(user, "uuid");
                  if (user_id && user_id->type == JSON_STRING) {
                    json_array_append(computed_users, json_create_string(user_id->value.string));
                  }
                  break;
                }
              }
            }
          }
        }
      }
      json_object_set(role_data, "users", computed_users);
      
      json_array_append(roles, role_data);
    }
  }
  
  json_free(result);
  if (users_result) {
    json_free(users_result);
  }
  
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
  json_value_t* role_doc = db_get_document(ctx->db, STORAGE_LIBRARY, STORAGE_COLLECTION, role_id);
  BUFFER_FREE(role_id);
  
  if (!role_doc) {
    return create_error_response("Role not found", HTTP_NOT_FOUND);
  }
  
  /* Create response */
  json_value_t* role_data = json_create_object();
  
  /* Copy id and name */
  json_value_t* id = json_object_get(role_doc, "id");
  json_value_t* name = json_object_get(role_doc, "name");
  json_value_t* permissions = json_object_get(role_doc, "permissions");
  json_value_t* users = json_object_get(role_doc, "users");
  
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
  
  if (users && users->type == JSON_ARRAY) {
    json_object_set(role_data, "users", json_clone(users));
  } else {
    json_object_set(role_data, "users", json_create_array());
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
  BUFFER_FREE(role_id);
  
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
    BUFFER_FREE(role_id);
    return create_error_response("Request body required", HTTP_BAD_REQUEST);
  }
  
  json_value_t* body = json_parse(request->body);
  if (!body || body->type != JSON_OBJECT) {
    BUFFER_FREE(role_id);
    if (body) json_free(body);
    return create_error_response("Invalid JSON body", HTTP_BAD_REQUEST);
  }
  
  /* Check if role exists first */
  rbac_role_t* existing_role_obj = rbac_db_get_role(ctx->db, role_id);
  if (!existing_role_obj) {
    BUFFER_FREE(role_id);
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
  BUFFER_FREE(role_id);
  
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
    if (role_id) BUFFER_FREE(role_id);
    if (user_id) BUFFER_FREE(user_id);
    return create_error_response("Role ID and User ID are required", HTTP_BAD_REQUEST);
  }
  
  /* Add user to role */
  int result = rbac_db_add_user_to_role(ctx->db, user_id, role_id);
  BUFFER_FREE(role_id);
  BUFFER_FREE(user_id);
  
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
    if (role_id) BUFFER_FREE(role_id);
    if (user_id) BUFFER_FREE(user_id);
    return create_error_response("Role ID and User ID are required", HTTP_BAD_REQUEST);
  }
  
  /* Remove user from role */
  int result = rbac_db_remove_user_from_role(ctx->db, user_id, role_id);
  BUFFER_FREE(role_id);
  BUFFER_FREE(user_id);
  
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
    BUFFER_FREE(role_id);
    return create_error_response("Request body is empty", HTTP_BAD_REQUEST);
  }
  
  json_value_t* request_json = json_parse(request->body);
  if (!request_json || request_json->type != JSON_OBJECT) {
    if (request_json) json_free(request_json);
    BUFFER_FREE(role_id);
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
    BUFFER_FREE(role_id);
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
    BUFFER_FREE(role_id);
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
    BUFFER_FREE(role_id);
    return create_error_response("Invalid permission", HTTP_BAD_REQUEST);
  }
  
  const char* resource_id = resource_id_val->value.string;
  
  /* Get role to ensure it exists and get actual document ID */
  rbac_role_t* role = rbac_db_get_role(ctx->db, role_id);
  if (!role) {
    json_free(request_json);
    BUFFER_FREE(role_id);
    return create_error_response("Role not found", HTTP_NOT_FOUND);
  }
  
  /* Store the actual document ID */
  char* actual_role_id = BUFFER_STRDUP(role->id);
  rbac_free_role(role);
  
  /* Grant permission using actual document ID */
  int result = rbac_db_grant_permission(ctx->db, actual_role_id, resource_type, resource_id, permission);
  json_free(request_json);
  BUFFER_FREE(role_id);
  BUFFER_FREE(actual_role_id);
  
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
    BUFFER_FREE(role_id);
    return create_error_response("Request body is empty", HTTP_BAD_REQUEST);
  }
  
  json_value_t* request_json = json_parse(request->body);
  if (!request_json || request_json->type != JSON_OBJECT) {
    if (request_json) json_free(request_json);
    BUFFER_FREE(role_id);
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
    BUFFER_FREE(role_id);
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
    BUFFER_FREE(role_id);
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
    BUFFER_FREE(role_id);
    return create_error_response("Invalid permission", HTTP_BAD_REQUEST);
  }
  
  const char* resource_id = resource_id_val->value.string;
  
  /* Get role to ensure it exists and get actual document ID */
  rbac_role_t* role = rbac_db_get_role(ctx->db, role_id);
  if (!role) {
    json_free(request_json);
    BUFFER_FREE(role_id);
    return create_error_response("Role not found", HTTP_NOT_FOUND);
  }
  
  /* Store the actual document ID */
  char* actual_role_id = BUFFER_STRDUP(role->id);
  rbac_free_role(role);
  
  /* Revoke permission using actual document ID */
  int result = rbac_db_revoke_permission(ctx->db, actual_role_id, resource_type, resource_id, permission);
  json_free(request_json);
  BUFFER_FREE(role_id);
  BUFFER_FREE(actual_role_id);
  
  if (!result) {
    return create_error_response("Failed to revoke permission", HTTP_INTERNAL_SERVER_ERROR);
  }
  
  /* Create response */
  json_value_t* response_json = json_create_object();
  json_object_set(response_json, "success", json_create_boolean(1));
  
  return create_json_response(response_json, HTTP_OK);
}

/**
 * Get all permissions in the system
 * 
 * GET /api/rbac/permissions
 * 
 * Returns a permission matrix showing all roles and their permissions
 */
http_response_t* api_handle_rbac_get_permissions(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !ctx->db || !ctx->rbac) {
    return create_error_response("RBAC not initialized", HTTP_INTERNAL_SERVER_ERROR);
  }
  
  /* Check if user has permission to view permissions */
  const char* user_id = get_request_user_id(request);
  if (!user_id) {
    return create_error_response("Unauthorized", HTTP_FORBIDDEN);
  }
  
  /* Check if user can read permissions */
  int has_permission = rbac_db_check_permission(ctx->db, user_id, RBAC_PERMISSION, "*", RBAC_READ);
  if (!has_permission) {
    return create_error_response("Unauthorized", HTTP_FORBIDDEN);
  }
  
  /* Build permission matrix */
  json_value_t* response_json = json_create_object();
  
  /* Get all roles */
  json_value_t* query = json_create_object();
  json_object_set(query, "type", json_create_string("role"));
  json_object_set(query, "library", json_create_string("system"));
  json_value_t* roles_result = db_query_documents(ctx->db, STORAGE_LIBRARY, STORAGE_COLLECTION, query);
  json_free(query);
  
  if (!roles_result) {
    json_free(response_json);
    return create_error_response("Failed to query roles", HTTP_INTERNAL_SERVER_ERROR);
  }
  
  json_value_t* roles_docs = json_object_get(roles_result, "documents");
  if (!roles_docs || roles_docs->type != JSON_ARRAY) {
    json_free(roles_result);
    json_free(response_json);
    return create_error_response("Invalid roles data", HTTP_INTERNAL_SERVER_ERROR);
  }
  
  /* Build roles array with permissions */
  json_value_t* roles_array = json_create_array();
  
  for (size_t i = 0; i < json_array_size(roles_docs); i++) {
    json_value_t* role_doc = json_array_get(roles_docs, i);
    json_value_t* role_obj = json_create_object();
    
    /* Copy role basic info */
    json_value_t* id = json_object_get(role_doc, "uuid");
    json_value_t* name = json_object_get(role_doc, "name");
    json_value_t* description = json_object_get(role_doc, "description");
    json_value_t* permissions = json_object_get(role_doc, "permissions");
    
    if (id) json_object_set(role_obj, "id", json_clone(id));
    if (name) json_object_set(role_obj, "name", json_clone(name));
    if (description) json_object_set(role_obj, "description", json_clone(description));
    if (permissions) json_object_set(role_obj, "permissions", json_clone(permissions));
    
    json_array_append(roles_array, role_obj);
  }
  
  json_free(roles_result);
  
  /* Add resource types for reference */
  json_value_t* resource_types = json_create_object();
  json_object_set(resource_types, "0", json_create_string("database"));
  json_object_set(resource_types, "1", json_create_string("collection"));
  json_object_set(resource_types, "2", json_create_string("document"));
  json_object_set(resource_types, "3", json_create_string("user"));
  json_object_set(resource_types, "4", json_create_string("role"));
  json_object_set(resource_types, "5", json_create_string("permission"));
  
  /* Add permission types for reference */
  json_value_t* permission_types = json_create_object();
  json_object_set(permission_types, "1", json_create_string("READ"));
  json_object_set(permission_types, "2", json_create_string("WRITE"));
  json_object_set(permission_types, "4", json_create_string("DELETE"));
  json_object_set(permission_types, "8", json_create_string("ADMIN"));
  
  json_object_set(response_json, "roles", roles_array);
  json_object_set(response_json, "resource_types", resource_types);
  json_object_set(response_json, "permission_types", permission_types);
  
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
  api_routes[num_routes++] = (api_route_t){"/api/rbac/permissions", HTTP_GET, api_handle_rbac_get_permissions, 1};
  
  return num_routes;
}