#ifndef LIBRARY_METRICS_API_H
#define LIBRARY_METRICS_API_H

#include "api/api.h"

/* Library metrics API handlers */
http_response_t* api_handle_library_metrics(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_record_library_metric(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_query_library_metrics(api_context_t* ctx, http_request_t* request);

#endif /* LIBRARY_METRICS_API_H */