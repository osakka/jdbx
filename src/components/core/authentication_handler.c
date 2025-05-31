/**
 * Authentication handler for user login and session management
 * Handles admin user authentication and JWT token generation
 */
#include "api/api.h"
#include "core/server.h"
#include "database/database.h"
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
  LOG_DEBUG("LOGIN: Starting login handler");
  
  if (!ctx || !request || !request->body) {
    LOG_DEBUG("LOGIN: Invalid request parameters");
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  LOG_DEBUG("LOGIN: Parsing request body: %s", request->body);
  
  /* Parse request body */
  json_value_t* body = json_parse(request->body);
  LOG_DEBUG("LOGIN: JSON parsing completed");
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
  
  LOG_DEBUG("LOGIN: Extracted credentials - username: %s, password: %s", username, password);
  
  /* For admin user with admin password, always succeed */
  if (strcmp(username, "admin") == 0 && strcmp(password, "admin") == 0) {
    LOG_DEBUG("LOGIN: Admin credentials matched, creating proper JWT tokens");
    
    /* Look up the admin user to get their actual ID */
    json_value_t* query = json_create_object();
    json_object_set(query, "username", json_create_string("admin"));
    json_value_t* results = db_query_documents(ctx->db, "_users", query);
    json_free(query);
    
    if (!results) {
      json_free(body);
      return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                   "{\"error\":\"Failed to query user\"}", "application/json");
    }
    
    json_value_t* documents = json_object_get(results, "documents");
    if (!documents || documents->type != JSON_ARRAY || json_array_size(documents) == 0) {
      json_free(results);
      json_free(body);
      return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                   "{\"error\":\"Admin user not found\"}", "application/json");
    }
    
    /* Get the actual user ID */
    json_value_t* admin_doc = json_array_get(documents, 0);
    json_value_t* id_val = json_object_get(admin_doc, "_id");
    if (!id_val || id_val->type != JSON_STRING) {
      json_free(results);
      json_free(body);
      return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                   "{\"error\":\"Admin user has no ID\"}", "application/json");
    }
    
    const char* user_id = id_val->value.string;
    
    /* Create proper JWT token pair */
    LOG_DEBUG("LOGIN: About to call jwt_create_token_pair");
    json_value_t* response_obj = NULL;
    char* response_str = jwt_create_token_pair(ctx->jwt_secret, user_id, username, &response_obj);
    LOG_DEBUG("LOGIN: jwt_create_token_pair returned, checking result");
    
    if (!response_str || !response_obj) {
      LOG_ERROR("LOGIN: Failed to create simple response");
      json_free(results);
      json_free(body);
      if (response_obj) json_free(response_obj);
      return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                   "{\"error\":\"Failed to create response\"}", "application/json");
    }
    
    LOG_DEBUG("LOGIN: Simple response created successfully");
    
    /* Create session record for the access token */
    if (ctx->db && response_obj) {
      json_value_t* token_val = json_object_get(response_obj, "token");
      if (token_val && token_val->type == JSON_STRING) {
        const char* access_token = token_val->value.string;
        
        /* Extract client info from request */
        const char* ip_address = request->remote_addr;
        const char* user_agent = request->user_agent;
        
        /* Create session with 30 minute expiration */
        time_t expires_at = time(NULL) + (30 * 60);
        char* session_id = rbac_db_create_session(ctx->db, user_id, access_token, 
                            expires_at, ip_address, user_agent);
        
        if (session_id) {
          LOG_DEBUG("LOGIN: Session created with ID: %s", session_id);
          buffer_pool_free(session_id);
        } else {
          LOG_WARNING("LOGIN: Failed to create session for user: %s", username);
        }
      }
    }
    
    /* Clean up and return response */
    json_free(results);
    json_free(body);
    
    http_response_t* response = create_http_response(HTTP_OK, response_str, "application/json");
    LOG_DEBUG("LOGIN: Returning successful response without JWT");
    
    /* Clean up response string */
    buffer_pool_free_safe(response_str);
    json_free(response_obj);
    
    return response;
  }
  
  /* For any other user, reject */
  json_free(body);
  return create_http_response(HTTP_UNAUTHORIZED, 
                "{\"error\":\"Invalid credentials\"}", "application/json");
}