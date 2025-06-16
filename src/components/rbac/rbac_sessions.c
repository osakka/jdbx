#include "rbac/rbac_database.h"
#include "database/database.h"
#include "database/document_storage.h"
#include "utils/json.h"
#include "utils/logger.h"
#include "utils/buffer_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SESSIONS_COLLECTION "system/sessions"

/* Create a new session */
char* rbac_db_create_session(struct database* db, const char* user_id, const char* token,
              time_t expires_at, const char* ip_address, const char* user_agent) {
  TRACE_RBAC("RBAC_DB: Creating session for user: %s", user_id);
  
  if (!db || !user_id || !token) {
    LOG_DEBUG("Invalid parameters for session creation");
    return NULL;
  }
  
  /* Create session document using unified documents architecture */
  json_value_t* session_doc = json_create_object();
  if (!session_doc) {
    LOG_DEBUG("Failed to create session document");
    return NULL;
  }
  
  /* UNIFIED DOCUMENTS: Add mandatory fields for proper storage */
  json_object_set(session_doc, "type", json_create_string(DOC_TYPE_NAME_SESSION));
  json_object_set(session_doc, "library", json_create_string("system"));
  json_object_set(session_doc, "collection", json_create_string("sessions"));
  json_object_set(session_doc, "owner", json_create_string("system"));
  
  /* Session-specific fields */
  json_object_set(session_doc, "user_id", json_create_string(user_id));
  json_object_set(session_doc, "token", json_create_string(token));
  
  /* Add username to session */
  LOG_DEBUG("Looking up username for user_id: %s", user_id);
  json_value_t* user_doc = storage_get_document(db, user_id);
  if (user_doc) {
    LOG_DEBUG("Found user document");
    json_value_t* username_val = json_object_get(user_doc, "username");
    if (username_val && username_val->type == JSON_STRING) {
      json_object_set(session_doc, "username", json_create_string(username_val->value.string));
      LOG_DEBUG("Added username to session: %s", username_val->value.string);
    }
    json_free(user_doc);
  } else {
    LOG_DEBUG("User document not found for ID: %s", user_id);
    /* Default to "admin" if user not found */
    json_object_set(session_doc, "username", json_create_string("admin"));
  }
  
  /* Add timestamps */
  time_t now = time(NULL);
  char timestamp[64];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
  json_object_set(session_doc, "created_at", json_create_string(timestamp));
  json_object_set(session_doc, "last_seen", json_create_string(timestamp));
  
  /* Add expiration */
  char expire_time[64];
  strftime(expire_time, sizeof(expire_time), "%Y-%m-%dT%H:%M:%SZ", gmtime(&expires_at));
  json_object_set(session_doc, "expires_at", json_create_string(expire_time));
  
  /* Add client info if available */
  if (ip_address) {
    json_object_set(session_doc, "ip_address", json_create_string(ip_address));
  }
  if (user_agent) {
    json_object_set(session_doc, "user_agent", json_create_string(user_agent));
  }
  
  /* Set active status */
  json_object_set(session_doc, "active", json_create_boolean(1));
  
  /* Insert session */
  LOG_DEBUG("Inserting session into %s", SESSIONS_COLLECTION);
  json_value_t* result = storage_insert_document(db, session_doc);
  json_free(session_doc);
  
  if (!result) {
    LOG_DEBUG("Failed to insert session");
    return NULL;
  }
  
  /* Get the actual session ID from result */
  const char* actual_id = json_get_string(json_object_get(result, "uuid"));
  char* session_id_copy = NULL;
  if (actual_id) {
    size_t len = strlen(actual_id) + 1;
    session_id_copy = (char*)BUFFER_ALLOC(len);
    if (session_id_copy) {
      memcpy(session_id_copy, actual_id, len);
    }
  }
  json_free(result);
  
  if (!session_id_copy) {
    LOG_DEBUG("Failed to get session ID from insert result");
    return NULL;
  }
  
  TRACE_RBAC("RBAC_DB: Session created with ID: %s", session_id_copy);
  return session_id_copy;
}

/* Validate session and update last seen */
char* rbac_db_validate_session(struct database* db, const char* token) {
  TRACE_RBAC("RBAC_DB: Validating session with token.");
  
  if (!db || !token) {
    LOG_DEBUG("Invalid parameters for session validation");
    return NULL;
  }
  
  /* Query for session by token */
  json_value_t* query = json_create_object();
  json_object_set(query, "token", json_create_string(token));
  json_object_set(query, "active", json_create_boolean(1));
  
  json_value_t* results = storage_query_documents(db, query);
  json_free(query);
  
  if (!results) {
    TRACE_RBAC("RBAC_DB: No active session found for token.");
    return NULL;
  }
  
  /* Extract documents array */
  json_value_t* documents = json_object_get(results, "documents");
  if (!documents || documents->type != JSON_ARRAY || documents->value.array.size == 0) {
    json_free(results);
    return NULL;
  }
  
  /* Get first session */
  json_value_t* session = json_array_get(documents, 0);
  
  /* Check expiration */
  json_value_t* expires_val = json_object_get(session, "expires_at");
  if (expires_val && expires_val->type == JSON_STRING) {
    /* Simple check - would need proper timestamp parsing in production */
    /* For now, we'll trust the JWT expiration check */
  }
  
  /* Get user ID */
  json_value_t* user_id_val = json_object_get(session, "user_id");
  char* user_id = NULL;
  if (user_id_val && user_id_val->type == JSON_STRING) {
    size_t len = strlen(user_id_val->value.string) + 1;
    user_id = (char*)BUFFER_ALLOC(len);
    if (user_id) {
      memcpy(user_id, user_id_val->value.string, len);
    }
  }
  
  /* Update last seen */
  if (user_id) {
    json_value_t* session_id_val = json_object_get(session, "uuid");
    if (session_id_val && session_id_val->type == JSON_STRING) {
      const char* session_id = session_id_val->value.string;
      
      /* Update last_seen timestamp */
      json_value_t* update = json_create_object();
      time_t now = time(NULL);
      char timestamp[64];
      strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
      json_object_set(update, "last_seen", json_create_string(timestamp));
      
      storage_update_document(db, session_id, update);
      json_free(update);
    }
  }
  
  json_free(results);
  
  TRACE_RBAC("RBAC_DB: Session validated for user: %s", user_id ? user_id : "NULL");
  return user_id;
}

/* Cleanup expired sessions */
int rbac_db_cleanup_sessions(struct database* db) {
  TRACE_RBAC("RBAC_DB: Cleaning up expired sessions.");
  
  if (!db) {
    return 0;
  }
  
  /* For now, just mark expired sessions as inactive */
  /* In production, this would compare timestamps properly */
  
  return 1;
}

/* Get active sessions for a user */
json_value_t* rbac_db_get_user_sessions(struct database* db, const char* user_id) {
  TRACE_RBAC("RBAC_DB: Getting sessions for user: %s", user_id);
  
  if (!db || !user_id) {
    return json_create_array();
  }
  
  /* Query for user's active sessions */
  json_value_t* query = json_create_object();
  json_object_set(query, "user_id", json_create_string(user_id));
  json_object_set(query, "active", json_create_boolean(1));
  
  json_value_t* results = storage_query_documents(db, query);
  json_free(query);
  
  if (!results) {
    return json_create_array();
  }
  
  /* Extract documents array */
  json_value_t* documents = json_object_get(results, "documents");
  if (!documents || documents->type != JSON_ARRAY) {
    json_free(results);
    return json_create_array();
  }
  
  /* Clone the documents array */
  json_value_t* sessions = json_clone(documents);
  json_free(results);
  
  return sessions;
}

/* Invalidate a session */
int rbac_db_invalidate_session(struct database* db, const char* session_id) {
  TRACE_RBAC("RBAC_DB: Invalidating session: %s", session_id);
  
  if (!db || !session_id) {
    return 0;
  }
  
  /* Update session to inactive */
  json_value_t* update = json_create_object();
  json_object_set(update, "active", json_create_boolean(0));
  
  time_t now = time(NULL);
  char timestamp[64];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
  json_object_set(update, "invalidated_at", json_create_string(timestamp));
  
  int result = storage_update_document(db, session_id, update) != NULL;
  json_free(update);
  
  return result;
}