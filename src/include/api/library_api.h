#ifndef LIBRARY_API_H
#define LIBRARY_API_H

#include "api/api.h"

/* Library API handlers */
http_response_t* api_handle_get_libraries(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_create_library(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_delete_library(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_get_library(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_update_library(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_get_library_stats(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_copy_library(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_get_library_templates(api_context_t* ctx, http_request_t* request);

#endif /* LIBRARY_API_H */