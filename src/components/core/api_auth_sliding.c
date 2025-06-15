#include "api/api.h"
#include "rbac/jwt.h"
#include "rbac/jwt_cache.h"
#include "rbac/rbac_database.h"
#include "database/database.h"
#include "utils/logger.h"
#include "utils/json.h"
#include "utils/buffer_pool.h"
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
    if (g_logger) LOG_DEBUG("Session extension skipped: invalid parameters.");
    return;
  }
  
  if (g_logger) LOG_DEBUG("Attempting to extend session for token: %.30s...", token);
  
  /* Query for the session by token */
  json_value_t* query = json_create_object();
  json_object_set(query, "token", json_create_string(token));
  json_object_set(query, "active", json_create_boolean(1));
  
  if (g_logger) LOG_DEBUG("Querying sessions for token: %.30s...", token);
  json_value_t* results = db_query_documents(ctx->db, "system/sessions", query);
  json_free(query);
  
  if (!results) {
    if (g_logger) LOG_DEBUG("No results from session query");
    return;
  }
  
  /* Extract documents array */
  json_value_t* documents = json_object_get(results, "documents");
  if (!documents || documents->type != JSON_ARRAY || documents->value.array.size == 0) {
    if (g_logger) LOG_DEBUG("No active sessions found for token.");
    json_free(results);
    return;
  }
  
  /* Get first session */
  json_value_t* session = json_array_get(documents, 0);
  json_value_t* session_id_val = json_object_get(session, "uuid");
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
  json_value_t* update_result = db_update_document(ctx->db, "system/sessions", session_id, full_session);
  
  if (update_result) {
    if (g_logger) {
      LOG_DEBUG("Extended session %s expiration to %s", session_id, expire_time);
    }
    json_free(update_result);
  } else {
    if (g_logger) {
      LOG_ERROR("Failed to update session %s expiration - sliding window disabled", session_id);
    }
  }
  
  json_free(full_session);
  json_free(results);
}

/* Enhanced authentication function with sliding sessions and comprehensive logging */
int api_authenticate_request_sliding(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request) {
    if (g_logger) TRACE_AUTH("Authentication failed - Invalid context or request (ctx=%p, request=%p)", 
        (void*)ctx, (void*)request);
    return 0;
  }
  
  /* Check if database is in bootstrap mode - bypass authentication */
  if (ctx->db && ctx->db->is_bootstrap_mode) {
    if (g_logger) {
      LOG_DEBUG("Authentication bypassed - database in bootstrap mode");
    }
    
    /* Check if we need to perform deferred bootstrap */
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
    
    return 1; /* Allow all requests during bootstrap */
  }
  
  /* Get client IP for detailed authentication tracking */
  const char* client_ip = request->remote_addr ? request->remote_addr : "unknown";
  
  if (g_logger) {
    TRACE_AUTH("Authentication flow start - client=%s, path=%s, method=%s", 
        client_ip, request->path ? request->path : "unknown", 
        request->method == HTTP_GET ? "GET" :
        request->method == HTTP_POST ? "POST" :
        request->method == HTTP_PUT ? "PUT" :
        request->method == HTTP_DELETE ? "DELETE" : "UNKNOWN");
    TRACE_API("AUTH_FLOW_DETAILS: user_agent=%s, origin=%s", 
        request->user_agent ? request->user_agent : "none",
        request->origin ? request->origin : "none");
  }
  
  /* Extract token with enhanced logging */
  char* token = api_extract_token(request);
  if (!token) {
    if (g_logger) {
      LOG_DEBUG("No token found - client=%s, path=%s", client_ip, request->path);
      TRACE_API("AUTH_FLOW_HEADERS: authorization=%s, cookie_header=%s", 
          request->authorization ? request->authorization : "none",
          request->cookie_header ? request->cookie_header : "none");
    }
    return 0;
  }
  
  if (g_logger) {
    TRACE_AUTH("Token extracted - client=%s, token_prefix=%.20s..., token_length=%zu", 
        client_ip, token, strlen(token));
    TRACE_API("AUTH_FLOW_TOKEN_SOURCE: authorization_header=%s", 
        request->authorization ? "present" : "absent");
  }
  
  /* Special handling for admin tokens from static_api.c */
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
        LOG_INFO("Admin token authentication allowed - client=%s", client_ip);
      }
      free(token);
      return 1;
    }
  }
  
  /* Check JWT cache first */
  jwt_payload_t* cached_payload = jwt_cache_get(token);
  if (cached_payload) {
    /* Extract username from claims if available */
    const char* username = "unknown";
    if (cached_payload->claims) {
      json_value_t* username_val = json_object_get(cached_payload->claims, "username");
      if (username_val && username_val->type == JSON_STRING) {
        username = username_val->value.string;
      }
    }
    if (g_logger) LOG_DEBUG("JWT cache hit - token valid for user: %s", username);
    
    /* Extend session expiration for sliding sessions */
    extend_session_expiration(ctx, token);
    
    free(token);
    return 1;
  }
  
  /* Verify JWT secret is set */
  if (!ctx->jwt_secret) {
    if (g_logger) {
      LOG_ERROR("JWT secret not set - client=%s, ctx=%p", client_ip, (void*)ctx);
    }
    free(token);
    return 0;
  }
  
  /* Log session lookup attempt */
  if (g_logger) {
    TRACE_API("AUTH_FLOW_SESSION_LOOKUP: client=%s, searching for active session with token", client_ip);
  }
  
  /* Check if session exists in database before JWT verification */
  json_value_t* session_query = json_create_object();
  json_object_set(session_query, "token", json_create_string(token));
  json_object_set(session_query, "active", json_create_boolean(1));
  
  json_value_t* session_results = db_query_documents(ctx->db, "system/sessions", session_query);
  json_free(session_query);
  
  int session_found = 0;
  const char* session_user = NULL;
  const char* session_id = NULL;
  const char* session_expires = NULL;
  const char* session_created = NULL;
  const char* session_last_seen = NULL;
  
  if (session_results) {
    json_value_t* documents = json_object_get(session_results, "documents");
    if (documents && documents->type == JSON_ARRAY && documents->value.array.size > 0) {
      session_found = 1;
      json_value_t* session = json_array_get(documents, 0);
      
      json_value_t* user_val = json_object_get(session, "username");
      json_value_t* id_val = json_object_get(session, "uuid");
      json_value_t* expires_val = json_object_get(session, "expires_at");
      json_value_t* created_val = json_object_get(session, "created_at");
      json_value_t* last_seen_val = json_object_get(session, "last_seen");
      
      if (user_val && user_val->type == JSON_STRING) {
        /* Create a safe copy of session user to avoid use-after-free */
        session_user = buffer_pool_strdup(user_val->value.string);
      }
      if (id_val && id_val->type == JSON_STRING) {
        session_id = buffer_pool_strdup(id_val->value.string);
      }
      if (expires_val && expires_val->type == JSON_STRING) {
        session_expires = buffer_pool_strdup(expires_val->value.string);
      }
      if (created_val && created_val->type == JSON_STRING) {
        session_created = buffer_pool_strdup(created_val->value.string);
      }
      if (last_seen_val && last_seen_val->type == JSON_STRING) {
        session_last_seen = buffer_pool_strdup(last_seen_val->value.string);
      }
    }
    json_free(session_results);
  }
  
  if (g_logger) {
    TRACE_AUTH("Session status - client=%s, session_found=%s, session_id=%s, user=%s", 
        client_ip, session_found ? "yes" : "no", session_id, session_user);
    TRACE_API("AUTH_FLOW_SESSION_TIMES: client=%s, expires=%s, created=%s, last_seen=%s", 
        client_ip, session_expires, session_created, session_last_seen);
  }
  
  /* Verify token with enhanced logging */
  if (g_logger) {
    TRACE_API("AUTH_FLOW_JWT_VERIFY: client=%s, using jwt_secret_length=%zu", 
        client_ip, strlen(ctx->jwt_secret));
  }
  
  int result = jwt_verify(token, ctx->jwt_secret);
  
  if (result) {
    if (g_logger) {
      LOG_INFO("Authentication successful - client=%s, token_valid=yes, session_found=%s, user=%s", 
          client_ip, session_found ? "yes" : "no", session_user);
    }
    
    /* Decode token to cache payload */
    jwt_token_t* decoded = jwt_decode(token);
    if (decoded && decoded->payload) {
      /* Note: jwt_cache_put takes ownership of payload, so we need to duplicate */
      jwt_payload_t* payload_copy = jwt_payload_duplicate(decoded->payload);
      if (payload_copy) {
        /* Extract username from claims */
        const char* username = NULL;
        if (decoded->payload->claims) {
          json_value_t* username_val = json_object_get(decoded->payload->claims, "username");
          if (username_val && username_val->type == JSON_STRING) {
            username = username_val->value.string;
          }
        }
        jwt_cache_put(token, payload_copy, username, decoded->payload->sub);
        if (g_logger) LOG_DEBUG("JWT cached for user: %s", username ? username : "unknown");
      }
      jwt_free(decoded);
    }
    
    /* Extend session expiration on successful authentication */
    extend_session_expiration(ctx, token);
    
  } else {
    if (g_logger) {
      LOG_DEBUG("Token verification failed - client=%s, session_found=%s", 
          client_ip, session_found ? "yes" : "no");
      TRACE_AUTH("Token debug - client=%s, token=%.30s...", client_ip, token);
      
      /* Detailed token analysis for debugging */
      jwt_token_t* decoded = jwt_decode(token);
      if (decoded) {
        if (decoded->header) {
          TRACE_AUTH("Token header - client=%s, alg=%s, typ=%s", client_ip,
               decoded->header->alg ? decoded->header->alg : "NULL",
               decoded->header->typ ? decoded->header->typ : "NULL");
        }
        
        if (decoded->payload) {
          TRACE_AUTH("Token payload - client=%s, iss=%s, sub=%s, exp=%ld", client_ip,
               decoded->payload->iss ? decoded->payload->iss : "NULL",
               decoded->payload->sub ? decoded->payload->sub : "NULL",
               decoded->payload->exp);
          
          /* Check for token expiration with detailed logging */
          time_t current_time = time(NULL);
          if (decoded->payload->exp > 0 && current_time > decoded->payload->exp) {
            LOG_DEBUG("Token expired - client=%s, current_time=%ld, exp=%ld, diff=%ld_seconds", 
                client_ip, current_time, decoded->payload->exp, current_time - decoded->payload->exp);
          } else if (decoded->payload->exp > 0) {
            TRACE_AUTH("Token valid time - client=%s, current_time=%ld, exp=%ld, remaining=%ld_seconds",
                client_ip, current_time, decoded->payload->exp, decoded->payload->exp - current_time);
          } else {
            TRACE_AUTH("Token has no expiration time - client=%s", client_ip);
          }
        }
        
        jwt_free(decoded);
      } else {
        LOG_DEBUG("Unable to decode token for analysis - client=%s", client_ip);
      }
      
      /* If session exists but token is invalid, this might be the double-login issue */
      if (session_found) {
        LOG_WARNING("Session exists but token invalid - possible double-login issue - client=%s", client_ip);
        LOG_DEBUG("Session mismatch details - session_id=%s, user=%s, expires=%s", session_id, session_user, session_expires);
      }
    }
  }
  
  if (g_logger) {
    TRACE_AUTH("Authentication flow end - client=%s, result=%s, user=%s", 
        client_ip, result ? "success" : "failure", session_user);
  }
  
  /* Clean up session variable copies to prevent use-after-free */
  if (session_user) {
    buffer_pool_free_safe((char*)session_user);
  }
  if (session_id) {
    buffer_pool_free_safe((char*)session_id);
  }
  if (session_expires) {
    buffer_pool_free_safe((char*)session_expires);
  }
  if (session_created) {
    buffer_pool_free_safe((char*)session_created);
  }
  if (session_last_seen) {
    buffer_pool_free_safe((char*)session_last_seen);
  }
  
  free(token);
  
  return result;
}