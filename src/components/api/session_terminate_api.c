#include "api/api.h"
#include "database/document_storage.h"
#include "api/session_api.h"
#include "database/database.h"
#include "rbac/rbac_database.h"
#include "rbac/jwt.h"
#include "utils/json.h"
#include "utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils/buffer_pool.h"

/* External functions */
extern int rbac_db_invalidate_session(struct database* db, const char* session_id);
extern int rbac_db_check_permission(struct database* db, const char* user_id, int resource_type, const char* resource_id, int permission);
extern char* rbac_db_validate_session(struct database* db, const char* token);

/* Terminate a specific session */
http_response_t* api_handle_session_terminate(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !ctx->db || !request) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  /* Extract session ID from path */
  const char* path = request->path;
  const char* session_id = NULL;
  
  /* Expected path: /api/sessions/{id}/terminate */
  if (strncmp(path, "/api/sessions/", 14) == 0) {
    const char* id_start = path + 14;
    const char* id_end = strstr(id_start, "/terminate");
    
    if (id_end && id_end > id_start) {
      size_t id_len = id_end - id_start;
      char* temp_id = BUFFER_ALLOC(id_len + 1);
      if (temp_id) {
        strncpy(temp_id, id_start, id_len);
        temp_id[id_len] = '\0';
        session_id = temp_id;
      }
    }
  }
  
  if (!session_id) {
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Session ID required\"}", "application/json");
  }
  
  /* Get current user from token */
  char* token = api_extract_token(request);
  if (!token) {
    BUFFER_FREE((void*)session_id);
    return create_http_response(HTTP_UNAUTHORIZED,
                 "{\"error\":\"Authentication required\"}", "application/json");
  }
  
  /* Validate token and get user info */
  char* user_id = rbac_db_validate_session(ctx->db, token);
  BUFFER_FREE(token);
  
  if (!user_id) {
    BUFFER_FREE((void*)session_id);
    return create_http_response(HTTP_UNAUTHORIZED,
                 "{\"error\":\"Invalid token\"}", "application/json");
  }
  
  /* Check if user has permission to terminate sessions */
  /* For now, users can terminate their own sessions, admins can terminate any */
  int has_permission = 0;
  
  /* First check if it's the user's own session */
  json_value_t* session_doc = db_get_document(ctx->db, STORAGE_LIBRARY, STORAGE_COLLECTION, session_id);
  if (session_doc) {
    json_value_t* session_user_id = json_object_get(session_doc, "user_id");
    if (session_user_id && session_user_id->type == JSON_STRING) {
      if (strcmp(json_get_string(session_user_id), user_id) == 0) {
        has_permission = 1;
      }
    }
    json_free(session_doc);
  }
  
  /* If not own session, check admin permission */
  if (!has_permission) {
    has_permission = rbac_db_check_permission(ctx->db, user_id, 5, "sessions", 8); /* RBAC_ADMIN */
  }
  
  if (!has_permission) {
    BUFFER_FREE(user_id);
    BUFFER_FREE((void*)session_id);
    return create_http_response(HTTP_FORBIDDEN,
                 "{\"error\":\"Permission denied\"}", "application/json");
  }
  
  /* Terminate the session */
  if (rbac_db_invalidate_session(ctx->db, session_id)) {
    LOG_INFO("Session %s terminated by user %s", session_id, user_id);
    BUFFER_FREE(user_id);
    BUFFER_FREE((void*)session_id);
    
    return create_http_response(HTTP_OK,
                 "{\"success\":true,\"message\":\"Session terminated\"}", 
                 "application/json");
  } else {
    BUFFER_FREE(user_id);
    BUFFER_FREE((void*)session_id);
    
    return create_http_response(HTTP_NOT_FOUND,
                 "{\"error\":\"Session not found or already terminated\"}", 
                 "application/json");
  }
}