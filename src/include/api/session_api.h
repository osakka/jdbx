#ifndef SESSION_API_H
#define SESSION_API_H

#include "api/api.h"

/* Session API handlers */
http_response_t* api_handle_get_sessions(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_get_active_sessions(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_logout(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_session_terminate(api_context_t* ctx, http_request_t* request);

#endif /* SESSION_API_H */