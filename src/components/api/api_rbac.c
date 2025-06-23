/**
 * @file api_rbac.c
 * @brief RBAC (Role-Based Access Control) API handlers implementation
 * 
 * Consolidates all user and role management API endpoints. Implements 
 * comprehensive user lifecycle management, role administration, and permission 
 * system integration with unified documents architecture and checkpoint-based
 * memory management.
 * 
 * Extracted from api.c as part of Phase 2.4 API module decomposition.
 */

#include "api/api_rbac.h"
#include "api/api.h"
#include "database/document_storage.h"
#include "database/virtual_layer.h"
#include "core/server.h"
#include "database/database.h"
#include "rbac/rbac.h"
#include "rbac/rbac_db.h"
#include "rbac/jwt.h"
#include "utils/json.h"
#include "utils/logger.h"
#include "utils/buffer_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Helper function to get string value from JSON object */
static const char* json_object_get_string(json_value_t* object, const char* key) {
  if (!object || object->type != JSON_OBJECT || !key) {
    return NULL;
  }
  
  json_value_t* value = json_object_get(object, key);
  if (!value || value->type != JSON_STRING) {
    return NULL;
  }
  
  return value->value.string;
}

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
  BUFFER_FREE(token);
  
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
  
  /* SINGLE SOURCE OF TRUTH: Query users using virtual layer */
  json_value_t* users_query = json_create_object();
  json_object_set(users_query, "type", json_create_string(DOC_TYPE_NAME_USER));
  json_object_set(users_query, "library", json_create_string("system"));
  
  json_value_t* users_results = virtual_query(ctx->db, DOC_TYPE_NAME_USER, "system", VIRTUAL_COLLECTION_USERS, users_query);
  /* CHECKPOINT: json_free(users_query); */
  
  if (!users_results) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to query users\"}", "application/json");
  }
  
  json_value_t* users_docs = json_object_get(users_results, "documents");
  if (!users_docs || users_docs->type != JSON_ARRAY) {
    /* CHECKPOINT: json_free(users_results); */
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Invalid users response\"}", "application/json");
  }
  
  /* Create a JSON array of users */
  json_value_t* users_array = json_create_array();
  if (!users_array) {
    /* CHECKPOINT: json_free(users_results); */
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to create users array\"}", "application/json");
  }
  
  /* Process each user document */
  for (size_t i = 0; i < json_array_size(users_docs); i++) {
    json_value_t* user_doc = json_array_get(users_docs, i);
    if (user_doc && user_doc->type == JSON_OBJECT) {
      /* Create a new user object with a subset of information (exclude password hash) */
      json_value_t* user = json_create_object();
      
      /* Add user ID from uuid field */
      json_value_t* uuid = json_object_get(user_doc, "uuid");
      if (uuid && uuid->type == JSON_STRING) {
        json_object_set(user, "id", json_create_string(uuid->value.string));
      }
      
      /* Add username if present */
      json_value_t* username = json_object_get(user_doc, "username");
      if (username && username->type == JSON_STRING) {
        json_object_set(user, "username", json_create_string(username->value.string));
      }
      
      /* Add roles array if present */
      json_value_t* roles = json_object_get(user_doc, "roles");
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
  
  /* CHECKPOINT: json_free(users_results); */
  
  /* Create response object */
  json_value_t* response = json_create_object();
  json_object_set(response, "users", users_array);
  
  /* Serialize response */
  char* response_str = json_stringify(response);
  
  /* Free resources */
  /* CHECKPOINT: json_free(response); */
  
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
  BUFFER_FREE(token);
  
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
  /* CHECKPOINT: json_free(response); */
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
  BUFFER_FREE(token);
  
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
    if (body) {

        /* CHECKPOINT: json_free(body); */

    }
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request body\"}", "application/json");
  }
  
  /* Extract username and password */
  json_value_t* username_val = json_object_get(body, "username");
  json_value_t* password_val = json_object_get(body, "password");
  json_value_t* roles_val = json_object_get(body, "roles");
  
  if (!username_val || username_val->type != JSON_STRING || 
    !password_val || password_val->type != JSON_STRING) {
    /* CHECKPOINT: json_free(body); */
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Username and password are required\"}", "application/json");
  }
  
  const char* username = username_val->value.string;
  const char* password = password_val->value.string;
  
  /* Validate input */
  if (strlen(username) < 3) {
    /* CHECKPOINT: json_free(body); */
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Username must be at least 3 characters long\"}", "application/json");
  }
  
  if (strlen(password) < 8) {
    /* CHECKPOINT: json_free(body); */
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Password must be at least 8 characters long\"}", "application/json");
  }
  
  /* Check if username already exists */
  if (rbac_get_user_by_username(ctx->rbac, username)) {
    /* CHECKPOINT: json_free(body); */
    return create_http_response(HTTP_CONFLICT, 
                 "{\"error\":\"Username already exists\"}", "application/json");
  }
  
  /* Create user */
  rbac_user_t* user = rbac_create_user(ctx->rbac, username, password);
  if (!user) {
    /* CHECKPOINT: json_free(body); */
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
  /* CHECKPOINT: json_free(response); */
  /* CHECKPOINT: json_free(body); */
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
  BUFFER_FREE(token);
  
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
    if (body) {

        /* CHECKPOINT: json_free(body); */

    }
    jwt_free(jwt);
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request body\"}", "application/json");
  }
  
  /* Check if user exists */
  rbac_user_t* user = rbac_get_user(ctx->rbac, target_user_id);
  if (!user) {
    /* CHECKPOINT: json_free(body); */
    jwt_free(jwt);
    return create_http_response(HTTP_NOT_FOUND, 
                 "{\"error\":\"User not found\"}", "application/json");
  }
  
  /* Extract fields to update */
  json_value_t* username_val = json_object_get(body, "username");
  json_value_t* password_val = json_object_get(body, "password");
  json_value_t* roles_val = json_object_get(body, "roles");
  
  /* Get the actual JSON user object using virtual layer */
  json_value_t* user_query = json_create_object();
  json_object_set(user_query, "type", json_create_string(DOC_TYPE_NAME_USER));
  json_object_set(user_query, "uuid", json_create_string(target_user_id));
  json_object_set(user_query, "library", json_create_string("system"));
  
  json_value_t* user_result = virtual_query(ctx->db, DOC_TYPE_NAME_USER, "system", VIRTUAL_COLLECTION_USERS, user_query);
  /* CHECKPOINT: json_free(user_query); */
  
  if (!user_result) {
    rbac_free_user(user);
    /* CHECKPOINT: json_free(body); */
    jwt_free(jwt);
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to query user from database\"}", "application/json");
  }
  
  json_value_t* user_docs = json_object_get(user_result, "documents");
  if (!user_docs || user_docs->type != JSON_ARRAY || user_docs->value.array.size == 0) {
    rbac_free_user(user);
    /* CHECKPOINT: json_free(body); */
    jwt_free(jwt);
    /* CHECKPOINT: json_free(user_result); */
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to find user in database\"}", "application/json");
  }
  
  json_value_t* user_obj = json_array_get(user_docs, 0);
  if (!user_obj || user_obj->type != JSON_OBJECT) {
    rbac_free_user(user);
    /* CHECKPOINT: json_free(body); */
    jwt_free(jwt);
    /* CHECKPOINT: json_free(user_result); */
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Invalid user document\"}", "application/json");
  }
  
  /* Update username if provided */
  if (username_val && username_val->type == JSON_STRING) {
    const char* new_username = username_val->value.string;
    
    /* Validate username */
    if (strlen(new_username) < 3) {
      rbac_free_user(user);
      /* CHECKPOINT: json_free(body); */
      jwt_free(jwt);
      return create_http_response(HTTP_BAD_REQUEST, 
                   "{\"error\":\"Username must be at least 3 characters long\"}", "application/json");
    }
    
    /* Check if username is already taken by another user */
    rbac_user_t* existing_user = rbac_get_user_by_username(ctx->rbac, new_username);
    if (existing_user && strcmp(existing_user->id, target_user_id) != 0) {
      rbac_free_user(user);
      rbac_free_user(existing_user);
      /* CHECKPOINT: json_free(body); */
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
      /* CHECKPOINT: json_free(body); */
      jwt_free(jwt);
      return create_http_response(HTTP_BAD_REQUEST, 
                   "{\"error\":\"Password must be at least 8 characters long\"}", "application/json");
    }
    
    /* Hash password */
    char* password_hash = hash_password(new_password);
    
    if (!password_hash) {
      rbac_free_user(user);
      /* CHECKPOINT: json_free(body); */
      jwt_free(jwt);
      return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                   "{\"error\":\"Failed to hash password\"}", "application/json");
    }
    
    /* Update password_hash in the JSON object */
    json_object_set(user_obj, "password_hash", json_create_string(password_hash));
    
    /* Free password hash */
    BUFFER_FREE(password_hash);
  }
  
  /* Update roles if provided - requires admin permissions */
  if (roles_val && roles_val->type == JSON_ARRAY) {
    /* Check if user has admin privileges for role management */
    if (!rbac_check_permission(ctx->rbac, requester_user_id, RBAC_ROLE, "*", RBAC_ADMIN) &&
      !rbac_check_permission(ctx->rbac, requester_user_id, RBAC_USER, "*", RBAC_ADMIN)) {
      rbac_free_user(user);
      /* CHECKPOINT: json_free(body); */
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
        
        /* Get role using virtual layer */
        json_value_t* role_query = json_create_object();
        json_object_set(role_query, "type", json_create_string(DOC_TYPE_NAME_ROLE));
        json_object_set(role_query, "uuid", json_create_string(role_id));
        json_object_set(role_query, "library", json_create_string("system"));
        
        json_value_t* role_result = virtual_query(ctx->db, DOC_TYPE_NAME_ROLE, "system", VIRTUAL_COLLECTION_ROLES, role_query);
        /* CHECKPOINT: json_free(role_query); */
        
        if (role_result) {
          json_value_t* role_docs = json_object_get(role_result, "documents");
          if (role_docs && role_docs->type == JSON_ARRAY && role_docs->value.array.size > 0) {
            json_value_t* role_obj = json_array_get(role_docs, 0);
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
                /* Update role document using virtual layer */
                const char* role_uuid = json_object_get_string(role_obj, "uuid");
                if (role_uuid) {
                  virtual_update(ctx->db, role_uuid, role_obj);
                }
              }
            }
          }
          /* CHECKPOINT: json_free(role_result); */
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
        
        /* Verify that role exists using virtual layer */
        json_value_t* role_query = json_create_object();
        json_object_set(role_query, "type", json_create_string(DOC_TYPE_NAME_ROLE));
        json_object_set(role_query, "uuid", json_create_string(role_id));
        json_object_set(role_query, "library", json_create_string("system"));
        
        json_value_t* role_result = virtual_query(ctx->db, DOC_TYPE_NAME_ROLE, "system", VIRTUAL_COLLECTION_ROLES, role_query);
        /* CHECKPOINT: json_free(role_query); */
        
        if (role_result) {
          json_value_t* role_docs = json_object_get(role_result, "documents");
          if (role_docs && role_docs->type == JSON_ARRAY && role_docs->value.array.size > 0) {
            json_value_t* role_obj = json_array_get(role_docs, 0);
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
              
              /* Update role document using virtual layer */
              const char* role_uuid = json_object_get_string(role_obj, "uuid");
              if (role_uuid) {
                virtual_update(ctx->db, role_uuid, role_obj);
              }
            }
          }
          /* CHECKPOINT: json_free(role_result); */
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
  
  /* Update the user document using virtual layer */
  const char* user_uuid = json_object_get_string(user_obj, "uuid");
  if (user_uuid) {
    virtual_update(ctx->db, user_uuid, user_obj);
  }
  
  /* Free resources */
  rbac_free_user(user);
  /* CHECKPOINT: json_free(response); */
  /* CHECKPOINT: json_free(body); */
  jwt_free(jwt);
  /* CHECKPOINT: json_free(user_result); */
  
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
  BUFFER_FREE(token);
  
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
  
  /* Check if user exists using virtual layer */
  json_value_t* user_query = json_create_object();
  json_object_set(user_query, "type", json_create_string(DOC_TYPE_NAME_USER));
  json_object_set(user_query, "uuid", json_create_string(target_user_id));
  json_object_set(user_query, "library", json_create_string("system"));
  
  json_value_t* user_result = virtual_query(ctx->db, DOC_TYPE_NAME_USER, "system", VIRTUAL_COLLECTION_USERS, user_query);
  /* CHECKPOINT: json_free(user_query); */
  
  if (!user_result) {
    return create_http_response(HTTP_NOT_FOUND, 
                 "{\"error\":\"User not found\"}", "application/json");
  }
  
  json_value_t* user_docs = json_object_get(user_result, "documents");
  if (!user_docs || user_docs->type != JSON_ARRAY || user_docs->value.array.size == 0) {
    /* CHECKPOINT: json_free(user_result); */
    return create_http_response(HTTP_NOT_FOUND, 
                 "{\"error\":\"User not found\"}", "application/json");
  }
  /* CHECKPOINT: json_free(user_result); */
  
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
  BUFFER_FREE(token);
  
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
  
  /* SINGLE SOURCE OF TRUTH: Query roles using virtual layer */
  json_value_t* roles_query = json_create_object();
  json_object_set(roles_query, "type", json_create_string(DOC_TYPE_NAME_ROLE));
  json_object_set(roles_query, "library", json_create_string("system"));
  
  json_value_t* roles_results = virtual_query(ctx->db, DOC_TYPE_NAME_ROLE, "system", VIRTUAL_COLLECTION_ROLES, roles_query);
  /* CHECKPOINT: json_free(roles_query); */
  
  if (!roles_results) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to query roles\"}", "application/json");
  }
  
  json_value_t* roles_docs = json_object_get(roles_results, "documents");
  if (!roles_docs || roles_docs->type != JSON_ARRAY) {
    /* CHECKPOINT: json_free(roles_results); */
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Invalid roles response\"}", "application/json");
  }
  
  /* Create a JSON array of roles */
  json_value_t* roles_array = json_create_array();
  if (!roles_array) {
    /* CHECKPOINT: json_free(roles_results); */
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to create roles array\"}", "application/json");
  }
  
  /* Process each role document */
  for (size_t i = 0; i < json_array_size(roles_docs); i++) {
    json_value_t* role_doc = json_array_get(roles_docs, i);
    if (role_doc && role_doc->type == JSON_OBJECT) {
      /* Create a new role object with relevant information */
      json_value_t* role = json_create_object();
      
      /* Add role ID from uuid field */
      json_value_t* uuid = json_object_get(role_doc, "uuid");
      if (uuid && uuid->type == JSON_STRING) {
        json_object_set(role, "id", json_create_string(uuid->value.string));
      }
      
      /* Add name if present */
      json_value_t* name = json_object_get(role_doc, "name");
      if (name && name->type == JSON_STRING) {
        json_object_set(role, "name", json_create_string(name->value.string));
      }
      
      /* Add users array if present */
      json_value_t* users = json_object_get(role_doc, "users");
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
      json_value_t* permissions = json_object_get(role_doc, "permissions");
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
  
  /* CHECKPOINT: json_free(roles_results); */
  
  /* Create response object */
  json_value_t* response = json_create_object();
  json_object_set(response, "roles", roles_array);
  
  /* Serialize response */
  char* response_str = json_stringify(response);
  
  /* Free resources */
  /* CHECKPOINT: json_free(response); */
  
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
  BUFFER_FREE(token);
  
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
  
  /* Get role users using virtual layer */
  json_value_t* role_query = json_create_object();
  json_object_set(role_query, "type", json_create_string(DOC_TYPE_NAME_ROLE));
  json_object_set(role_query, "uuid", json_create_string(role_id));
  json_object_set(role_query, "library", json_create_string("system"));
  
  json_value_t* role_result = virtual_query(ctx->db, DOC_TYPE_NAME_ROLE, "system", VIRTUAL_COLLECTION_ROLES, role_query);
  /* CHECKPOINT: json_free(role_query); */
  
  if (role_result) {
    json_value_t* role_docs = json_object_get(role_result, "documents");
    if (role_docs && role_docs->type == JSON_ARRAY && role_docs->value.array.size > 0) {
      json_value_t* role_json = json_array_get(role_docs, 0);
      if (role_json && role_json->type == JSON_OBJECT) {
        json_value_t* users = json_object_get(role_json, "users");
        if (users && users->type == JSON_ARRAY) {
          for (size_t i = 0; i < users->value.array.size; i++) {
            json_value_t* user_id_val = users->value.array.items[i];
            if (user_id_val && user_id_val->type == JSON_STRING) {
              /* Add user ID to array */
              json_array_append(users_array, json_create_string(user_id_val->value.string));
              
              /* Optionally, add user details from database */
              const char* user_id_str = user_id_val->value.string;
              json_value_t* user_query = json_create_object();
              json_object_set(user_query, "type", json_create_string(DOC_TYPE_NAME_USER));
              json_object_set(user_query, "uuid", json_create_string(user_id_str));
              json_object_set(user_query, "library", json_create_string("system"));
              
              json_value_t* user_result = virtual_query(ctx->db, DOC_TYPE_NAME_USER, "system", VIRTUAL_COLLECTION_USERS, user_query);
              /* CHECKPOINT: json_free(user_query); */
              
              if (user_result) {
                json_value_t* user_docs = json_object_get(user_result, "documents");
                if (user_docs && user_docs->type == JSON_ARRAY && user_docs->value.array.size > 0) {
                  json_value_t* user_json = json_array_get(user_docs, 0);
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
                /* CHECKPOINT: json_free(user_result); */
              }
            }
          }
        }
      }
    }
    /* CHECKPOINT: json_free(role_result); */
  }
  
  json_object_set(role_obj, "users", users_array);
  
  json_object_set(response, "role", role_obj);
  
  /* Serialize response */
  char* response_str = json_stringify(response);
  
  /* Free resources */
  /* CHECKPOINT: json_free(response); */
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
  BUFFER_FREE(token);
  
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
    if (body) {

        /* CHECKPOINT: json_free(body); */

    }
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request body\"}", "application/json");
  }
  
  /* Extract role name and permissions */
  json_value_t* name_val = json_object_get(body, "name");
  json_value_t* permissions_val = json_object_get(body, "permissions");
  
  if (!name_val || name_val->type != JSON_STRING) {
    /* CHECKPOINT: json_free(body); */
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Role name is required\"}", "application/json");
  }
  
  const char* name = name_val->value.string;
  
  /* Validate input */
  if (strlen(name) < 2) {
    /* CHECKPOINT: json_free(body); */
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Role name must be at least 2 characters long\"}", "application/json");
  }
  
  /* Check if role name already exists in database */
  json_value_t* role_query = json_create_object();
  json_object_set(role_query, "type", json_create_string(DOC_TYPE_NAME_ROLE));
  json_object_set(role_query, "library", json_create_string("system"));
  json_object_set(role_query, "name", json_create_string(name));
  
  json_value_t* role_result = virtual_query(ctx->db, DOC_TYPE_NAME_ROLE, "system", VIRTUAL_COLLECTION_ROLES, role_query);
  /* CHECKPOINT: json_free(role_query); */
  
  if (role_result) {
    json_value_t* role_docs = json_object_get(role_result, "documents");
    if (role_docs && role_docs->type == JSON_ARRAY && role_docs->value.array.size > 0) {
      /* CHECKPOINT: json_free(role_result); */
      /* CHECKPOINT: json_free(body); */
      return create_http_response(HTTP_CONFLICT, 
                   "{\"error\":\"Role name already exists\"}", "application/json");
    }
    /* CHECKPOINT: json_free(role_result); */
  }
  
  /* Create role */
  rbac_role_t* role = rbac_create_role(ctx->rbac, name);
  if (!role) {
    /* CHECKPOINT: json_free(body); */
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
  /* CHECKPOINT: json_free(response); */
  /* CHECKPOINT: json_free(body); */
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
  BUFFER_FREE(token);
  
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
  
  /* Check if role exists in database */
  json_value_t* role_query = json_create_object();
  json_object_set(role_query, "type", json_create_string(DOC_TYPE_NAME_ROLE));
  json_object_set(role_query, "uuid", json_create_string(role_id));
  json_object_set(role_query, "library", json_create_string("system"));
  
  json_value_t* role_result = virtual_query(ctx->db, DOC_TYPE_NAME_ROLE, "system", VIRTUAL_COLLECTION_ROLES, role_query);
  /* CHECKPOINT: json_free(role_query); */
  
  if (!role_result) {
    return create_http_response(HTTP_NOT_FOUND, 
                 "{\"error\":\"Role not found\"}", "application/json");
  }
  
  json_value_t* role_docs = json_object_get(role_result, "documents");
  if (!role_docs || role_docs->type != JSON_ARRAY || role_docs->value.array.size == 0) {
    /* CHECKPOINT: json_free(role_result); */
    return create_http_response(HTTP_NOT_FOUND, 
                 "{\"error\":\"Role not found\"}", "application/json");
  }
  
  json_value_t* role_json = json_array_get(role_docs, 0);
  if (!role_json || role_json->type != JSON_OBJECT) {
    /* CHECKPOINT: json_free(role_result); */
    return create_http_response(HTTP_NOT_FOUND, 
                 "{\"error\":\"Invalid role document\"}", "application/json");
  }
  
  /* Parse request body */
  json_value_t* body = json_parse(request->body);
  if (!body || body->type != JSON_OBJECT) {
    if (body) {

        /* CHECKPOINT: json_free(body); */

    }
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
      /* CHECKPOINT: json_free(body); */
      return create_http_response(HTTP_BAD_REQUEST, 
                   "{\"error\":\"Role name must be at least 2 characters long\"}", "application/json");
    }
    
    /* Check if name is already taken by another role using virtual layer */
    json_value_t* name_query = json_create_object();
    json_object_set(name_query, "type", json_create_string(DOC_TYPE_NAME_ROLE));
    json_object_set(name_query, "library", json_create_string("system"));
    json_object_set(name_query, "name", json_create_string(new_name));
    
    json_value_t* name_result = virtual_query(ctx->db, DOC_TYPE_NAME_ROLE, "system", VIRTUAL_COLLECTION_ROLES, name_query);
    /* CHECKPOINT: json_free(name_query); */
    
    if (name_result) {
      json_value_t* name_docs = json_object_get(name_result, "documents");
      if (name_docs && name_docs->type == JSON_ARRAY) {
        for (size_t i = 0; i < name_docs->value.array.size; i++) {
          json_value_t* existing_role = json_array_get(name_docs, i);
          if (existing_role && existing_role->type == JSON_OBJECT) {
            const char* existing_uuid = json_object_get_string(existing_role, "uuid");
            if (existing_uuid && strcmp(existing_uuid, role_id) != 0) {
              /* CHECKPOINT: json_free(name_result); */
              /* CHECKPOINT: json_free(body); */
              /* CHECKPOINT: json_free(role_result); */
              return create_http_response(HTTP_CONFLICT, 
                           "{\"error\":\"Role name already exists\"}", "application/json");
            }
          }
        }
      }
      /* CHECKPOINT: json_free(name_result); */
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
        
        /* Get user from database */
        json_value_t* user_query = json_create_object();
        json_object_set(user_query, "type", json_create_string(DOC_TYPE_NAME_USER));
        json_object_set(user_query, "uuid", json_create_string(user_id_str));
        json_object_set(user_query, "library", json_create_string("system"));
        
        json_value_t* user_result = virtual_query(ctx->db, DOC_TYPE_NAME_USER, "system", VIRTUAL_COLLECTION_USERS, user_query);
        /* CHECKPOINT: json_free(user_query); */
        
        if (user_result) {
          json_value_t* user_docs = json_object_get(user_result, "documents");
          if (user_docs && user_docs->type == JSON_ARRAY && user_docs->value.array.size > 0) {
            json_value_t* user_obj = json_array_get(user_docs, 0);
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
                /* Update user document in database */
                const char* user_uuid = json_object_get_string(user_obj, "uuid");
                if (user_uuid) {
                  virtual_update(ctx->db, user_uuid, user_obj);
                }
              }
            }
          }
          /* CHECKPOINT: json_free(user_result); */
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
        /* Check if user exists in database */
        json_value_t* user_query = json_create_object();
        json_object_set(user_query, "type", json_create_string(DOC_TYPE_NAME_USER));
        json_object_set(user_query, "uuid", json_create_string(user_id_str));
        json_object_set(user_query, "library", json_create_string("system"));
        
        json_value_t* user_result = virtual_query(ctx->db, DOC_TYPE_NAME_USER, "system", VIRTUAL_COLLECTION_USERS, user_query);
        /* CHECKPOINT: json_free(user_query); */
        
        if (user_result) {
          json_value_t* user_docs = json_object_get(user_result, "documents");
          if (user_docs && user_docs->type == JSON_ARRAY && user_docs->value.array.size > 0) {
            json_value_t* user_obj = json_array_get(user_docs, 0);
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
              
              /* Update user document in database */
              const char* user_uuid = json_object_get_string(user_obj, "uuid");
              if (user_uuid) {
                virtual_update(ctx->db, user_uuid, user_obj);
              }
            }
          }
          /* CHECKPOINT: json_free(user_result); */
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
  
  /* Get role users from the role_json we already have */
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
  
  json_object_set(role_obj, "users", users_array);
  
  json_object_set(response, "role", role_obj);
  
  /* Serialize response */
  char* response_str = json_stringify(response);
  
  /* Update the role document in the database */
  const char* role_uuid = json_object_get_string(role_json, "uuid");
  if (role_uuid) {
    virtual_update(ctx->db, role_uuid, role_json);
  }
  
  /* Free resources */
  /* CHECKPOINT: json_free(response); */
  /* CHECKPOINT: json_free(body); */
  rbac_free_role(role);
  /* CHECKPOINT: json_free(role_result); */
  
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
  BUFFER_FREE(token);
  
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
  
  /* Check if role exists in database */
  json_value_t* role_query = json_create_object();
  json_object_set(role_query, "type", json_create_string(DOC_TYPE_NAME_ROLE));
  json_object_set(role_query, "uuid", json_create_string(role_id));
  json_object_set(role_query, "library", json_create_string("system"));
  
  json_value_t* role_result = virtual_query(ctx->db, DOC_TYPE_NAME_ROLE, "system", VIRTUAL_COLLECTION_ROLES, role_query);
  /* CHECKPOINT: json_free(role_query); */
  
  if (!role_result) {
    return create_http_response(HTTP_NOT_FOUND, 
                 "{\"error\":\"Role not found\"}", "application/json");
  }
  
  json_value_t* role_docs = json_object_get(role_result, "documents");
  if (!role_docs || role_docs->type != JSON_ARRAY || role_docs->value.array.size == 0) {
    /* CHECKPOINT: json_free(role_result); */
    return create_http_response(HTTP_NOT_FOUND, 
                 "{\"error\":\"Role not found\"}", "application/json");
  }
  
  /* Special case: Don't allow deletion of the admin role */
  json_value_t* role_json = json_array_get(role_docs, 0);
  if (role_json && role_json->type == JSON_OBJECT) {
    json_value_t* name_val = json_object_get(role_json, "name");
    if (name_val && name_val->type == JSON_STRING && 
      strcmp(name_val->value.string, "admin") == 0) {
      /* CHECKPOINT: json_free(role_result); */
      return create_http_response(HTTP_FORBIDDEN, 
                   "{\"error\":\"Cannot delete the admin role\"}", "application/json");
    }
  }
  /* CHECKPOINT: json_free(role_result); */
  
  /* Delete role */
  if (!rbac_delete_role(ctx->rbac, role_id)) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to delete role\"}", "application/json");
  }
  
  /* Return success with no content */
  return create_http_response(HTTP_NO_CONTENT, NULL, "application/json");
}