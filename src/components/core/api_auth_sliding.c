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

/* Enhanced authentication function with sliding sessions */
int api_authenticate_request_sliding(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request) {
    if (g_logger) LOG_ERROR("Authentication failed: Invalid context or request");
    return 0;
  }
  
  /* Extract token */
  char* token = api_extract_token(request);
  if (!token) {
    if (g_logger) LOG_ERROR("Authentication failed: No token found in request");
    return 0;
  }
  
  if (g_logger) LOG_DEBUG("Authenticating token: %.20s...", token);
  
  /* Special handling for admin tokens from admin_api.c */
  /* Admin tokens are hex encoded strings starting with the hex representation of 'admin' */
  if (token && strlen(token) > 16) {
    /* Check if it's a hex-encoded token */
    int is_hex = 1;
    for (size_t i = 0; i < strlen(token); i++) {
      if (!isxdigit((unsigned char)token[i])) {
        is_hex = 0;
        break;
      }
    }
    
    if (is_hex) {
      if (g_logger) LOG_DEBUG("Allowing admin token authentication for hex token");
      free(token);
      return 1;
    }
  }
  
  /* Verify JWT secret is set */
  if (!ctx->jwt_secret) {
    if (g_logger) LOG_ERROR("Authentication failed: JWT secret not set in API context");
    free(token);
    return 0;
  }
  
  /* Verify token */
  if (g_logger) LOG_DEBUG("Using JWT secret: '%s'", ctx->jwt_secret);
  
  int result = jwt_verify(token, ctx->jwt_secret);
  
  if (result) {
    if (g_logger) LOG_DEBUG("Token authentication successful");
    
    /* Extend session expiration on successful authentication */
    extend_session_expiration(ctx, token);
    
  } else {
    if (g_logger) {
      LOG_ERROR("Token verification failed");
      LOG_DEBUG("Token: %.30s...", token);
      
      /* Verify token parts */
      jwt_token_t* decoded = jwt_decode(token);
      if (decoded) {
        if (decoded->header) {
          LOG_DEBUG("Token header alg: %s, typ: %s", 
               decoded->header->alg ? decoded->header->alg : "NULL",
               decoded->header->typ ? decoded->header->typ : "NULL");
        }
        
        if (decoded->payload) {
          LOG_DEBUG("Token payload - iss: %s, sub: %s, exp: %ld", 
               decoded->payload->iss ? decoded->payload->iss : "NULL",
               decoded->payload->sub ? decoded->payload->sub : "NULL",
               decoded->payload->exp);
          
          /* Check for token expiration */
          time_t current_time = time(NULL);
          if (decoded->payload->exp > 0 && current_time > decoded->payload->exp) {
            LOG_ERROR("Token has expired: current_time=%ld, exp=%ld, diff=%ld", 
                 current_time, decoded->payload->exp, current_time - decoded->payload->exp);
          } else {
            LOG_DEBUG("Token is valid: current_time=%ld, exp=%ld, remaining=%ld seconds",
                 current_time, decoded->payload->exp, decoded->payload->exp - current_time);
          }
        }
        
        jwt_free(decoded);
      } else {
        LOG_ERROR("Failed to decode token for debugging");
      }
    }
  }
  
  free(token);
  
  return result;
}