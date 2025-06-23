#ifndef AUTH_SESSION_API_H
#define AUTH_SESSION_API_H

#include "api/api.h"

/**
 * Enhanced authentication and session management endpoints
 */

/* Get current session information */
http_response_t* api_handle_get_current_session(api_context_t* ctx, http_request_t* request);

/* Get current library context */
http_response_t* api_handle_get_library_context(api_context_t* ctx, http_request_t* request);

/* Switch library context - MOVED to api_auth.h for single source of truth */

/* Terminate a specific session */
http_response_t* api_handle_terminate_session(api_context_t* ctx, http_request_t* request);

/* Change user password */
http_response_t* api_handle_change_password(api_context_t* ctx, http_request_t* request);

#endif /* AUTH_SESSION_API_H */