/**
 * Authentication handler for user login and session management
 * Handles admin user authentication and JWT token generation
 */
#include "api/api.h"
#include "core/server.h"
#include "database/database.h"
#include "database/unified_documents.h"
#include "rbac/rbac.h"
#include "rbac/jwt.h"
#include "rbac/rbac_database.h"
#include "utils/json.h"
#include "utils/logger.h"
#include "utils/buffer_pool.h"
#include <string.h>
#include <time.h>
#include <stdlib.h>
http_response_t* api_handle_login(api_context_t* ctx, http_request_t* request) {
  LOG_DEBUG("Starting login handler.");
  
  if (!ctx || !request || !request->body) {
    LOG_ERROR("Invalid request parameters.");
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  LOG_DEBUG("Parsing request body: %s", request->body);
  
  /* Parse request body */
  json_value_t* body = json_parse(request->body);
  LOG_DEBUG("JSON parsing completed.");
  if (!body || body->type != JSON_OBJECT) {
    if (body) json_free(body);
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request body\"}", "application/json");
  }
  
  /* Extract username, password, and optional library */
  json_value_t* username_val = json_object_get(body, "username");
  json_value_t* password_val = json_object_get(body, "password");
  json_value_t* library_val = json_object_get(body, "library");
  
  if (!username_val || username_val->type != JSON_STRING || 
    !password_val || password_val->type != JSON_STRING) {
    json_free(body);
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Username and password required\"}", "application/json");
  }
  
  const char* username = username_val->value.string;
  const char* password = password_val->value.string;
  const char* library = NULL;
  
  /* Check if library is specified in request or username contains @ */
  char username_copy[256] = {0};
  if (library_val && library_val->type == JSON_STRING) {
    library = library_val->value.string;
  } else if (strchr(username, '@')) {
    /* Parse username@library format - need to copy since we'll modify it */
    strncpy(username_copy, username, sizeof(username_copy) - 1);
    char* at_pos = strchr(username_copy, '@');
    if (at_pos) {
      *at_pos = '\0';
      username = username_copy;
      library = at_pos + 1;
    }
  }
  
  TRACE_AUTH("Extracted credentials for username: %s", username);
  
  /* Check if we need to perform deferred bootstrap */
  if (ctx->db && ctx->db->is_bootstrap_mode) {
    const char* deferred_bootstrap = getenv("JDBX_DEFERRED_BOOTSTRAP");
    if (deferred_bootstrap && strcmp(deferred_bootstrap, "1") == 0) {
      LOG_INFO("Performing deferred bootstrap - creating admin user");
      
      /* Create admin role and user now that server is fully initialized */
      char* admin_role_id = NULL;
      if (create_default_admin_role(ctx->db, &admin_role_id)) {
        if (admin_role_id) {
          if (create_default_admin_user(ctx->db, admin_role_id)) {
            LOG_INFO("Deferred bootstrap completed successfully");
            /* Disable bootstrap mode */
            ctx->db->is_bootstrap_mode = 0;
            unsetenv("JDBX_DEFERRED_BOOTSTRAP");
          }
          free(admin_role_id);
        }
      }
    }
  }
  
  /* Determine target library - default to system if not specified */
  if (!library) {
    library = "system";
  }
  
  LOG_DEBUG("Attempting login for user: %s in library: %s", username, library);
  
  /* HIERARCHICAL ARCHITECTURE: Query system/users collection for user */
  json_value_t* query = json_create_object();
  json_object_set(query, "username", json_create_string(username));
  
  char* query_str = json_stringify(query);
  LOG_DEBUG("Querying for user in system/users with: %s", query_str);
  buffer_pool_free_safe(query_str);
  
  if (!ctx->db) {
    LOG_ERROR("Database context is NULL!");
    json_free(query);
    json_free(body);
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Database not initialized\"}", "application/json");
  }
  
  /* HIERARCHICAL: Query system/users collection */
  json_value_t* results = db_query_documents(ctx->db, "system/users", query);
  
  json_free(query);
    
  if (!results) {
    json_free(body);
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to query user\"}", "application/json");
  }
  
  if (!results || results->type != JSON_ARRAY || json_array_size(results) == 0) {
    json_free(results);
    json_free(body);
    return create_http_response(HTTP_UNAUTHORIZED, 
                 "{\"error\":\"Invalid credentials\"}", "application/json");
  }
  
  json_value_t* user_doc = json_array_get(results, 0);
  json_value_t* id_val = json_object_get(user_doc, "uuid");
  json_value_t* password_hash_val = json_object_get(user_doc, "password_hash");
  
  if (!id_val || id_val->type != JSON_STRING || 
      !password_hash_val || password_hash_val->type != JSON_STRING) {
    json_free(results);
    json_free(body);
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"User data corrupted\"}", "application/json");
  }
  
  const char* user_id = id_val->value.string;
  const char* stored_hash = password_hash_val->value.string;
  
  /* Verify password */
  extern int verify_password(const char* password, const char* hash);
  
  if (!verify_password(password, stored_hash)) {
    json_free(results);
    json_free(body);
    return create_http_response(HTTP_UNAUTHORIZED, 
                 "{\"error\":\"Invalid credentials\"}", "application/json");
  }
  
  /* Store library in user's session for context */
  json_value_t* user_library = json_object_get(user_doc, "library");
  const char* user_lib = (user_library && user_library->type == JSON_STRING) ? 
                         user_library->value.string : library;
    
  /* Create proper JWT token pair */
  LOG_DEBUG("Creating JWT token pair for user: %s", username);
  json_value_t* response_obj = NULL;
  char* response_str = jwt_create_token_pair(ctx->jwt_secret, user_id, username, &response_obj);
  LOG_DEBUG("jwt_create_token_pair returned, checking result.");
  
  if (!response_str || !response_obj) {
    LOG_ERROR("Failed to create token response.");
    json_free(results);
    json_free(body);
    if (response_obj) json_free(response_obj);
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to create response\"}", "application/json");
  }
  
  LOG_DEBUG("Token response created successfully.");
  
  /* Add library context to response */
  json_object_set(response_obj, "library", json_create_string(user_lib));
    
    /* Create session record for the access token */
    if (ctx->db && response_obj) {
      json_value_t* token_val = json_object_get(response_obj, "token");
      if (token_val && token_val->type == JSON_STRING) {
        const char* access_token = token_val->value.string;
        
        /* First, invalidate any existing active sessions for this user */
        LOG_DEBUG("Checking for existing sessions for user: %s", user_id);
        json_value_t* session_query = json_create_object();
        json_object_set(session_query, "user_id", json_create_string(user_id));
        json_object_set(session_query, "active", json_create_boolean(1));
        
        /* Query system/sessions collection in JDBX architecture */
        json_value_t* existing_sessions = db_query_documents(ctx->db, "system/sessions", session_query);
        json_free(session_query);
        
        if (existing_sessions && existing_sessions->type == JSON_ARRAY) {
          int session_count = json_array_size(existing_sessions);
          LOG_DEBUG("Found %d existing active sessions for user", session_count);
          
          /* Invalidate each existing session */
          for (size_t i = 0; i < existing_sessions->value.array.size; i++) {
            json_value_t* session = json_array_get(existing_sessions, i);
            json_value_t* session_id_val = json_object_get(session, "_id");
            if (!session_id_val) {
              session_id_val = json_object_get(session, "uuid");
            }
            if (session_id_val && session_id_val->type == JSON_STRING) {
              const char* old_session_id = session_id_val->value.string;
              LOG_INFO("Invalidating old session: %s", old_session_id);
              rbac_db_invalidate_session(ctx->db, old_session_id);
            }
          }
          json_free(existing_sessions);
        }
        
        /* Extract client info from request */
        const char* ip_address = request->remote_addr;
        const char* user_agent = request->user_agent;
        
        /* Create new session with 30 minute expiration */
        time_t expires_at = time(NULL) + (30 * 60);
        char* session_id = rbac_db_create_session(ctx->db, user_id, access_token, 
                            expires_at, ip_address, user_agent);
        
        if (session_id) {
          LOG_INFO("New session created with ID: %s", session_id);
          buffer_pool_free(session_id);
        } else {
          LOG_ERROR("Failed to create session for user: %s", username);
        }
      }
    }
    
    /* Re-serialize response with library added */
    buffer_pool_free_safe(response_str);
    response_str = json_stringify(response_obj);
    
    /* Clean up and return response */
    json_free(results);
    json_free(body);
    
    http_response_t* response = create_http_response(HTTP_OK, response_str, "application/json");
    LOG_DEBUG("Returning successful login response with library context: %s", user_lib);
    
    /* Clean up response string */
    buffer_pool_free_safe(response_str);
    json_free(response_obj);
    
    return response;
}