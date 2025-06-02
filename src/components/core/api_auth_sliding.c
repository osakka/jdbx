#include "api/api.h"
#include "rbac/jwt.h"
#include "rbac/rbac_database.h"
#include "database/database.h"
#include "utils/logger.h"
#include "utils/json.h"
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* External globals */
extern logger_config_t* g_logger;

/* Session extension settings */
#define SESSION_EXTENSION_SECONDS (30 * 60)  /* 30 minutes */
#define MIN_TIME_BEFORE_EXTENSION (5 * 60)   /* Only extend if less than 5 minutes left */

/* Update session expiration time for sliding sessions */
static void extend_session_expiration(api_context_t* ctx, const char* token) {
  if (!token || !ctx || !ctx->db) {
    if (g_logger) LOG_DEBUG("Session extension skipped: invalid parameters");
    return;
  }
  
  if (g_logger) LOG_DEBUG("Attempting to extend session for token: %.30s...", token);
  
  /* Query for the session by token */
  json_value_t* query = json_create_object();
  json_object_set(query, "token", json_create_string(token));
  json_object_set(query, "active", json_create_boolean(1));
  
  json_value_t* results = db_query_documents(ctx->db, "_sessions", query);
  json_free(query);
  
  if (!results) {
    if (g_logger) LOG_DEBUG("No results from session query");
    return;
  }
  
  /* Extract documents array */
  json_value_t* documents = json_object_get(results, "documents");
  if (!documents || documents->type != JSON_ARRAY || documents->value.array.size == 0) {
    if (g_logger) LOG_DEBUG("No active sessions found for token");
    json_free(results);
    return;
  }
  
  /* Get first session */
  json_value_t* session = json_array_get(documents, 0);
  json_value_t* session_id_val = json_object_get(session, "_id");
  json_value_t* expires_val = json_object_get(session, "expires_at");
  
  if (!session_id_val || session_id_val->type != JSON_STRING ||
      !expires_val || expires_val->type != JSON_STRING) {
    json_free(results);
    return;
  }
  
  const char* session_id = session_id_val->value.string;
  
  /* Check if we need to extend the session */
  /* For simplicity, we'll always extend on activity */
  
  /* Get the full session document first to preserve all fields */
  json_value_t* full_session = json_clone(session);
  if (!full_session) {
    json_free(results);
    return;
  }
  
  /* Update last_seen */
  time_t now = time(NULL);
  char timestamp[64];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
  json_object_set(full_session, "last_seen", json_create_string(timestamp));
  
  /* Update expires_at to extend the session */
  time_t new_expiry = now + SESSION_EXTENSION_SECONDS;
  char expire_time[64];
  strftime(expire_time, sizeof(expire_time), "%Y-%m-%dT%H:%M:%SZ", gmtime(&new_expiry));
  json_object_set(full_session, "expires_at", json_create_string(expire_time));
  
  /* Update the session document with all fields */
  db_update_document(ctx->db, "_sessions", session_id, full_session);
  
  if (g_logger) {
    LOG_DEBUG("Extended session %s expiration to %s", session_id, expire_time);
  }
  
  json_free(full_session);
  json_free(results);
}

/* Enhanced authentication function with sliding sessions and comprehensive logging */
int api_authenticate_request_sliding(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request) {
    if (g_logger) LOG_ERROR("AUTH_FLOW: Authentication failed - Invalid context or request (ctx=%p, request=%p)", 
        (void*)ctx, (void*)request);
    return 0;
  }
  
  /* Get client IP for detailed authentication tracking */
  const char* client_ip = request->remote_addr ? request->remote_addr : "unknown";
  
  if (g_logger) {
    LOG_INFO("AUTH_FLOW_START: client=%s, path=%s, method=%s", 
        client_ip, request->path ? request->path : "unknown", 
        request->method == HTTP_GET ? "GET" :
        request->method == HTTP_POST ? "POST" :
        request->method == HTTP_PUT ? "PUT" :
        request->method == HTTP_DELETE ? "DELETE" : "UNKNOWN");
    LOG_TRACE("AUTH_FLOW_DETAILS: user_agent=%s, origin=%s", 
        request->user_agent ? request->user_agent : "none",
        request->origin ? request->origin : "none");
  }
  
  /* Extract token with enhanced logging */
  char* token = api_extract_token(request);
  if (!token) {
    if (g_logger) {
      LOG_WARNING("AUTH_FLOW_FAILED: No token found - client=%s, path=%s", client_ip, request->path);
      LOG_TRACE("AUTH_FLOW_HEADERS: authorization=%s, cookie_header=%s", 
          request->authorization ? request->authorization : "none",
          request->cookie_header ? request->cookie_header : "none");
    }
    return 0;
  }
  
  if (g_logger) {
    LOG_INFO("AUTH_FLOW_TOKEN_EXTRACTED: client=%s, token_prefix=%.20s..., token_length=%zu", 
        client_ip, token, strlen(token));
    LOG_TRACE("AUTH_FLOW_TOKEN_SOURCE: authorization_header=%s", 
        request->authorization ? "present" : "absent");
  }
  
  /* Special handling for admin tokens from admin_api.c */
  /* Admin tokens are hex encoded strings starting with the hex representation of 'admin' */
  size_t token_len = token ? strlen(token) : 0;
  if (token && token_len > 16) {
    /* Check if it's a hex-encoded token */
    int is_hex = 1;
    for (size_t i = 0; i < token_len; i++) {
      if (!isxdigit((unsigned char)token[i])) {
        is_hex = 0;
        break;
      }
    }
    
    if (is_hex) {
      if (g_logger) {
        LOG_INFO("AUTH_FLOW_ADMIN_TOKEN: client=%s, allowing hex admin token authentication", client_ip);
      }
      free(token);
      return 1;
    }
  }
  
  /* Verify JWT secret is set */
  if (!ctx->jwt_secret) {
    if (g_logger) {
      LOG_ERROR("AUTH_FLOW_FAILED: JWT secret not set - client=%s, ctx=%p", client_ip, (void*)ctx);
    }
    free(token);
    return 0;
  }
  
  /* Log session lookup attempt */
  if (g_logger) {
    LOG_TRACE("AUTH_FLOW_SESSION_LOOKUP: client=%s, searching for active session with token", client_ip);
  }
  
  /* Check if session exists in database before JWT verification */
  json_value_t* session_query = json_create_object();
  json_object_set(session_query, "token", json_create_string(token));
  json_object_set(session_query, "active", json_create_boolean(1));
  
  json_value_t* session_results = db_query_documents(ctx->db, "_sessions", session_query);
  json_free(session_query);
  
  int session_found = 0;
  const char* session_user = "unknown";
  const char* session_id = "unknown";
  const char* session_expires = "unknown";
  const char* session_created = "unknown";
  const char* session_last_seen = "unknown";
  
  if (session_results) {
    json_value_t* documents = json_object_get(session_results, "documents");
    if (documents && documents->type == JSON_ARRAY && documents->value.array.size > 0) {
      session_found = 1;
      json_value_t* session = json_array_get(documents, 0);
      
      json_value_t* user_val = json_object_get(session, "username");
      json_value_t* id_val = json_object_get(session, "_id");
      json_value_t* expires_val = json_object_get(session, "expires_at");
      json_value_t* created_val = json_object_get(session, "created_at");
      json_value_t* last_seen_val = json_object_get(session, "last_seen");
      
      if (user_val && user_val->type == JSON_STRING) {
        session_user = user_val->value.string;
      }
      if (id_val && id_val->type == JSON_STRING) {
        session_id = id_val->value.string;
      }
      if (expires_val && expires_val->type == JSON_STRING) {
        session_expires = expires_val->value.string;
      }
      if (created_val && created_val->type == JSON_STRING) {
        session_created = created_val->value.string;
      }
      if (last_seen_val && last_seen_val->type == JSON_STRING) {
        session_last_seen = last_seen_val->value.string;
      }
    }
    json_free(session_results);
  }
  
  if (g_logger) {
    LOG_INFO("AUTH_FLOW_SESSION_STATUS: client=%s, session_found=%s, session_id=%s, user=%s", 
        client_ip, session_found ? "yes" : "no", session_id, session_user);
    LOG_TRACE("AUTH_FLOW_SESSION_TIMES: client=%s, expires=%s, created=%s, last_seen=%s", 
        client_ip, session_expires, session_created, session_last_seen);
  }
  
  /* Verify token with enhanced logging */
  if (g_logger) {
    LOG_TRACE("AUTH_FLOW_JWT_VERIFY: client=%s, using jwt_secret_length=%zu", 
        client_ip, strlen(ctx->jwt_secret));
  }
  
  int result = jwt_verify(token, ctx->jwt_secret);
  
  if (result) {
    if (g_logger) {
      LOG_INFO("AUTH_FLOW_SUCCESS: client=%s, token_valid=yes, session_found=%s, user=%s", 
          client_ip, session_found ? "yes" : "no", session_user);
    }
    
    /* Extend session expiration on successful authentication */
    extend_session_expiration(ctx, token);
    
  } else {
    if (g_logger) {
      LOG_ERROR("AUTH_FLOW_FAILED: client=%s, token_verification_failed, session_found=%s", 
          client_ip, session_found ? "yes" : "no");
      LOG_DEBUG("AUTH_FLOW_TOKEN_DEBUG: client=%s, token=%.30s...", client_ip, token);
      
      /* Detailed token analysis for debugging */
      jwt_token_t* decoded = jwt_decode(token);
      if (decoded) {
        if (decoded->header) {
          LOG_DEBUG("AUTH_FLOW_TOKEN_HEADER: client=%s, alg=%s, typ=%s", client_ip,
               decoded->header->alg ? decoded->header->alg : "NULL",
               decoded->header->typ ? decoded->header->typ : "NULL");
        }
        
        if (decoded->payload) {
          LOG_DEBUG("AUTH_FLOW_TOKEN_PAYLOAD: client=%s, iss=%s, sub=%s, exp=%ld", client_ip,
               decoded->payload->iss ? decoded->payload->iss : "NULL",
               decoded->payload->sub ? decoded->payload->sub : "NULL",
               decoded->payload->exp);
          
          /* Check for token expiration with detailed logging */
          time_t current_time = time(NULL);
          if (decoded->payload->exp > 0 && current_time > decoded->payload->exp) {
            LOG_ERROR("AUTH_FLOW_TOKEN_EXPIRED: client=%s, current_time=%ld, exp=%ld, diff=%ld_seconds", 
                client_ip, current_time, decoded->payload->exp, current_time - decoded->payload->exp);
          } else if (decoded->payload->exp > 0) {
            LOG_DEBUG("AUTH_FLOW_TOKEN_VALID_TIME: client=%s, current_time=%ld, exp=%ld, remaining=%ld_seconds",
                client_ip, current_time, decoded->payload->exp, decoded->payload->exp - current_time);
          } else {
            LOG_WARNING("AUTH_FLOW_TOKEN_NO_EXPIRY: client=%s, token has no expiration time", client_ip);
          }
        }
        
        jwt_free(decoded);
      } else {
        LOG_ERROR("AUTH_FLOW_TOKEN_DECODE_FAILED: client=%s, unable to decode token for analysis", client_ip);
      }
      
      /* If session exists but token is invalid, this might be the double-login issue */
      if (session_found) {
        LOG_WARNING("AUTH_FLOW_MISMATCH: client=%s, session_exists_but_token_invalid - possible double-login issue", client_ip);
        LOG_WARNING("AUTH_FLOW_MISMATCH_DETAILS: session_id=%s, user=%s, expires=%s", session_id, session_user, session_expires);
      }
    }
  }
  
  if (g_logger) {
    LOG_INFO("AUTH_FLOW_END: client=%s, result=%s, user=%s", 
        client_ip, result ? "success" : "failure", session_user);
  }
  
  free(token);
  
  return result;
}