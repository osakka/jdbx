/**
 * @file api_metrics.h
 * @brief System Metrics API handlers
 * 
 * Provides RESTful API endpoints for system-wide metrics collection, 
 * monitoring, and export functionality. Supports both JSON and 
 * Prometheus/OpenMetrics formats for comprehensive metrics integration.
 */

#ifndef API_METRICS_H
#define API_METRICS_H

#include "api/api.h"

/**
 * @defgroup METRICS_API System Metrics API
 * @brief Complete system metrics and monitoring endpoints
 * 
 * Provides comprehensive system metrics collection, querying, and export
 * capabilities. Supports multiple output formats including JSON for APIs
 * and Prometheus format for monitoring systems integration.
 * 
 * @{
 */

/**
 * @defgroup METRICS_Query Metrics Query and Retrieval
 * @brief Metrics data retrieval and filtering
 * @{
 */

/**
 * @brief Get system metrics with filtering
 * 
 * Retrieves system metrics in JSON format with optional filtering
 * by category, type, and name. Supports query parameters for 
 * granular metric selection and real-time monitoring.
 * 
 * @param ctx API context with metrics registry
 * @param request HTTP request with optional query parameters
 * @return HTTP response with filtered metrics in JSON format
 * 
 * @note Query parameters: ?type=counter&name=requests&category=stats
 * @note Response format: {"metrics": [...], "timestamp": ISO8601}
 */
http_response_t* api_handle_metrics_get(api_context_t* ctx, http_request_t* request);

/**
 * @brief Get metrics in Prometheus format
 * 
 * Retrieves all system metrics formatted for Prometheus/OpenMetrics
 * consumption. Provides standard metrics exposition format for
 * integration with monitoring and alerting systems.
 * 
 * @param ctx API context with metrics registry
 * @param request HTTP request (no parameters required)
 * @return HTTP response with metrics in Prometheus format
 * 
 * @note Content-Type: text/plain (Prometheus format)
 * @note Standard exposition format for scraping by Prometheus
 */
http_response_t* api_handle_metrics(api_context_t* ctx, http_request_t* request);

/**
 * @brief List available metrics definitions
 * 
 * Retrieves metadata about all available metrics including names,
 * types, and descriptions. Useful for metrics discovery and
 * monitoring system configuration.
 * 
 * @param ctx API context with metrics registry  
 * @param request HTTP request (no parameters required)
 * @return HTTP response with metrics metadata
 * 
 * @note Response grouped by metric types: counters, gauges, timers, histograms
 * @note Includes metric descriptions and current availability
 */
http_response_t* api_handle_metrics_available(api_context_t* ctx, http_request_t* request);

/** @} */ /* End of METRICS_Query group */

/**
 * @defgroup METRICS_Export Metrics Export and Integration
 * @brief Metrics export functionality for external systems
 * @{
 */

/**
 * @brief Export metrics to file
 * 
 * Exports system metrics to specified file path in JSON format.
 * Supports batch export for backup, analysis, and external system
 * integration workflows.
 * 
 * @param ctx API context with metrics registry
 * @param request HTTP POST request with export configuration
 * @return HTTP response confirming export operation
 * 
 * @note Requires admin privileges for file system access
 * @note Request body: {"export_path": "/path/to/metrics.json"}
 * @note Validates file path permissions and disk space
 */
http_response_t* api_handle_metrics_export(api_context_t* ctx, http_request_t* request);

/** @} */ /* End of METRICS_Export group */

/** @} */ /* End of METRICS_API group */

#endif /* API_METRICS_H */