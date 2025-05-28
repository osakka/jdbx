#include "api/api.h"
#include "database/database.h"
#include "rbac/rbac_database.h"
#include "utils/json.h"
#include "utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Handle get sessions request */
http_response_t* api_handle_get_sessions(api_context_t* ctx, http_request_t* request) {
  (void)request; /* Suppress unused parameter warning */
  if (!ctx || !ctx->db) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 "{\"error\":\"Database not initialized\"}", "application/json");
  }
  
  /* Query all sessions */
  json_value_t* query = json_create_object();
  json_value_t* results = db_query_documents(ctx->db, "_sessions", query);
  json_free(query);
  
  if (!results) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 "{\"error\":\"Failed to query sessions\"}", "application/json");
  }
  
  /* Extract documents array */
  json_value_t* documents = json_object_get(results, "documents");
  if (!documents || documents->type != JSON_ARRAY) {
    json_free(results);
    
    /* Return empty array */
    json_value_t* empty_response = json_create_object();
    json_object_set(empty_response, "sessions", json_create_array());
    json_object_set(empty_response, "count", json_create_number(0));
    
    char* response_str = json_stringify(empty_response);
    json_free(empty_response);
    
    return create_http_response(HTTP_OK, response_str, "application/json");
  }
  
  /* Create response with sessions */
  json_value_t* response_obj = json_create_object();
  json_object_set(response_obj, "sessions", json_clone(documents));
  json_object_set(response_obj, "count", json_create_number(json_array_size(documents)));
  
  json_free(results);
  
  char* response_str = json_stringify(response_obj);
  json_free(response_obj);
  
  return create_http_response(HTTP_OK, response_str, "application/json");
}

/* Handle get active sessions request */
http_response_t* api_handle_get_active_sessions(api_context_t* ctx, http_request_t* request) {
  (void)request; /* Suppress unused parameter warning */
  if (!ctx || !ctx->db) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 "{\"error\":\"Database not initialized\"}", "application/json");
  }
  
  /* Query active sessions */
  json_value_t* query = json_create_object();
  json_object_set(query, "active", json_create_boolean(1));
  
  json_value_t* results = db_query_documents(ctx->db, "_sessions", query);
  json_free(query);
  
  if (!results) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 "{\"error\":\"Failed to query sessions\"}", "application/json");
  }
  
  /* Extract documents array */
  json_value_t* documents = json_object_get(results, "documents");
  if (!documents || documents->type != JSON_ARRAY) {
    json_free(results);
    
    /* Return empty array */
    json_value_t* empty_response = json_create_object();
    json_object_set(empty_response, "sessions", json_create_array());
    json_object_set(empty_response, "count", json_create_number(0));
    
    char* response_str = json_stringify(empty_response);
    json_free(empty_response);
    
    return create_http_response(HTTP_OK, response_str, "application/json");
  }
  
  /* Create response with sessions */
  json_value_t* response_obj = json_create_object();
  json_object_set(response_obj, "sessions", json_clone(documents));
  json_object_set(response_obj, "count", json_create_number(json_array_size(documents)));
  
  json_free(results);
  
  char* response_str = json_stringify(response_obj);
  json_free(response_obj);
  
  return create_http_response(HTTP_OK, response_str, "application/json");
}

/* Handle logout (invalidate session) */
http_response_t* api_handle_logout(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !ctx->db || !request) {
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  /* Get token from authorization header */
  const char* auth_header = request->authorization;
  if (!auth_header) {
    return create_http_response(HTTP_UNAUTHORIZED,
                 "{\"error\":\"No authorization header\"}", "application/json");
  }
  
  /* Extract token */
  const char* token = auth_header;
  if (strncmp(auth_header, "Bearer ", 7) == 0) {
    token = auth_header + 7;
  }
  
  /* Find session by token */
  json_value_t* query = json_create_object();
  json_object_set(query, "token", json_create_string(token));
  json_object_set(query, "active", json_create_boolean(1));
  
  json_value_t* results = db_query_documents(ctx->db, "_sessions", query);
  json_free(query);
  
  if (!results) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 "{\"error\":\"Failed to query sessions\"}", "application/json");
  }
  
  /* Get documents array */
  json_value_t* documents = json_object_get(results, "documents");
  if (!documents || documents->type != JSON_ARRAY || json_array_size(documents) == 0) {
    json_free(results);
    return create_http_response(HTTP_NOT_FOUND,
                 "{\"error\":\"Session not found\"}", "application/json");
  }
  
  /* Get first session */
  json_value_t* session = json_array_get(documents, 0);
  json_value_t* session_id_val = json_object_get(session, "_id");
  
  if (session_id_val && session_id_val->type == JSON_STRING) {
    const char* session_id = session_id_val->value.string;
    
    /* Invalidate session */
    if (rbac_db_invalidate_session(ctx->db, session_id)) {
      json_free(results);
      return create_http_response(HTTP_OK,
                   "{\"success\":true,\"message\":\"Logged out successfully\"}", 
                   "application/json");
    }
  }
  
  json_free(results);
  return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
               "{\"error\":\"Failed to invalidate session\"}", "application/json");
}

