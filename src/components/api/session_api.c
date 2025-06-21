#include "api/api.h"
#include "database/document_storage.h"
#include "database/database.h"
#include "database/virtual_layer.h"
#include "rbac/rbac_database.h"
#include "rbac/rbac_db.h"
#include "utils/json.h"
#include "utils/logger.h"
#include "utils/buffer_pool.h"
#include "rbac/jwt_cache.h"
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
  
  /* Query all sessions using virtual layer - single source of truth */
  json_value_t* query = json_create_object();
  json_object_set(query, "type", json_create_string(DOC_TYPE_NAME_SESSION));
  json_object_set(query, "library", json_create_string("system"));
  json_object_set(query, "collection", json_create_string("sessions"));
  json_value_t* results = virtual_query(ctx->db, DOC_TYPE_NAME_SESSION, "system", "sessions", query);
  /* CHECKPOINT: json_free(query); */
  
  if (!results) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 "{\"error\":\"Failed to query sessions\"}", "application/json");
  }
  
  /* Extract documents array */
  json_value_t* documents = json_object_get(results, "documents");
  if (!documents || documents->type != JSON_ARRAY) {
    /* CHECKPOINT: json_free(results); */
    
    /* Return empty array */
    json_value_t* empty_response = json_create_object();
    json_object_set(empty_response, "sessions", json_create_array());
    json_object_set(empty_response, "count", json_create_number(0));
    
    char* response_str = json_stringify(empty_response);
    /* CHECKPOINT: json_free(empty_response); */
    
    return create_http_response(HTTP_OK, response_str, "application/json");
  }
  
  /* Filter out corrupted sessions */
  json_value_t* valid_sessions = json_create_array();
  size_t doc_count = json_array_size(documents);
  
  for (size_t i = 0; i < doc_count; i++) {
    json_value_t* session = json_array_get(documents, i);
    if (!session) continue;
    
    /* Check for required fields */
    json_value_t* user_id = json_object_get(session, "user_id");
    json_value_t* username = json_object_get(session, "username");
    json_value_t* token = json_object_get(session, "token");
    json_value_t* created_at = json_object_get(session, "created_at");
    json_value_t* expires_at = json_object_get(session, "expires_at");
    
    /* Only include sessions with all required fields */
    if (user_id && user_id->type == JSON_STRING &&
        username && username->type == JSON_STRING &&
        token && token->type == JSON_STRING &&
        created_at && created_at->type == JSON_STRING &&
        expires_at && expires_at->type == JSON_STRING) {
      json_array_append(valid_sessions, json_clone(session));
    } else {
      /* Log corrupted session for debugging */
      json_value_t* session_id = json_object_get(session, "uuid");
      if (session_id && session_id->type == JSON_STRING) {
        LOG_WARNING("Skipping corrupted session: %s", session_id->value.string);
      }
    }
  }
  
  /* Create response with valid sessions */
  json_value_t* response_obj = json_create_object();
  json_object_set(response_obj, "sessions", valid_sessions);
  json_object_set(response_obj, "count", json_create_number(json_array_size(valid_sessions)));
  
  /* CHECKPOINT: json_free(results); */
  
  char* response_str = json_stringify(response_obj);
  /* CHECKPOINT: json_free(response_obj); */
  
  return create_http_response(HTTP_OK, response_str, "application/json");
}

/* Handle get active sessions request */
http_response_t* api_handle_get_active_sessions(api_context_t* ctx, http_request_t* request) {
  (void)request; /* Suppress unused parameter warning */
  if (!ctx || !ctx->db) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 "{\"error\":\"Database not initialized\"}", "application/json");
  }
  
  /* Query active sessions using virtual layer - single source of truth */
  json_value_t* query = json_create_object();
  json_object_set(query, "type", json_create_string(DOC_TYPE_NAME_SESSION));
  json_object_set(query, "library", json_create_string("system"));
  json_object_set(query, "collection", json_create_string("sessions"));
  json_object_set(query, "active", json_create_boolean(1));
  
  json_value_t* results = virtual_query(ctx->db, DOC_TYPE_NAME_SESSION, "system", "sessions", query);
  /* CHECKPOINT: json_free(query); */
  
  if (!results) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 "{\"error\":\"Failed to query sessions\"}", "application/json");
  }
  
  /* Extract documents array */
  json_value_t* documents = json_object_get(results, "documents");
  if (!documents || documents->type != JSON_ARRAY) {
    /* CHECKPOINT: json_free(results); */
    
    /* Return empty array */
    json_value_t* empty_response = json_create_object();
    json_object_set(empty_response, "sessions", json_create_array());
    json_object_set(empty_response, "count", json_create_number(0));
    
    char* response_str = json_stringify(empty_response);
    /* CHECKPOINT: json_free(empty_response); */
    
    return create_http_response(HTTP_OK, response_str, "application/json");
  }
  
  /* Filter out corrupted sessions */
  json_value_t* valid_sessions = json_create_array();
  size_t doc_count = json_array_size(documents);
  
  for (size_t i = 0; i < doc_count; i++) {
    json_value_t* session = json_array_get(documents, i);
    if (!session) continue;
    
    /* Check for required fields */
    json_value_t* user_id = json_object_get(session, "user_id");
    json_value_t* username = json_object_get(session, "username");
    json_value_t* token = json_object_get(session, "token");
    json_value_t* created_at = json_object_get(session, "created_at");
    json_value_t* expires_at = json_object_get(session, "expires_at");
    
    /* Only include sessions with all required fields */
    if (user_id && user_id->type == JSON_STRING &&
        username && username->type == JSON_STRING &&
        token && token->type == JSON_STRING &&
        created_at && created_at->type == JSON_STRING &&
        expires_at && expires_at->type == JSON_STRING) {
      json_array_append(valid_sessions, json_clone(session));
    } else {
      /* Log corrupted session for debugging */
      json_value_t* session_id = json_object_get(session, "uuid");
      if (session_id && session_id->type == JSON_STRING) {
        LOG_WARNING("Skipping corrupted active session: %s", session_id->value.string);
      }
    }
  }
  
  /* Create response with valid sessions */
  json_value_t* response_obj = json_create_object();
  json_object_set(response_obj, "sessions", valid_sessions);
  json_object_set(response_obj, "count", json_create_number(json_array_size(valid_sessions)));
  
  /* CHECKPOINT: json_free(results); */
  
  char* response_str = json_stringify(response_obj);
  /* CHECKPOINT: json_free(response_obj); */
  
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
  
  /* Find session by token using virtual layer - single source of truth */
  json_value_t* query = json_create_object();
  json_object_set(query, "token", json_create_string(token));
  json_object_set(query, "active", json_create_boolean(1));
  
  json_value_t* results = virtual_query(ctx->db, DOC_TYPE_NAME_SESSION, "system", "sessions", query);
  /* CHECKPOINT: json_free(query); */
  
  if (!results) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 "{\"error\":\"Failed to query sessions\"}", "application/json");
  }
  
  /* Get documents array */
  json_value_t* documents = json_object_get(results, "documents");
  if (!documents || documents->type != JSON_ARRAY || json_array_size(documents) == 0) {
    /* CHECKPOINT: json_free(results); */
    return create_http_response(HTTP_NOT_FOUND,
                 "{\"error\":\"Session not found\"}", "application/json");
  }
  
  /* Get the session (there should only be one with this token) */
  json_value_t* session = json_array_get(documents, 0);
  json_value_t* session_id_val = json_object_get(session, "uuid");
  json_value_t* user_id_val = json_object_get(session, "user_id");
  
  if (session_id_val && session_id_val->type == JSON_STRING) {
    /* Invalidate the specific token from JWT cache */
    jwt_cache_invalidate_token(token);
    LOG_INFO("Invalidated JWT cache for token");
    
    /* Also invalidate all cache entries for the user */
    if (user_id_val && user_id_val->type == JSON_STRING) {
      jwt_cache_invalidate_user(user_id_val->value.string);
      LOG_INFO("Invalidated all JWT cache entries for user: %s", user_id_val->value.string);
    }
    
    /* Invalidate ALL sessions with this token */
    if (rbac_db_invalidate_sessions_by_token(ctx->db, token)) {
      /* CHECKPOINT: json_free(results); */
      return create_http_response(HTTP_OK,
                   "{\"success\":true,\"message\":\"Logged out successfully\"}", 
                   "application/json");
    }
  }
  
  /* CHECKPOINT: json_free(results); */
  return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
               "{\"error\":\"Failed to invalidate session\"}", "application/json");
}

/* Handle library switching - REMOVED: Moved to auth_session_api.c with enhanced JWT handling */
#if 0
http_response_t* api_handle_switch_library(api_context_t* ctx, http_request_t* request) {
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
  
  /* Parse request body */
  json_value_t* body = json_parse(request->body);
  if (!body || body->type != JSON_OBJECT) {
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid JSON body\"}", "application/json");
  }
  
  /* Get library name from request */
  json_value_t* library_val = json_object_get(body, "library");
  if (!library_val || library_val->type != JSON_STRING) {
    /* CHECKPOINT: json_free(body); */
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Library name required\"}", "application/json");
  }
  
  const char* library_name = library_val->value.string;
  
  /* Validate library exists using unified documents architecture */
  json_value_t* library_query = json_create_object();
  json_object_set(library_query, "type", json_create_string("library"));
  json_object_set(library_query, "name", json_create_string(library_name));
  json_value_t* library_results = virtual_query(ctx->db, DOC_TYPE_NAME_LIBRARY, "system", VIRTUAL_COLLECTION_LIBRARIES, library_query);
  /* CHECKPOINT: json_free(library_query); */
  
  int library_exists = 0;
  if (library_results) {
    json_value_t* docs = json_object_get(library_results, "documents");
    if (docs && docs->type == JSON_ARRAY && json_array_size(docs) > 0) {
      library_exists = 1;
    }
    /* CHECKPOINT: json_free(library_results); */
  }
  
  if (!library_exists) {
    /* CHECKPOINT: json_free(body); */
    return create_http_response(HTTP_NOT_FOUND,
                 "{\"error\":\"Library not found\"}", "application/json");
  }
  
  /* Find session by token */
  json_value_t* query = json_create_object();
  json_object_set(query, "token", json_create_string(token));
  json_object_set(query, "active", json_create_boolean(1));
  
  json_value_t* results = virtual_query(ctx->db, DOC_TYPE_NAME_SESSION, "system", VIRTUAL_COLLECTION_SESSIONS, query);
  /* CHECKPOINT: json_free(query); */
  
  if (!results) {
    /* CHECKPOINT: json_free(body); */
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 "{\"error\":\"Failed to query sessions\"}", "application/json");
  }
  
  /* Get documents array */
  json_value_t* documents = json_object_get(results, "documents");
  if (!documents || documents->type != JSON_ARRAY || json_array_size(documents) == 0) {
    /* CHECKPOINT: json_free(results); */
    /* CHECKPOINT: json_free(body); */
    return create_http_response(HTTP_NOT_FOUND,
                 "{\"error\":\"Session not found\"}", "application/json");
  }
  
  /* Get first session */
  json_value_t* session = json_array_get(documents, 0);
  json_value_t* session_id_val = json_object_get(session, "uuid");
  
  if (!session_id_val || session_id_val->type != JSON_STRING) {
    /* CHECKPOINT: json_free(results); */
    /* CHECKPOINT: json_free(body); */
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 "{\"error\":\"Invalid session data\"}", "application/json");
  }
  
  const char* session_id = session_id_val->value.string;
  
  /* Update session with new library */
  json_value_t* update_doc = json_clone(session);
  json_object_set(update_doc, "library", json_create_string(library_name));
  
  /* Add timestamp */
  time_t now = time(NULL);
  char timestamp[64];
  struct tm* utc_tm = gmtime(&now);
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", utc_tm);
  json_object_set(update_doc, "updated_at", json_create_string(timestamp));
  
  /* Update session in database */
  json_value_t* update_result = virtual_update(ctx->db, session_id, update_doc);
  /* CHECKPOINT: json_free(update_doc); */
  
  if (!update_result) {
    /* CHECKPOINT: json_free(results); */
    /* CHECKPOINT: json_free(body); */
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 "{\"error\":\"Failed to update session\"}", "application/json");
  }
  
  /* CHECKPOINT: json_free(update_result); */
  /* CHECKPOINT: json_free(results); */
  /* CHECKPOINT: json_free(body); */
  
  /* Return success response */
  json_value_t* response = json_create_object();
  json_object_set(response, "success", json_create_boolean(1));
  json_object_set(response, "library", json_create_string(library_name));
  json_object_set(response, "message", json_create_string("Library switched successfully"));
  
  char* response_str = json_stringify(response);
  /* CHECKPOINT: json_free(response); */
  
  return create_http_response(HTTP_OK, response_str, "application/json");
}
#endif

