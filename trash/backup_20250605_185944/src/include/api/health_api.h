#ifndef JSONDB_HEALTH_API_H
#define JSONDB_HEALTH_API_H

#include "api/api.h"

/**
 * @file health_api.h
 * @brief Health and metrics API functions for JSONdb.
 */

/**
 * @brief Initialize the health API.
 * 
 * Sets up the health API and initializes monitoring.
 */
void health_api_init(void);

/**
 * @brief Handle health check request.
 * 
 * Returns a JSON response with health information about the server.
 * 
 * @param ctx The API context.
 * @param request The HTTP request.
 * @return An HTTP response with health information.
 */
http_response_t* api_handle_health_check(api_context_t *ctx, http_request_t *request);

/**
 * @brief Handle metrics request.
 * 
 * Returns a JSON response with all metrics collected by the server.
 * 
 * @param ctx The API context.
 * @param request The HTTP request.
 * @return An HTTP response with metrics data.
 */
http_response_t* api_handle_metrics(api_context_t *ctx, http_request_t *request);

/**
 * @brief Handle available metrics request.
 * 
 * Returns a JSON response with a list of all available metrics.
 * 
 * @param ctx The API context.
 * @param request The HTTP request.
 * @return An HTTP response with available metrics.
 */
http_response_t* api_handle_metrics_available(api_context_t *ctx, http_request_t *request);

/**
 * @brief Register health API endpoints.
 * 
 * Registers all health and metrics endpoints with the API context.
 * 
 * @param ctx The API context to register endpoints with.
 */
void register_health_api_endpoints(api_context_t *ctx);

#endif /* JSONDB_HEALTH_API_H */