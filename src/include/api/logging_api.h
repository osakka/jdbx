#ifndef LOGGING_API_H
#define LOGGING_API_H

#include "api/api.h"

/**
 * Register logging API endpoints
 * 
 * Endpoints:
 * - GET  /api/system/logging - Get current logging configuration
 * - PUT  /api/system/logging - Update logging configuration
 * 
 * @param ctx The API context to register routes with
 */
void register_logging_api_endpoints(api_context_t* ctx);

#endif /* LOGGING_API_H */