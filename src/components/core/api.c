/**
 * @file api.c
 * @brief Core API routing and request handling system
 * 
 * Implements the central API dispatch system for JDBX database server,
 * handling HTTP request routing, authentication, authorization, and
 * response generation. Provides unified interface for all database
 * and administrative operations.
 * 
 * Request Processing Pipeline:
 * 1. HTTP request parsing and validation
 * 2. Route matching and parameter extraction
 * 3. Authentication verification (JWT token validation)
 * 4. Authorization checking (RBAC permission verification)
 * 5. Handler function dispatch with context
 * 6. Response formatting and error handling
 * 7. Metrics collection and logging
 * 
 * Supported Operations:
 * - Database CRUD operations (collections, documents, indexes)
 * - User management and authentication
 * - System administration and configuration
 * - Metrics and monitoring endpoints
 * - JavaScript integration and execution
 * - Library management and multi-tenancy
 * 
 * Features:
 * - RESTful API design with consistent URL patterns
 * - JWT-based stateless authentication
 * - Role-based access control with fine-grained permissions
 * - Request/response metrics and performance monitoring
 * - Comprehensive error handling with appropriate HTTP status codes
 */

#include "api/api.h"
#include "database/document_storage.h"
#include "database/virtual_layer.h"
#include "api/session_api.h"
#include "api/auth_session_api.h"
#include "api/library_api.h"
#include "api/library_metrics_api.h"
#include "api/virtual_collections_api.h"
#include "api/api_documents.h"
#include "api/api_auth.h"
#include "api/api_rbac.h"
#include "api/api_metrics.h"
#include "core/server.h"
#include "database/database.h"
#include "core/rate_limiter.h"
#include "database/document_storage.h"
#include "rbac/rbac.h"
#include "rbac/jwt.h"
#include "rbac/jwt_cache.h"
#include "rbac/rbac_db.h"
#include "js/js_function_resolver.h"
#include "utils/metrics.h"
#include "utils/logger.h"
#include "utils/buffer_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

/* External globals declaration */
#ifndef TOOLS_BUILD
extern metrics_registry_t* g_metrics_registry;
extern logger_config_t* g_logger;
#else
metrics_registry_t* g_metrics_registry = NULL;
logger_config_t* g_logger = NULL;
#endif

/* External rate limiter instance */
extern rate_limiter_t* g_rate_limiter;

/* Forward declarations */

/* API routes */
api_route_t routes[] = {
  /* Authentication routes */
  {"/api/auth/login", HTTP_POST, api_handle_login, 0},
  {"/api/auth/register", HTTP_POST, api_handle_register, 0},
  {"/api/auth/refresh", HTTP_POST, api_handle_token_refresh, 0},
  {"/api/auth/logout", HTTP_POST, api_handle_logout, 1},
  {"/api/auth/session", HTTP_GET, api_handle_get_current_session, 1},
  {"/api/auth/library", HTTP_GET, api_handle_get_library_context, 1},
  {"/api/auth/library/", HTTP_POST, api_handle_switch_library, 1},
  {"/api/auth/password", HTTP_PUT, api_handle_change_password, 1},
  
  /* Session management routes */
  {"/api/sessions", HTTP_GET, api_handle_get_sessions, 1},
  {"/api/sessions/active", HTTP_GET, api_handle_get_active_sessions, 1},
  {"/api/sessions/", HTTP_POST, api_handle_session_terminate, 1},
  {"/api/sessions/", HTTP_DELETE, api_handle_terminate_session, 1},
  
  /* Library management routes */
  {"/api/libraries", HTTP_GET, api_handle_get_libraries, 1},
  {"/api/libraries", HTTP_POST, api_handle_create_library, 1},
  {"/api/library-templates", HTTP_GET, api_handle_get_library_templates, 1},
  /* Library metrics routes - must come before generic library routes */
  {"/api/libraries/:library/metrics/query", HTTP_GET, api_handle_query_library_metrics, 1},
  {"/api/libraries/:library/metrics", HTTP_GET, api_handle_library_metrics, 1},
  {"/api/libraries/:library/metrics", HTTP_POST, api_handle_record_library_metric, 1},
  
  /* Virtual Collection routes */
  {"/api/collections", HTTP_GET, api_handle_virtual_collections_list, 1},
  {"/api/collections", HTTP_POST, api_handle_virtual_collection_create, 1},
  
  /* Unified documents routes */
  {"/api/documents", HTTP_GET, api_handle_unified_documents_query, 1},
  {"/api/documents", HTTP_POST, api_handle_unified_documents_create, 1},
  {"/api/documents/query", HTTP_POST, api_handle_unified_documents_query, 1},
  {"/api/documents/", HTTP_GET, api_handle_unified_document_get, 1},
  {"/api/documents/", HTTP_PUT, api_handle_unified_document_update, 1},
  {"/api/documents/", HTTP_DELETE, api_handle_unified_document_delete, 1},
  
  /* Document routes - Authentication enabled */
  /* NOTE: The order matters - the first matching route wins */
  
  /* Collection-scoped document routes (supports both legacy and library-scoped) */
  {"/api/collections/", HTTP_GET, api_handle_documents_query, 1}, /* General query handler - checks for /documents suffix */
  {"/api/collections/", HTTP_GET, api_handle_document_field_access, 1}, /* Field access - checks for field path */
  {"/api/collections/", HTTP_GET, api_handle_document_get, 1}, /* Specific document GET */
  {"/api/collections/", HTTP_POST, api_handle_document_create, 1},
  {"/api/collections/", HTTP_PUT, api_handle_document_update, 1},
  {"/api/collections/", HTTP_DELETE, api_handle_document_delete, 1},
  
  /* Virtual collection drop must come after document routes to avoid matching document paths */
  {"/api/collections/", HTTP_DELETE, api_handle_virtual_collection_drop, 1},
  
  /* RBAC routes */
  {"/api/users", HTTP_GET, api_handle_users_list, 1},
  {"/api/users", HTTP_POST, api_handle_user_create, 1},
  {"/api/users/", HTTP_GET, api_handle_user_get, 1},
  {"/api/users/", HTTP_PUT, api_handle_user_update, 1},
  {"/api/users/", HTTP_DELETE, api_handle_user_delete, 1},
  {"/api/roles", HTTP_GET, api_handle_roles_list, 1},
  {"/api/roles", HTTP_POST, api_handle_role_create, 1},
  {"/api/roles/", HTTP_GET, api_handle_role_get, 1},
  {"/api/roles/", HTTP_PUT, api_handle_role_update, 1},
  {"/api/roles/", HTTP_DELETE, api_handle_role_delete, 1},
  
  /* Configuration routes */
  
  /* Metrics routes */
  {"/api/metrics", HTTP_GET, health_api_handle_metrics, 1},
  {"/api/metrics/stats", HTTP_GET, health_api_handle_metrics, 1},
  {"/api/metrics/activity", HTTP_GET, health_api_handle_metrics, 1},
  {"/api/metrics/export", HTTP_POST, health_api_handle_metrics_export, 1},
  {"/api/metrics/history", HTTP_GET, api_handle_metrics_history, 1},
  {"/api/metrics/aggregate", HTTP_GET, api_handle_metrics_aggregate, 1},
  {"/api/metrics/adaptive-indexing", HTTP_GET, api_handle_adaptive_indexing_metrics, 1},
  
  /* System info routes */
  {"/api/system/info", HTTP_GET, api_handle_system_info, 1},
  {"/api/system/log-control", HTTP_GET, api_handle_log_control, 1},
  {"/api/system/log-control", HTTP_POST, api_handle_log_control, 1},
  
  /* OpenAPI specification route */
  {"/api/openapi.json", HTTP_GET, api_handle_openapi_spec, 0},
  
  /* Data visualization routes */
  {"/api/visualization/collection-stats", HTTP_GET, api_handle_visualization_collection_stats, 1},
  {"/api/visualization/document-types", HTTP_GET, api_handle_visualization_document_types, 1},
  {"/api/visualization/field-distribution", HTTP_GET, api_handle_visualization_field_distribution, 1},
  
  /* Transaction visualization routes */
  {"/api/visualization/transaction-history", HTTP_GET, api_handle_visualization_transaction_history, 1},
  {"/api/visualization/transaction-metrics", HTTP_GET, api_handle_visualization_transaction_metrics, 1},
  {"/api/visualization/transaction-relationships", HTTP_GET, api_handle_visualization_transaction_relationships, 1},
  
  /* Backup and restore routes */
  /* Backup functionality not yet implemented
  {"/api/backup", HTTP_POST, api_handle_backup_create, 1},
  {"/api/backup", HTTP_GET, api_handle_backup_list, 1},
  {"/api/backup/restore", HTTP_POST, api_handle_backup_restore, 1},
  {"/api/backup/", HTTP_DELETE, api_handle_backup_delete, 1},
  */
  {"/api/export", HTTP_POST, api_handle_export, 1},
  {"/api/import", HTTP_POST, api_handle_import, 1},
  
  /* Authentication routes */
  {"/api/login", HTTP_POST, api_handle_login, 0},
  
  /* Admin auth routes */
  {"/api/admin/login", HTTP_POST, api_handle_admin_login, 0},
  {"/api/admin/test", HTTP_GET, api_handle_admin_test, 0},

  /* Health and monitoring routes */
  {"/health", HTTP_GET, api_handle_health_check, 0},
  {"/metrics", HTTP_GET, health_api_handle_metrics, 0},
  {"/metrics/available", HTTP_GET, health_api_handle_metrics_available, 0},
  
  /* Schema validation routes */
  {"/api/schemas", HTTP_GET, api_handle_schema_get, 0},
  {"/api/schemas", HTTP_POST, api_handle_schema_create, 0},
  {"/api/schemas/", HTTP_GET, api_handle_schema_get, 0},
  {"/api/schemas/", HTTP_PUT, api_handle_schema_update, 0},
  {"/api/schemas/", HTTP_DELETE, api_handle_schema_delete, 0},
  {"/api/validate", HTTP_POST, api_handle_schema_validate, 1},
  
  /* Index routes */
  {"/api/indexes/", HTTP_GET, api_handle_index_list, 1},
  {"/api/indexes/", HTTP_POST, api_handle_index_create, 1},
  {"/api/indexes/", HTTP_GET, api_handle_index_get, 1},
  {"/api/indexes/", HTTP_DELETE, api_handle_index_delete, 1},
  {"/api/indexes/rebuild/", HTTP_POST, api_handle_index_rebuild, 1},
  {"/api/indexes/stats/", HTTP_GET, api_handle_index_stats, 1},
  {"/api/indexes/query/", HTTP_POST, api_handle_index_query, 1},
  {"/api/indexes/compound/", HTTP_POST, api_handle_index_compound_query, 1},
  
  /* JavaScript routes */
  {"/api/js/query", HTTP_POST, api_handle_js_query, 1},
  {"/api/js/eval", HTTP_POST, api_handle_js_eval, 1},
  {"/api/js/functions", HTTP_POST, api_handle_js_function_register, 1},
  {"/api/js/functions/", HTTP_POST, api_handle_js_function_execute, 1},
  {"/api/js/validators", HTTP_POST, api_handle_js_validator_register, 1},
  {"/api/js/transformers", HTTP_POST, api_handle_js_transformer_register, 1},
  
  /* Cache routes */
  {"/api/cache/stats", HTTP_GET, api_handle_cache_stats, 1},
  {"/api/cache/configure", HTTP_POST, api_handle_cache_configure, 1},
  {"/api/cache/clear", HTTP_POST, api_handle_cache_clear, 1},
  {"/api/cache/invalidate", HTTP_POST, api_handle_cache_invalidate, 1},
  
  /* Transaction routes */
  {"/api/transactions", HTTP_POST, api_handle_transaction_begin, 1}, /* Create new transaction */
  
  /* 
   * The following routes use a prefix match mechanism. The actual URL paths are:
   * - Transaction commit: /api/transactions/:id/commit
   * - Transaction rollback: /api/transactions/:id/rollback
   * - Transaction isolation: /api/transactions/:id/isolation
   * - Transaction timeout: /api/transactions/:id/timeout
   * - Transaction document operations: /api/transactions/:id/collections/:collection/documents[/:id]
   * 
   * The handlers contain logic to parse these paths and handle them differently.
   */
  
  /* Transaction commit route */
  {"/api/transactions/", HTTP_POST, api_handle_transaction_commit, 1},
  
  /* We need to list the rollback route first, since it's more specific in the handler code */
  {"/api/transactions/", HTTP_DELETE, api_handle_transaction_rollback, 1},
  
  /* Transaction configuration routes */
  {"/api/transactions/", HTTP_PATCH, api_handle_transaction_set_isolation, 1},
  {"/api/transactions/", HTTP_PATCH, api_handle_transaction_set_timeout, 1},
  
  /* Transaction savepoint routes */
  {"/api/transactions/", HTTP_POST, api_handle_transaction_create_savepoint, 1},
  {"/api/transactions/", HTTP_POST, api_handle_transaction_rollback_to_savepoint, 1},
  {"/api/transactions/", HTTP_DELETE, api_handle_transaction_release_savepoint, 1},
  
  /* Transaction status and metrics routes */
  {"/api/transactions/metrics", HTTP_GET, api_handle_transaction_metrics, 1},
  {"/api/transactions/check-deadlocks", HTTP_POST, api_handle_transaction_check_deadlocks, 1},
  {"/api/transactions/", HTTP_GET, api_handle_transaction_status, 1},
  
  /* Transaction log and audit trail routes */
  {"/api/transactions/logs", HTTP_GET, api_handle_transaction_logs, 1},
  {"/api/transactions/logs/configure", HTTP_POST, api_handle_transaction_logs_configure, 1},
  {"/api/transactions/logs/archive", HTTP_POST, api_handle_transaction_logs_archive, 1},
  {"/api/transactions/logs/report", HTTP_GET, api_handle_transaction_logs_report, 1},
  {"/api/transactions/logs/document-history", HTTP_GET, api_handle_transaction_logs_document_history, 1},
  
  /* Document operations - these are handled after the more specific paths */
  {"/api/transactions/", HTTP_GET, api_handle_transaction_document_operation, 1},
  {"/api/transactions/", HTTP_POST, api_handle_transaction_document_operation, 1},
  {"/api/transactions/", HTTP_PUT, api_handle_transaction_document_operation, 1},
  
  /* 
   * Document delete - needs special care to distinguish from transaction rollback
   * This works because api_handle_transaction_rollback checks for path ending in "/rollback"
   * while api_handle_transaction_document_operation looks for "/documents/" in the path
   */
  {"/api/transactions/", HTTP_DELETE, api_handle_transaction_document_operation, 1},

  /* Library-scoped document routes - must come AFTER other library routes */
  {"/api/libraries/", HTTP_GET, api_handle_documents_query, 1}, /* Handles /api/libraries/:lib/collections/:col/documents */
  {"/api/libraries/", HTTP_POST, api_handle_document_create, 1}, /* Handles /api/libraries/:lib/collections/:col/documents */
  /* TODO: Update these handlers to support library-scoped paths
  {"/api/libraries/", HTTP_PUT, api_handle_document_update, 1},
  {"/api/libraries/", HTTP_DELETE, api_handle_document_delete, 1},
  */
  
  /* Library management routes - must come AFTER library document routes */
  {"/api/libraries/", HTTP_DELETE, api_handle_delete_library, 1},
  {"/api/libraries/", HTTP_GET, api_handle_library_document_field_access, 1},
  {"/api/libraries/", HTTP_PUT, api_handle_update_library, 1},
  {"/api/libraries/", HTTP_POST, api_handle_copy_library, 1},
  
  /* End of routes */
  {NULL, HTTP_UNKNOWN, NULL, 0}
};

/* Helper function to get string value from JSON object */
__attribute__((unused)) static const char* json_object_get_string(json_value_t* object, const char* key) {
  if (!object || object->type != JSON_OBJECT || !key) {
    return NULL;
  }
  
  json_value_t* value = json_object_get(object, key);
  if (!value || value->type != JSON_STRING) {
    return NULL;
  }
  
  return value->value.string;
}

/* Create API context */

api_context_t* api_create_context(database_t* db, rbac_system_t* rbac, const char* jwt_secret) {
  if (!db || !rbac || !jwt_secret) {
    if (g_logger) {
      LOG_ERROR("API context creation failed: Missing required components.");
    }
    return NULL;
  }

  api_context_t* ctx = (api_context_t*)BUFFER_ALLOC(sizeof(api_context_t));
  if (!ctx) {
    if (g_logger) {
      LOG_ERROR("API context creation failed: Memory allocation failed.");
    }
    return NULL;
  }
  memory_promote(ctx);  /* API context survives checkpoints */

  /* Initialize basic fields */
  ctx->db = db;
  ctx->rbac = rbac;
  ctx->jwt_secret = BUFFER_STRDUP(jwt_secret);
  if (!ctx->jwt_secret) {
    BUFFER_FREE(ctx);
    if (g_logger) {
      LOG_ERROR("API context creation failed: JWT secret copy failed.");
    }
    return NULL;
  }
  memory_promote((void*)ctx->jwt_secret);  /* Part of API context */

  /* Initialize transaction manager with capacity for 100 concurrent transactions */
  ctx->transaction_manager = transaction_manager_create(db, 100);
  if (!ctx->transaction_manager) {
    BUFFER_FREE((void*)ctx->jwt_secret);
    BUFFER_FREE(ctx);
    if (g_logger) {
      LOG_ERROR("API context creation failed: Transaction manager creation failed.");
    }
    return NULL;
  }

  /* Count the number of static routes */
  int num_routes = 0;
  while (routes[num_routes].path != NULL) {
    num_routes++;
  }

  /* Allocate memory for routes (50 extra slots for dynamic registration) */
  ctx->max_routes = num_routes + 50;
  ctx->routes = (api_route_t*)BUFFER_ALLOC(ctx->max_routes * sizeof(api_route_t));
  if (!ctx->routes) {
    transaction_manager_free(ctx->transaction_manager);
    BUFFER_FREE((void*)ctx->jwt_secret);
    BUFFER_FREE(ctx);
    if (g_logger) {
      LOG_ERROR("API context creation failed: Routes array allocation failed.");
    }
    return NULL;
  }
  memory_promote(ctx->routes);  /* Part of API context */

  /* Copy the routes */
  for (int i = 0; i < num_routes; i++) {
    ctx->routes[i] = routes[i];
  }
  ctx->num_routes = num_routes;

  /* No need to initialize metrics registry, not in this struct anymore */
  
  if (g_logger) {
    LOG_INFO("API context created with %d routes", ctx->num_routes);
  }
  
  return ctx;
}

/* Free API context */
void api_free_context(api_context_t* ctx) {
  if (ctx) {
    if (ctx->transaction_manager) {
      transaction_manager_free(ctx->transaction_manager);
    }
    if (ctx->jwt_secret) {
      BUFFER_FREE((void*)ctx->jwt_secret);
    }
    if (ctx->routes) {
      BUFFER_FREE(ctx->routes);
    }
    BUFFER_FREE(ctx);
  }
}

/* Extract JWT token from request */
char* api_extract_token(http_request_t* request) {
  if (!request || !request->authorization) {
    return NULL;
  }
  
  /* Check for "Bearer" prefix */
  if (strncasecmp(request->authorization, "Bearer ", 7) != 0) {
    return NULL;
  }
  
  /* Skip "Bearer " prefix */
  return BUFFER_STRDUP(request->authorization + 7);
}


/* Route matching */
static int route_matches(const char* route, const char* path) {
  if (g_logger) {
    TRACE_API("ROUTE_MATCH_CHECK: route='%s', path='%s'", route ? route : "NULL", path ? path : "NULL");
  }
  
  /* Exact match */
  if (strcmp(route, path) == 0) {
    if (g_logger) {
      TRACE_API("ROUTE_MATCH_EXACT: route='%s' matches path='%s'", route, path);
    }
    return 1;
  }
  
  /* Check for parameterized routes (e.g., /api/rbac/roles/:id) */
  const char* route_ptr = route;
  const char* path_ptr = path;
  
  while (*route_ptr && *path_ptr) {
    /* Check for parameter placeholder */
    if (*route_ptr == ':') {
      /* Skip the parameter name in the route */
      while (*route_ptr && *route_ptr != '/') {
        route_ptr++;
      }
      
      /* Skip the actual value in the path */
      while (*path_ptr && *path_ptr != '/') {
        path_ptr++;
      }
    } else {
      /* Characters must match exactly */
      if (*route_ptr != *path_ptr) {
        break;
      }
      route_ptr++;
      path_ptr++;
    }
  }
  
  /* Both strings should be at the end for a match */
  if (*route_ptr == '\0' && *path_ptr == '\0') {
    return 1;
  }
  
  /* Prefix match with trailing '/' */
  size_t route_len = strlen(route);
  if (route[route_len - 1] == '/') {
    int prefix_match = strncmp(route, path, route_len) == 0;
    if (g_logger) {
      TRACE_API("ROUTE_MATCH_PREFIX: route='%s' (len=%zu) vs path='%s', match=%d", 
          route, route_len, path, prefix_match);
    }
    return prefix_match;
  }
  
  if (g_logger) {
    TRACE_API("ROUTE_MATCH_NONE: route='%s' does not match path='%s'", route, path);
  }
  return 0;
}

/* Dispatch request to appropriate handler */
http_response_t* api_dispatch_request(api_context_t* ctx, http_request_t* request) {
  
  if (g_logger) {
    TRACE_API("API_DISPATCH_ENTRY: ctx=%p, request=%p", (void*)ctx, (void*)request);
  }
  
  if (!ctx || !request) {
    if (g_logger) {
      LOG_ERROR("API dispatch failed: Invalid context or request.");
      LOG_ERROR("Context=%p, Request=%p", (void*)ctx, (void*)request);
      TRACE_API("API_DISPATCH_EXIT: returning 500 - invalid params.");
    }
    printf("API dispatch failed: Invalid context or request. Context=%p, Request=%p\n", (void*)ctx, (void*)request);
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Internal server error\"}", "application/json");
  }
  
  /* Create memory checkpoint for this request */
  memory_checkpoint_t* request_checkpoint = memory_checkpoint_create();
  if (request_checkpoint) {
    if (g_logger) LOG_DEBUG("Created memory checkpoint %p for API request: %s %s", 
              request_checkpoint,
              request->method == HTTP_GET ? "GET" : 
              request->method == HTTP_POST ? "POST" : 
              request->method == HTTP_PUT ? "PUT" : 
              request->method == HTTP_DELETE ? "DELETE" : "UNKNOWN",
              request->path ? request->path : "<null>");
  } else {
    if (g_logger) LOG_WARNING("Failed to create memory checkpoint for request");
  }
  
  if (!request->path) {
    if (g_logger) LOG_ERROR("API dispatch failed: Request has NULL path.");
    printf("API dispatch failed: Request has NULL path\n");
    
    /* Rewind checkpoint before returning error */
    if (request_checkpoint) {
        memory_checkpoint_rewind(request_checkpoint);
        request_checkpoint = NULL;
    }
    
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Internal server error\"}", "application/json");
  }
  
  printf("API dispatch: Processing request for path '%s'\n", request->path);
  printf("API context: Routes=%p, num_routes=%d\n", (void*)ctx->routes, ctx->num_routes);
  
  /* Check rate limiting before processing request */
  if (g_rate_limiter && request->remote_addr) {
    if (!rate_limiter_check_request(g_rate_limiter, request->remote_addr)) {
      if (g_logger) LOG_WARNING("Rate limit exceeded for IP: %s on path: %s", 
                               request->remote_addr, request->path);
      
      /* Rewind checkpoint before returning error */
      if (request_checkpoint) {
          memory_checkpoint_rewind(request_checkpoint);
          request_checkpoint = NULL;
      }
      
      http_response_t* response = create_http_response(HTTP_TOO_MANY_REQUESTS, 
                                   "{\"error\":\"Rate limit exceeded\"}", "application/json");
      /* Add Retry-After header */
      response->headers = BUFFER_ALLOC(sizeof(char*) * 2);
      response->headers[0] = BUFFER_STRDUP("Retry-After: 60");
      response->num_headers = 1;
      
      return response;
    }
    
    /* Token already consumed in rate_limiter_check_request */
  }
  
  if (g_logger) LOG_DEBUG("Dispatching request: %s %s (searching %d routes)", 
              request->method == HTTP_GET ? "GET" : 
              request->method == HTTP_POST ? "POST" : 
              request->method == HTTP_PUT ? "PUT" : 
              request->method == HTTP_DELETE ? "DELETE" :
              request->method == HTTP_OPTIONS ? "OPTIONS" : "UNKNOWN",
              request->path, ctx->num_routes);
              
  /* Handle OPTIONS requests (CORS preflight) */
  if (request->method == HTTP_OPTIONS) {
    if (g_logger) LOG_DEBUG("Handling OPTIONS preflight request for CORS.");
    
    http_response_t* response = create_http_response(HTTP_OK, "", "text/plain");
    
    /* Add CORS headers */
    add_response_header(response, "Access-Control-Allow-Origin: *");
    add_response_header(response, "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS");
    add_response_header(response, "Access-Control-Allow-Headers: Content-Type, Authorization");
    add_response_header(response, "Access-Control-Allow-Credentials: true");
    add_response_header(response, "Access-Control-Max-Age: 86400");
    
    /* Commit checkpoint for successful OPTIONS response */
    if (request_checkpoint) {
        memory_checkpoint_commit(request_checkpoint);
        request_checkpoint = NULL;
    }
    
    return response;
  }
  
  /* Find matching route */
  if (g_logger) {
    TRACE_API("API_ROUTE_SEARCH: Searching %d routes for %s %s", 
        ctx->num_routes, 
        request->method == HTTP_GET ? "GET" :
        request->method == HTTP_POST ? "POST" :
        request->method == HTTP_PUT ? "PUT" :
        request->method == HTTP_DELETE ? "DELETE" : "UNKNOWN",
        request->path);
  }
  
  for (int i = 0; i < ctx->num_routes; i++) {
    if (g_logger) {
      TRACE_API("API_ROUTE_CHECK[%d]: route='%s', method=%d (want %d)", 
          i, ctx->routes[i].path, ctx->routes[i].method, request->method);
    }
    
    if (route_matches(ctx->routes[i].path, request->path) && ctx->routes[i].method == request->method) {
      if (g_logger) {
        TRACE_API("API_ROUTE_MATCHED[%d]: route='%s' matched!", i, ctx->routes[i].path);
      }
      if (g_logger) LOG_DEBUG("Found matching route: %s (requires_auth: %d)", 
                ctx->routes[i].path, ctx->routes[i].requires_auth);
      
      /* Check if route requires authentication */
      if (ctx->routes[i].requires_auth) {
        if (g_logger) LOG_DEBUG("Route requires authentication, checking token.");
        if (!api_authenticate_request_sliding(ctx, request)) {
          if (g_logger) LOG_WARNING("Authentication failed for route: %s", ctx->routes[i].path);
          
          /* Rewind checkpoint on auth failure */
          if (request_checkpoint) {
              memory_checkpoint_rewind(request_checkpoint);
              request_checkpoint = NULL;
          }
          
          return create_http_response(HTTP_UNAUTHORIZED, 
                       "{\"error\":\"Unauthorized\"}", "application/json");
        }
      }
      
      if (g_logger) LOG_DEBUG("Calling handler for route: %s", ctx->routes[i].path);
      if (g_logger) LOG_DEBUG("Handler function pointer: %p", (void*)ctx->routes[i].handler);
      
      /* Call handler */
      if (g_logger) LOG_DEBUG("About to call handler function.");
      
      /* Increment request counter */
      metric_t* request_counter = get_server_requests_metric();
      if (request_counter) {
          metrics_counter_inc(request_counter, 1);
      }
      
      http_response_t* result = ctx->routes[i].handler(ctx, request);
      if (g_logger) LOG_DEBUG("Handler function returned: %p", (void*)result);
      
      /* SELECTIVE RESPONSE PROMOTION: Only promote responses that truly need 
       * to survive checkpoint operations. Most responses are immediately transmitted
       * and should use automatic checkpoint cleanup to prevent memory leaks.
       * 
       * Promotion criteria:
       * - Large responses (>64KB) that might be transmitted asynchronously
       * - Streaming responses or chunked transfer
       * - Static files that require keep-alive transmission
       * 
       * This fixes the critical memory leak where ALL responses were promoted,
       * causing unbounded memory growth under concurrent load. */
      bool needs_promotion = false;
      
      if (request_checkpoint && result) {
          /* Check if response needs promotion to survive checkpoint operations */
          
          /* Large response bodies that might be transmitted asynchronously */
          if (result->body && strlen(result->body) > 65536) {  /* 64KB threshold */
              needs_promotion = true;
          }
          
          /* Static file responses (typically large HTML/CSS/JS files) */
          if (result->content_type && 
              (strstr(result->content_type, "text/html") ||
               strstr(result->content_type, "text/css") ||
               strstr(result->content_type, "application/javascript") ||
               strstr(result->content_type, "text/javascript"))) {
              /* Only promote if large enough to warrant async transmission */
              if (result->body && strlen(result->body) > 32768) {  /* 32KB for static files */
                  needs_promotion = true;
              }
          }
          
          /* Streaming or chunked responses (future extension point) */
          if (result->headers) {
              for (size_t j = 0; j < result->num_headers; j++) {
                  if (result->headers[j] && 
                      (strstr(result->headers[j], "Transfer-Encoding: chunked") ||
                       strstr(result->headers[j], "Content-Encoding: gzip"))) {
                      needs_promotion = true;
                      break;
                  }
              }
          }
          
          /* Apply selective promotion */
          if (needs_promotion) {
              memory_promote(result);
              if (result->body) memory_promote(result->body);
              if (result->content_type) memory_promote(result->content_type);
              if (result->headers) {
                  memory_promote(result->headers);
                  for (size_t j = 0; j < result->num_headers; j++) {
                      if (result->headers[j]) {
                          memory_promote(result->headers[j]);
                      }
                  }
              }
              LOG_DEBUG("Promoted large response (body_size=%zu) to survive checkpoint", 
                       result->body ? strlen(result->body) : 0);
          } else {
              /* Most responses use automatic checkpoint cleanup - no promotion needed */
              LOG_TRACE("Response will use automatic checkpoint cleanup (body_size=%zu)", 
                       result->body ? strlen(result->body) : 0);
          }
      }
      
      /* Track errors */
      if (result && result->status >= 400) {
          metric_t* error_counter = get_api_errors_metric();
          if (error_counter) {
              metrics_counter_inc(error_counter, 1);
          }
          
          /* Rewind memory checkpoint on error */
          if (request_checkpoint) {
              memory_checkpoint_rewind(request_checkpoint);
              request_checkpoint = NULL;
              /* LOG_DEBUG("Rewound memory checkpoint due to error response"); */
          }
      } else {
          /* Commit memory checkpoint on success */
          if (request_checkpoint) {
              memory_checkpoint_commit(request_checkpoint);
              request_checkpoint = NULL;
              /* LOG_DEBUG("Committed memory checkpoint for successful request"); */
          }
      }
      
      return result;
    }
  }
  
  /* No matching route */
  if (g_logger) {
    LOG_WARNING("No matching route found for: %s", request->path);
    TRACE_API("Available routes:");
    // Show last 20 routes since RBAC routes are added at the end
    int start = ctx->num_routes > 20 ? ctx->num_routes - 20 : 0;
    for (int i = start; i < ctx->num_routes; i++) {
      TRACE_API(" Route %d: %s (method: %d)", i, ctx->routes[i].path, ctx->routes[i].method);
    }
    if (start > 0) {
      TRACE_API(" ... showing last 20 of %d routes", ctx->num_routes);
    }
  }
  
  /* Rewind checkpoint before returning error */
  if (request_checkpoint) {
      memory_checkpoint_rewind(request_checkpoint);
      request_checkpoint = NULL;
  }
  
  return create_http_response(HTTP_NOT_FOUND, 
               "{\"error\":\"Not found\"}", "application/json");
}

/* Authentication handlers */

/* Login handler - implemented in api_auth.c as part of Phase 2.2 */
/* Token refresh handler - implemented in api_auth.c */
/* Register handler - implemented in api_auth.c */

/* Collection handlers */

/* REMOVED: Old collections handler moved to virtual_collections_api.c */

/* Create collection */
/* Helper function to extract user info from request - safe version using existing auth system */
__attribute__((unused)) static int get_request_user_info(http_request_t* request, char* user_id_out, char* username_out, size_t buffer_size) {
  if (!request || !user_id_out || !username_out || buffer_size < 1) {
    return 0;
  }
  
  /* Initialize output buffers */
  user_id_out[0] = '\0';
  username_out[0] = '\0';
  
  /* Check if authorization header exists */
  if (!request->authorization) {
    return 0;
  }
  
  /* Extract token from Authorization header */
  const char* auth = request->authorization;
  const char* token_str = NULL;
  
  /* Check for "Bearer " prefix */
  if (strncmp(auth, "Bearer ", 7) == 0) {
    token_str = auth + 7;
  } else {
    token_str = auth;
  }
  
  /* Validate token string */
  if (!token_str || strlen(token_str) < 10) {
    return 0; /* Token too short to be valid */
  }
  
  /* Safely decode the JWT with error handling */
  jwt_token_t* token = jwt_decode(token_str);
  
  if (!token) {
    return 0;
  }
  
  /* Get the subject (user_id) from the token */
  if (!token->payload || !token->payload->sub) {
    jwt_free(token);
    return 0;
  }
  
  /* Copy the user_id */
  strncpy(user_id_out, token->payload->sub, buffer_size - 1);
  user_id_out[buffer_size - 1] = '\0';
  
  /* Get the username from the token claims */
  if (token->payload->claims) {
    json_value_t* username_val = json_object_get(token->payload->claims, "username");
    if (username_val && username_val->type == JSON_STRING) {
      strncpy(username_out, username_val->value.string, buffer_size - 1);
      username_out[buffer_size - 1] = '\0';
    }
  }
  
  jwt_free(token);
  return 1; /* Success */
}

/* Orphaned collection create handler - superseded by api_handle_virtual_collection_create */
#if 0
http_response_t* api_handle_collection_create(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request || !request->body) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  /* SECURITY: Extract user info for permission checking */
  char user_id[256];
  char username[256];
  
  if (!get_request_user_info(request, user_id, username, sizeof(user_id))) {
    return create_http_response(HTTP_UNAUTHORIZED, 
                 "{\"error\":\"Authentication required\"}", "application/json");
  }
  
  /* Parse request body */
  json_value_t* body = json_parse(request->body);
  if (!body || body->type != JSON_OBJECT) {
    if (body) {

        /* CHECKPOINT: json_free(body); */

    }
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request body\"}", "application/json");
  }
  
  /* Extract collection name */
  json_value_t* name_val = json_object_get(body, "name");
  if (!name_val || name_val->type != JSON_STRING) {
    /* CHECKPOINT: json_free(body); */
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Collection name required\"}", "application/json");
  }
  
  const char* name = name_val->value.string;
  
  /* Extract library (optional, defaults to session library) */
  json_value_t* library_val = json_object_get(body, "library");
  char* session_library = NULL;
  const char* library = NULL;
  
  if (library_val && library_val->type == JSON_STRING) {
    library = library_val->value.string;
  } else {
    /* Default to "default" library if not specified */
    session_library = BUFFER_STRDUP("default");
    library = session_library;
  }
  
  /* SECURITY: Permission checking for collection creation */
  
  /* 1. Check if trying to create in system library - admin only */
  if (library && strcmp(library, "system") == 0) {
    if (!rbac_db_check_permission(ctx->db, user_id, RBAC_COLLECTION, "system", RBAC_ADMIN)) {
      /* CHECKPOINT: json_free(body); */
      if (session_library) BUFFER_FREE(session_library);
      return create_http_response(HTTP_FORBIDDEN, 
                   "{\"error\":\"Admin permission required for system library\"}", "application/json");
    }
  }
  
  /* 2. User namespace enforcement - users can only create in their own namespace */
  if (library && strcmp(library, "system") != 0) {
    /* Users can create collections in default library or their username library */
    if (strcmp(library, "default") != 0 && strcmp(library, username) != 0) {
      /* Check if user has explicit permission for this library */
      if (!rbac_db_check_permission(ctx->db, user_id, RBAC_COLLECTION, library, RBAC_WRITE)) {
        /* CHECKPOINT: json_free(body); */
        if (session_library) BUFFER_FREE(session_library);
        return create_http_response(HTTP_FORBIDDEN, 
                     "{\"error\":\"Can only create collections in 'default' library or your username library\"}", "application/json");
      }
    }
  }
  
  /* 3. Check collection name - prevent system collection names */
  if (strncmp(name, "system_", 7) == 0 || strncmp(name, "_system", 7) == 0) {
    if (!rbac_db_check_permission(ctx->db, user_id, RBAC_COLLECTION, "system", RBAC_ADMIN)) {
      /* CHECKPOINT: json_free(body); */
      if (session_library) BUFFER_FREE(session_library);
      return create_http_response(HTTP_FORBIDDEN, 
                   "{\"error\":\"Admin permission required for system collection names\"}", "application/json");
    }
  }
  
  /* Get username from token for ownership */
  char owner_username_buffer[256];
  strcpy(owner_username_buffer, SYSTEM_USER_ADMIN);
  const char* owner_username = owner_username_buffer;
  
  char* token = api_extract_token(request);
  if (token) {
    jwt_token_t* jwt = jwt_decode(token);
    if (jwt && jwt->payload && jwt->payload->claims) {
      json_value_t* username_claim = json_object_get(jwt->payload->claims, "username");
      if (username_claim && username_claim->type == JSON_STRING) {
        strncpy(owner_username_buffer, username_claim->value.string, sizeof(owner_username_buffer) - 1);
        owner_username_buffer[sizeof(owner_username_buffer) - 1] = '\0';
      }
    }
    if (jwt) jwt_free(jwt);
    BUFFER_FREE(token);
  }
  
  /* Create collection metadata in unified documents */
  json_value_t* coll_doc = json_create_object();
  add_document_system_fields(coll_doc, "collection", library, "collections", owner_username);
  json_object_set(coll_doc, "name", json_create_string(name));
  json_object_set(coll_doc, "library", json_create_string(library));
  json_object_set(coll_doc, "is_system", json_create_boolean(0));
  
  /* Add default settings */
  json_value_t* settings = json_object_get(body, "settings");
  if (settings) {
    json_object_set(coll_doc, "settings", json_clone(settings));
  } else {
    json_object_set(coll_doc, "settings", json_create_object());
  }
  
  /* Insert collection metadata using virtual layer - single source of truth */
  json_value_t* result = virtual_insert(ctx->db, "collection", library, "collections", coll_doc, "system");
  /* CHECKPOINT: json_free(coll_doc); */
  
  if (!result) {
    /* CHECKPOINT: json_free(body); */
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to create collection metadata\"}", "application/json");
  }
  
  /* Also create the actual collection */
  char collection_path[256];
  snprintf(collection_path, sizeof(collection_path), "%s/%s", library, name);
  if (db_create_collection(ctx->db, collection_path) != 0) {
    /* Collection creation failed, but metadata exists - not ideal but not fatal */
    LOG_WARNING("Collection metadata created but physical collection failed: %s", collection_path);
  }
  
  char* response_str = json_stringify(result);
  /* CHECKPOINT: json_free(result); */
  /* CHECKPOINT: json_free(body); */
  
  /* Free session library if allocated */
  if (session_library) {
    BUFFER_FREE(session_library);
  }
  
  return create_http_response(HTTP_CREATED, response_str, "application/json");
}
#endif /* Orphaned collection create handler */

/* Drop collection */
/* REMOVED: Old collection drop handler moved to virtual_collections_api.c */


/* Placeholder for remaining API handlers */

/* RBAC handlers - MOVED TO api_rbac.c as part of Phase 2.4 API module extraction */
#if 0
http_response_t* api_handle_users_list(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  if (!ctx->rbac) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"RBAC system not initialized\"}", "application/json");
  }
  
  /* Check if the user has admin privileges */
  /* Extract token and get user ID */
  char* token = api_extract_token(request);
  if (!token) {
    /* This should not happen since authorization is already checked in api_dispatch_request */
    return create_http_response(HTTP_UNAUTHORIZED, 
                 "{\"error\":\"Unauthorized\"}", "application/json");
  }
  
  /* Decode token */
  jwt_token_t* jwt = jwt_decode(token);
  BUFFER_FREE(token);
  
  if (!jwt || !jwt->payload || !jwt->payload->sub) {
    if (jwt) jwt_free(jwt);
    return create_http_response(HTTP_UNAUTHORIZED, 
                 "{\"error\":\"Invalid token\"}", "application/json");
  }
  
  const char* user_id = jwt->payload->sub;
  
  /* Check if user has permission to list users */
  if (!rbac_check_permission(ctx->rbac, user_id, RBAC_USER, "*", RBAC_READ)) {
    jwt_free(jwt);
    return create_http_response(HTTP_FORBIDDEN, 
                 "{\"error\":\"Permission denied\"}", "application/json");
  }
  
  jwt_free(jwt);
  
  /* SINGLE SOURCE OF TRUTH: Query users using virtual layer */
  json_value_t* users_query = json_create_object();
  json_object_set(users_query, "type", json_create_string(DOC_TYPE_NAME_USER));
  json_object_set(users_query, "library", json_create_string("system"));
  
  json_value_t* users_results = virtual_query(ctx->db, DOC_TYPE_NAME_USER, "system", VIRTUAL_COLLECTION_USERS, users_query);
  /* CHECKPOINT: json_free(users_query); */
  
  if (!users_results) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to query users\"}", "application/json");
  }
  
  json_value_t* users_docs = json_object_get(users_results, "documents");
  if (!users_docs || users_docs->type != JSON_ARRAY) {
    /* CHECKPOINT: json_free(users_results); */
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Invalid users response\"}", "application/json");
  }
  
  /* Create a JSON array of users */
  json_value_t* users_array = json_create_array();
  if (!users_array) {
    /* CHECKPOINT: json_free(users_results); */
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to create users array\"}", "application/json");
  }
  
  /* Process each user document */
  for (size_t i = 0; i < json_array_size(users_docs); i++) {
    json_value_t* user_doc = json_array_get(users_docs, i);
    if (user_doc && user_doc->type == JSON_OBJECT) {
      /* Create a new user object with a subset of information (exclude password hash) */
      json_value_t* user = json_create_object();
      
      /* Add user ID from uuid field */
      json_value_t* uuid = json_object_get(user_doc, "uuid");
      if (uuid && uuid->type == JSON_STRING) {
        json_object_set(user, "id", json_create_string(uuid->value.string));
      }
      
      /* Add username if present */
      json_value_t* username = json_object_get(user_doc, "username");
      if (username && username->type == JSON_STRING) {
        json_object_set(user, "username", json_create_string(username->value.string));
      }
      
      /* Add roles array if present */
      json_value_t* roles = json_object_get(user_doc, "roles");
      if (roles && roles->type == JSON_ARRAY) {
        /* Create a deep copy of the roles array */
        json_value_t* roles_copy = json_create_array();
        for (size_t j = 0; j < roles->value.array.size; j++) {
          json_value_t* role_id = roles->value.array.items[j];
          if (role_id && role_id->type == JSON_STRING) {
            json_array_append(roles_copy, json_create_string(role_id->value.string));
          }
        }
        json_object_set(user, "roles", roles_copy);
      }
      
      /* Add user to array */
      json_array_append(users_array, user);
    }
  }
  
  /* CHECKPOINT: json_free(users_results); */
  
  /* Create response object */
  json_value_t* response = json_create_object();
  json_object_set(response, "users", users_array);
  
  /* Serialize response */
  char* response_str = json_stringify(response);
  
  /* Free resources */
  /* CHECKPOINT: json_free(response); */
  
  /* Create and return response */
  return create_http_response(HTTP_OK, response_str, "application/json");
}

http_response_t* api_handle_user_get(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  if (!ctx->rbac) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"RBAC system not initialized\"}", "application/json");
  }
  
  /* Extract user ID from path */
  const char* path = request->path;
  if (strncmp(path, "/api/users/", 11) != 0) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid path\"}", "application/json");
  }
  
  const char* target_user_id = path + 11;
  
  /* Check if the user has appropriate permissions */
  /* Extract token and get user ID */
  char* token = api_extract_token(request);
  if (!token) {
    /* This should not happen since authorization is already checked in api_dispatch_request */
    return create_http_response(HTTP_UNAUTHORIZED, 
                 "{\"error\":\"Unauthorized\"}", "application/json");
  }
  
  /* Decode token */
  jwt_token_t* jwt = jwt_decode(token);
  BUFFER_FREE(token);
  
  if (!jwt || !jwt->payload || !jwt->payload->sub) {
    if (jwt) jwt_free(jwt);
    return create_http_response(HTTP_UNAUTHORIZED, 
                 "{\"error\":\"Invalid token\"}", "application/json");
  }
  
  const char* requester_user_id = jwt->payload->sub;
  
  /* Check if user has permission to view the target user 
   * The permission check is more permissive if the user is viewing their own profile
   */
  if (strcmp(requester_user_id, target_user_id) != 0 && 
    !rbac_check_permission(ctx->rbac, requester_user_id, RBAC_USER, target_user_id, RBAC_READ) &&
    !rbac_check_permission(ctx->rbac, requester_user_id, RBAC_USER, "*", RBAC_READ)) {
    jwt_free(jwt);
    return create_http_response(HTTP_FORBIDDEN, 
                 "{\"error\":\"Permission denied\"}", "application/json");
  }
  
  jwt_free(jwt);
  
  /* Get user */
  rbac_user_t* user = rbac_get_user(ctx->rbac, target_user_id);
  if (!user) {
    return create_http_response(HTTP_NOT_FOUND, 
                 "{\"error\":\"User not found\"}", "application/json");
  }
  
  /* Create response */
  json_value_t* response = json_create_object();
  json_value_t* user_obj = json_create_object();
  
  json_object_set(user_obj, "id", json_create_string(user->id));
  json_object_set(user_obj, "username", json_create_string(user->username));
  
  /* Add roles array */
  json_value_t* roles_array = json_create_array();
  for (size_t i = 0; i < user->roles->value.array.size; i++) {
    json_value_t* role_id = user->roles->value.array.items[i];
    if (role_id->type == JSON_STRING) {
      json_array_append(roles_array, json_create_string(role_id->value.string));
    }
  }
  json_object_set(user_obj, "roles", roles_array);
  
  /* Add information about the role objects if the user has appropriate permission */
  if (rbac_check_permission(ctx->rbac, requester_user_id, RBAC_ROLE, "*", RBAC_READ)) {
    json_value_t* roles_info = json_create_array();
    
    for (size_t i = 0; i < user->roles->value.array.size; i++) {
      json_value_t* role_id_val = user->roles->value.array.items[i];
      if (role_id_val->type == JSON_STRING) {
        const char* role_id = role_id_val->value.string;
        
        /* Get role */
        rbac_role_t* role = rbac_get_role(ctx->rbac, role_id);
        if (role) {
          /* Create role info object */
          json_value_t* role_info = json_create_object();
          json_object_set(role_info, "id", json_create_string(role->id));
          json_object_set(role_info, "name", json_create_string(role->name));
          
          /* Add role info to array */
          json_array_append(roles_info, role_info);
          
          /* Free role */
          rbac_free_role(role);
        }
      }
    }
    
    json_object_set(user_obj, "role_details", roles_info);
  }
  
  json_object_set(response, "user", user_obj);
  
  /* Serialize response */
  char* response_str = json_stringify(response);
  
  /* Free resources */
  /* CHECKPOINT: json_free(response); */
  rbac_free_user(user);
  
  /* Create and return response */
  return create_http_response(HTTP_OK, response_str, "application/json");
}

http_response_t* api_handle_user_create(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request || !request->body) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  if (!ctx->rbac) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"RBAC system not initialized\"}", "application/json");
  }
  
  /* Check if the user has admin privileges */
  /* Extract token and get user ID */
  char* token = api_extract_token(request);
  if (!token) {
    /* This should not happen since authorization is already checked in api_dispatch_request */
    return create_http_response(HTTP_UNAUTHORIZED, 
                 "{\"error\":\"Unauthorized\"}", "application/json");
  }
  
  /* Decode token */
  jwt_token_t* jwt = jwt_decode(token);
  BUFFER_FREE(token);
  
  if (!jwt || !jwt->payload || !jwt->payload->sub) {
    if (jwt) jwt_free(jwt);
    return create_http_response(HTTP_UNAUTHORIZED, 
                 "{\"error\":\"Invalid token\"}", "application/json");
  }
  
  const char* user_id = jwt->payload->sub;
  
  /* Check if user has permission to create users */
  if (!rbac_check_permission(ctx->rbac, user_id, RBAC_USER, "*", RBAC_WRITE)) {
    jwt_free(jwt);
    return create_http_response(HTTP_FORBIDDEN, 
                 "{\"error\":\"Permission denied\"}", "application/json");
  }
  
  jwt_free(jwt);
  
  /* Parse request body */
  json_value_t* body = json_parse(request->body);
  if (!body || body->type != JSON_OBJECT) {
    if (body) {

        /* CHECKPOINT: json_free(body); */

    }
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request body\"}", "application/json");
  }
  
  /* Extract username and password */
  json_value_t* username_val = json_object_get(body, "username");
  json_value_t* password_val = json_object_get(body, "password");
  json_value_t* roles_val = json_object_get(body, "roles");
  
  if (!username_val || username_val->type != JSON_STRING || 
    !password_val || password_val->type != JSON_STRING) {
    /* CHECKPOINT: json_free(body); */
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Username and password are required\"}", "application/json");
  }
  
  const char* username = username_val->value.string;
  const char* password = password_val->value.string;
  
  /* Validate input */
  if (strlen(username) < 3) {
    /* CHECKPOINT: json_free(body); */
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Username must be at least 3 characters long\"}", "application/json");
  }
  
  if (strlen(password) < 8) {
    /* CHECKPOINT: json_free(body); */
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Password must be at least 8 characters long\"}", "application/json");
  }
  
  /* Check if username already exists */
  if (rbac_get_user_by_username(ctx->rbac, username)) {
    /* CHECKPOINT: json_free(body); */
    return create_http_response(HTTP_CONFLICT, 
                 "{\"error\":\"Username already exists\"}", "application/json");
  }
  
  /* Create user */
  rbac_user_t* user = rbac_create_user(ctx->rbac, username, password);
  if (!user) {
    /* CHECKPOINT: json_free(body); */
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to create user\"}", "application/json");
  }
  
  /* Handle roles if provided */
  if (roles_val && roles_val->type == JSON_ARRAY) {
    for (size_t i = 0; i < roles_val->value.array.size; i++) {
      json_value_t* role_id_val = roles_val->value.array.items[i];
      if (role_id_val && role_id_val->type == JSON_STRING) {
        const char* role_id = role_id_val->value.string;
        
        /* Add user to role */
        rbac_add_user_to_role(ctx->rbac, user->id, role_id);
      }
    }
  }
  
  /* Create response */
  json_value_t* response = json_create_object();
  json_value_t* user_obj = json_create_object();
  
  json_object_set(user_obj, "id", json_create_string(user->id));
  json_object_set(user_obj, "username", json_create_string(user->username));
  
  /* Add roles array */
  json_value_t* roles_array = json_create_array();
  for (size_t i = 0; i < user->roles->value.array.size; i++) {
    json_value_t* role_id = user->roles->value.array.items[i];
    if (role_id && role_id->type == JSON_STRING) {
      json_array_append(roles_array, json_create_string(role_id->value.string));
    }
  }
  json_object_set(user_obj, "roles", roles_array);
  
  json_object_set(response, "user", user_obj);
  
  /* Serialize response */
  char* response_str = json_stringify(response);
  
  /* Free resources */
  /* CHECKPOINT: json_free(response); */
  /* CHECKPOINT: json_free(body); */
  rbac_free_user(user);
  
  /* Create and return response */
  return create_http_response(HTTP_CREATED, response_str, "application/json");
}

http_response_t* api_handle_user_update(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request || !request->body) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  if (!ctx->rbac) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"RBAC system not initialized\"}", "application/json");
  }
  
  /* Extract user ID from path */
  const char* path = request->path;
  if (strncmp(path, "/api/users/", 11) != 0) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid path\"}", "application/json");
  }
  
  const char* target_user_id = path + 11;
  
  /* Check if the user has appropriate permissions */
  /* Extract token and get user ID */
  char* token = api_extract_token(request);
  if (!token) {
    /* This should not happen since authorization is already checked in api_dispatch_request */
    return create_http_response(HTTP_UNAUTHORIZED, 
                 "{\"error\":\"Unauthorized\"}", "application/json");
  }
  
  /* Decode token */
  jwt_token_t* jwt = jwt_decode(token);
  BUFFER_FREE(token);
  
  if (!jwt || !jwt->payload || !jwt->payload->sub) {
    if (jwt) jwt_free(jwt);
    return create_http_response(HTTP_UNAUTHORIZED, 
                 "{\"error\":\"Invalid token\"}", "application/json");
  }
  
  const char* requester_user_id = jwt->payload->sub;
  
  /* Check if user has permission to update the target user 
   * The permission check is more permissive if the user is updating their own profile,
   * but updating roles requires admin privileges regardless.
   */
  int is_self_update = (strcmp(requester_user_id, target_user_id) == 0);
  int has_user_write = rbac_check_permission(ctx->rbac, requester_user_id, RBAC_USER, target_user_id, RBAC_WRITE) ||
             rbac_check_permission(ctx->rbac, requester_user_id, RBAC_USER, "*", RBAC_WRITE);
  
  if (!is_self_update && !has_user_write) {
    jwt_free(jwt);
    return create_http_response(HTTP_FORBIDDEN, 
                 "{\"error\":\"Permission denied\"}", "application/json");
  }
  
  /* Parse request body */
  json_value_t* body = json_parse(request->body);
  if (!body || body->type != JSON_OBJECT) {
    if (body) {

        /* CHECKPOINT: json_free(body); */

    }
    jwt_free(jwt);
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request body\"}", "application/json");
  }
  
  /* Check if user exists */
  rbac_user_t* user = rbac_get_user(ctx->rbac, target_user_id);
  if (!user) {
    /* CHECKPOINT: json_free(body); */
    jwt_free(jwt);
    return create_http_response(HTTP_NOT_FOUND, 
                 "{\"error\":\"User not found\"}", "application/json");
  }
  
  /* Extract fields to update */
  json_value_t* username_val = json_object_get(body, "username");
  json_value_t* password_val = json_object_get(body, "password");
  json_value_t* roles_val = json_object_get(body, "roles");
  
  /* Get the actual JSON user object using virtual layer */
  json_value_t* user_query = json_create_object();
  json_object_set(user_query, "type", json_create_string(DOC_TYPE_NAME_USER));
  json_object_set(user_query, "uuid", json_create_string(target_user_id));
  json_object_set(user_query, "library", json_create_string("system"));
  
  json_value_t* user_result = virtual_query(ctx->db, DOC_TYPE_NAME_USER, "system", VIRTUAL_COLLECTION_USERS, user_query);
  /* CHECKPOINT: json_free(user_query); */
  
  if (!user_result) {
    rbac_free_user(user);
    /* CHECKPOINT: json_free(body); */
    jwt_free(jwt);
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to query user from database\"}", "application/json");
  }
  
  json_value_t* user_docs = json_object_get(user_result, "documents");
  if (!user_docs || user_docs->type != JSON_ARRAY || user_docs->value.array.size == 0) {
    rbac_free_user(user);
    /* CHECKPOINT: json_free(body); */
    jwt_free(jwt);
    /* CHECKPOINT: json_free(user_result); */
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to find user in database\"}", "application/json");
  }
  
  json_value_t* user_obj = json_array_get(user_docs, 0);
  if (!user_obj || user_obj->type != JSON_OBJECT) {
    rbac_free_user(user);
    /* CHECKPOINT: json_free(body); */
    jwt_free(jwt);
    /* CHECKPOINT: json_free(user_result); */
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Invalid user document\"}", "application/json");
  }
  
  /* Update username if provided */
  if (username_val && username_val->type == JSON_STRING) {
    const char* new_username = username_val->value.string;
    
    /* Validate username */
    if (strlen(new_username) < 3) {
      rbac_free_user(user);
      /* CHECKPOINT: json_free(body); */
      jwt_free(jwt);
      return create_http_response(HTTP_BAD_REQUEST, 
                   "{\"error\":\"Username must be at least 3 characters long\"}", "application/json");
    }
    
    /* Check if username is already taken by another user */
    rbac_user_t* existing_user = rbac_get_user_by_username(ctx->rbac, new_username);
    if (existing_user && strcmp(existing_user->id, target_user_id) != 0) {
      rbac_free_user(user);
      rbac_free_user(existing_user);
      /* CHECKPOINT: json_free(body); */
      jwt_free(jwt);
      return create_http_response(HTTP_CONFLICT, 
                   "{\"error\":\"Username already exists\"}", "application/json");
    }
    
    if (existing_user) {
      rbac_free_user(existing_user);
    }
    
    /* Update username in the JSON object */
    json_object_set(user_obj, "username", json_create_string(new_username));
  }
  
  /* Update password if provided */
  if (password_val && password_val->type == JSON_STRING) {
    const char* new_password = password_val->value.string;
    
    /* Validate password */
    if (strlen(new_password) < 8) {
      rbac_free_user(user);
      /* CHECKPOINT: json_free(body); */
      jwt_free(jwt);
      return create_http_response(HTTP_BAD_REQUEST, 
                   "{\"error\":\"Password must be at least 8 characters long\"}", "application/json");
    }
    
    /* Hash password */
    char* password_hash = hash_password(new_password);
    
    if (!password_hash) {
      rbac_free_user(user);
      /* CHECKPOINT: json_free(body); */
      jwt_free(jwt);
      return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                   "{\"error\":\"Failed to hash password\"}", "application/json");
    }
    
    /* Update password_hash in the JSON object */
    json_object_set(user_obj, "password_hash", json_create_string(password_hash));
    
    /* Free password hash */
    BUFFER_FREE(password_hash);
  }
  
  /* Update roles if provided - requires admin permissions */
  if (roles_val && roles_val->type == JSON_ARRAY) {
    /* Check if user has admin privileges for role management */
    if (!rbac_check_permission(ctx->rbac, requester_user_id, RBAC_ROLE, "*", RBAC_ADMIN) &&
      !rbac_check_permission(ctx->rbac, requester_user_id, RBAC_USER, "*", RBAC_ADMIN)) {
      rbac_free_user(user);
      /* CHECKPOINT: json_free(body); */
      jwt_free(jwt);
      return create_http_response(HTTP_FORBIDDEN, 
                   "{\"error\":\"Permission denied for role management\"}", "application/json");
    }
    
    /* Get the current roles array from the user object */
    json_value_t* current_roles = json_object_get(user_obj, "roles");
    if (!current_roles || current_roles->type != JSON_ARRAY) {
      /* Create roles array if it doesn't exist */
      current_roles = json_create_array();
      json_object_set(user_obj, "roles", current_roles);
    }
    
    /* First, remove this user from all existing roles */
    for (size_t i = 0; i < current_roles->value.array.size; i++) {
      json_value_t* role_id_val = current_roles->value.array.items[i];
      if (role_id_val && role_id_val->type == JSON_STRING) {
        const char* role_id = role_id_val->value.string;
        
        /* Get role using virtual layer */
        json_value_t* role_query = json_create_object();
        json_object_set(role_query, "type", json_create_string(DOC_TYPE_NAME_ROLE));
        json_object_set(role_query, "uuid", json_create_string(role_id));
        json_object_set(role_query, "library", json_create_string("system"));
        
        json_value_t* role_result = virtual_query(ctx->db, DOC_TYPE_NAME_ROLE, "system", VIRTUAL_COLLECTION_ROLES, role_query);
        /* CHECKPOINT: json_free(role_query); */
        
        if (role_result) {
          json_value_t* role_docs = json_object_get(role_result, "documents");
          if (role_docs && role_docs->type == JSON_ARRAY && role_docs->value.array.size > 0) {
            json_value_t* role_obj = json_array_get(role_docs, 0);
            if (role_obj && role_obj->type == JSON_OBJECT) {
              /* Get users array */
              json_value_t* users = json_object_get(role_obj, "users");
              if (users && users->type == JSON_ARRAY) {
                /* Remove user from role */
                for (size_t j = 0; j < users->value.array.size; j++) {
                  json_value_t* user_id_val = users->value.array.items[j];
                  if (user_id_val && user_id_val->type == JSON_STRING && 
                    strcmp(user_id_val->value.string, target_user_id) == 0) {
                    /* Remove user from array */
                    for (size_t k = j; k < users->value.array.size - 1; k++) {
                      users->value.array.items[k] = users->value.array.items[k + 1];
                    }
                    users->value.array.size--;
                    break;
                  }
                }
                /* Update role document using virtual layer */
                const char* role_uuid = json_object_get_string(role_obj, "uuid");
                if (role_uuid) {
                  virtual_update(ctx->db, role_uuid, role_obj);
                }
              }
            }
          }
          /* CHECKPOINT: json_free(role_result); */
        }
      }
    }
    
    /* Clear current roles array */
    current_roles->value.array.size = 0;
    
    /* Add user to new roles and update roles array */
    for (size_t i = 0; i < roles_val->value.array.size; i++) {
      json_value_t* role_id_val = roles_val->value.array.items[i];
      if (role_id_val && role_id_val->type == JSON_STRING) {
        const char* role_id = role_id_val->value.string;
        
        /* Verify that role exists using virtual layer */
        json_value_t* role_query = json_create_object();
        json_object_set(role_query, "type", json_create_string(DOC_TYPE_NAME_ROLE));
        json_object_set(role_query, "uuid", json_create_string(role_id));
        json_object_set(role_query, "library", json_create_string("system"));
        
        json_value_t* role_result = virtual_query(ctx->db, DOC_TYPE_NAME_ROLE, "system", VIRTUAL_COLLECTION_ROLES, role_query);
        /* CHECKPOINT: json_free(role_query); */
        
        if (role_result) {
          json_value_t* role_docs = json_object_get(role_result, "documents");
          if (role_docs && role_docs->type == JSON_ARRAY && role_docs->value.array.size > 0) {
            json_value_t* role_obj = json_array_get(role_docs, 0);
            if (role_obj && role_obj->type == JSON_OBJECT) {
              /* Add role ID to user's roles */
              json_array_append(current_roles, json_create_string(role_id));
              
              /* Add user to role's users */
              json_value_t* users = json_object_get(role_obj, "users");
              if (!users || users->type != JSON_ARRAY) {
                /* Create users array if it doesn't exist */
                users = json_create_array();
                json_object_set(role_obj, "users", users);
              }
              
              /* Check if user is already in role */
              int user_in_role = 0;
              for (size_t j = 0; j < users->value.array.size; j++) {
                json_value_t* user_id_val = users->value.array.items[j];
                if (user_id_val && user_id_val->type == JSON_STRING && 
                  strcmp(user_id_val->value.string, target_user_id) == 0) {
                  user_in_role = 1;
                  break;
                }
              }
              
              /* Add user to role if not already present */
              if (!user_in_role) {
                json_array_append(users, json_create_string(target_user_id));
              }
              
              /* Update role document using virtual layer */
              const char* role_uuid = json_object_get_string(role_obj, "uuid");
              if (role_uuid) {
                virtual_update(ctx->db, role_uuid, role_obj);
              }
            }
          }
          /* CHECKPOINT: json_free(role_result); */
        }
      }
    }
  }
  
  /* Get updated user for response */
  rbac_free_user(user);
  user = rbac_get_user(ctx->rbac, target_user_id);
  
  /* Create response */
  json_value_t* response = json_create_object();
  json_value_t* updated_user = json_create_object();
  
  json_object_set(updated_user, "id", json_create_string(user->id));
  json_object_set(updated_user, "username", json_create_string(user->username));
  
  /* Add roles array */
  json_value_t* roles_array = json_create_array();
  for (size_t i = 0; i < user->roles->value.array.size; i++) {
    json_value_t* role_id = user->roles->value.array.items[i];
    if (role_id->type == JSON_STRING) {
      json_array_append(roles_array, json_create_string(role_id->value.string));
    }
  }
  json_object_set(updated_user, "roles", roles_array);
  
  json_object_set(response, "user", updated_user);
  
  /* Serialize response */
  char* response_str = json_stringify(response);
  
  /* Update the user document using virtual layer */
  const char* user_uuid = json_object_get_string(user_obj, "uuid");
  if (user_uuid) {
    virtual_update(ctx->db, user_uuid, user_obj);
  }
  
  /* Free resources */
  rbac_free_user(user);
  /* CHECKPOINT: json_free(response); */
  /* CHECKPOINT: json_free(body); */
  jwt_free(jwt);
  /* CHECKPOINT: json_free(user_result); */
  
  /* Create and return response */
  return create_http_response(HTTP_OK, response_str, "application/json");
}

http_response_t* api_handle_user_delete(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  if (!ctx->rbac) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"RBAC system not initialized\"}", "application/json");
  }
  
  /* Extract user ID from path */
  const char* path = request->path;
  if (strncmp(path, "/api/users/", 11) != 0) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid path\"}", "application/json");
  }
  
  const char* target_user_id = path + 11;
  
  /* Check if the user has appropriate permissions */
  /* Extract token and get user ID */
  char* token = api_extract_token(request);
  if (!token) {
    /* This should not happen since authorization is already checked in api_dispatch_request */
    return create_http_response(HTTP_UNAUTHORIZED, 
                 "{\"error\":\"Unauthorized\"}", "application/json");
  }
  
  /* Decode token */
  jwt_token_t* jwt = jwt_decode(token);
  BUFFER_FREE(token);
  
  if (!jwt || !jwt->payload || !jwt->payload->sub) {
    if (jwt) jwt_free(jwt);
    return create_http_response(HTTP_UNAUTHORIZED, 
                 "{\"error\":\"Invalid token\"}", "application/json");
  }
  
  const char* requester_user_id = jwt->payload->sub;
  
  /* Users can't delete themselves, only admins can delete users */
  if (strcmp(requester_user_id, target_user_id) == 0) {
    jwt_free(jwt);
    return create_http_response(HTTP_FORBIDDEN, 
                 "{\"error\":\"You cannot delete your own account\"}", "application/json");
  }
  
  /* Check if user has permission to delete the target user */
  if (!rbac_check_permission(ctx->rbac, requester_user_id, RBAC_USER, target_user_id, RBAC_DELETE) &&
    !rbac_check_permission(ctx->rbac, requester_user_id, RBAC_USER, "*", RBAC_DELETE)) {
    jwt_free(jwt);
    return create_http_response(HTTP_FORBIDDEN, 
                 "{\"error\":\"Permission denied\"}", "application/json");
  }
  
  jwt_free(jwt);
  
  /* Check if user exists using virtual layer */
  json_value_t* user_query = json_create_object();
  json_object_set(user_query, "type", json_create_string(DOC_TYPE_NAME_USER));
  json_object_set(user_query, "uuid", json_create_string(target_user_id));
  json_object_set(user_query, "library", json_create_string("system"));
  
  json_value_t* user_result = virtual_query(ctx->db, DOC_TYPE_NAME_USER, "system", VIRTUAL_COLLECTION_USERS, user_query);
  /* CHECKPOINT: json_free(user_query); */
  
  if (!user_result) {
    return create_http_response(HTTP_NOT_FOUND, 
                 "{\"error\":\"User not found\"}", "application/json");
  }
  
  json_value_t* user_docs = json_object_get(user_result, "documents");
  if (!user_docs || user_docs->type != JSON_ARRAY || user_docs->value.array.size == 0) {
    /* CHECKPOINT: json_free(user_result); */
    return create_http_response(HTTP_NOT_FOUND, 
                 "{\"error\":\"User not found\"}", "application/json");
  }
  /* CHECKPOINT: json_free(user_result); */
  
  /* Delete user */
  if (!rbac_delete_user(ctx->rbac, target_user_id)) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to delete user\"}", "application/json");
  }
  
  /* Return success with no content */
  return create_http_response(HTTP_NO_CONTENT, NULL, "application/json");
}

http_response_t* api_handle_roles_list(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  if (!ctx->rbac) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"RBAC system not initialized\"}", "application/json");
  }
  
  /* Check if the user has admin privileges */
  /* Extract token and get user ID */
  char* token = api_extract_token(request);
  if (!token) {
    /* This should not happen since authorization is already checked in api_dispatch_request */
    return create_http_response(HTTP_UNAUTHORIZED, 
                 "{\"error\":\"Unauthorized\"}", "application/json");
  }
  
  /* Decode token */
  jwt_token_t* jwt = jwt_decode(token);
  BUFFER_FREE(token);
  
  if (!jwt || !jwt->payload || !jwt->payload->sub) {
    if (jwt) jwt_free(jwt);
    return create_http_response(HTTP_UNAUTHORIZED, 
                 "{\"error\":\"Invalid token\"}", "application/json");
  }
  
  const char* user_id = jwt->payload->sub;
  
  /* Check if user has permission to list roles */
  if (!rbac_check_permission(ctx->rbac, user_id, RBAC_ROLE, "*", RBAC_READ)) {
    jwt_free(jwt);
    return create_http_response(HTTP_FORBIDDEN, 
                 "{\"error\":\"Permission denied\"}", "application/json");
  }
  
  jwt_free(jwt);
  
  /* SINGLE SOURCE OF TRUTH: Query roles using virtual layer */
  json_value_t* roles_query = json_create_object();
  json_object_set(roles_query, "type", json_create_string(DOC_TYPE_NAME_ROLE));
  json_object_set(roles_query, "library", json_create_string("system"));
  
  json_value_t* roles_results = virtual_query(ctx->db, DOC_TYPE_NAME_ROLE, "system", VIRTUAL_COLLECTION_ROLES, roles_query);
  /* CHECKPOINT: json_free(roles_query); */
  
  if (!roles_results) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to query roles\"}", "application/json");
  }
  
  json_value_t* roles_docs = json_object_get(roles_results, "documents");
  if (!roles_docs || roles_docs->type != JSON_ARRAY) {
    /* CHECKPOINT: json_free(roles_results); */
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Invalid roles response\"}", "application/json");
  }
  
  /* Create a JSON array of roles */
  json_value_t* roles_array = json_create_array();
  if (!roles_array) {
    /* CHECKPOINT: json_free(roles_results); */
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to create roles array\"}", "application/json");
  }
  
  /* Process each role document */
  for (size_t i = 0; i < json_array_size(roles_docs); i++) {
    json_value_t* role_doc = json_array_get(roles_docs, i);
    if (role_doc && role_doc->type == JSON_OBJECT) {
      /* Create a new role object with relevant information */
      json_value_t* role = json_create_object();
      
      /* Add role ID from uuid field */
      json_value_t* uuid = json_object_get(role_doc, "uuid");
      if (uuid && uuid->type == JSON_STRING) {
        json_object_set(role, "id", json_create_string(uuid->value.string));
      }
      
      /* Add name if present */
      json_value_t* name = json_object_get(role_doc, "name");
      if (name && name->type == JSON_STRING) {
        json_object_set(role, "name", json_create_string(name->value.string));
      }
      
      /* Add users array if present */
      json_value_t* users = json_object_get(role_doc, "users");
      if (users && users->type == JSON_ARRAY) {
        /* Create a deep copy of the users array */
        json_value_t* users_copy = json_create_array();
        for (size_t j = 0; j < users->value.array.size; j++) {
          json_value_t* user_id = users->value.array.items[j];
          if (user_id && user_id->type == JSON_STRING) {
            json_array_append(users_copy, json_create_string(user_id->value.string));
          }
        }
        json_object_set(role, "users", users_copy);
      }
      
      /* Add permissions object if present */
      json_value_t* permissions = json_object_get(role_doc, "permissions");
      if (permissions && permissions->type == JSON_OBJECT) {
        /* Create a deep copy of the permissions object */
        json_value_t* permissions_copy = json_create_object();
        for (size_t j = 0; j < permissions->value.object.size; j++) {
          const char* perm_key = permissions->value.object.entries[j].key;
          json_value_t* perm_val = permissions->value.object.entries[j].value;
          
          if (perm_val && (perm_val->type == JSON_NUMBER || perm_val->type == JSON_INTEGER)) {
            json_object_set(permissions_copy, perm_key, 
                    json_create_number(perm_val->type == JSON_NUMBER ? 
                             perm_val->value.number : perm_val->value.integer));
          }
        }
        json_object_set(role, "permissions", permissions_copy);
      }
      
      /* Add role to array */
      json_array_append(roles_array, role);
    }
  }
  
  /* CHECKPOINT: json_free(roles_results); */
  
  /* Create response object */
  json_value_t* response = json_create_object();
  json_object_set(response, "roles", roles_array);
  
  /* Serialize response */
  char* response_str = json_stringify(response);
  
  /* Free resources */
  /* CHECKPOINT: json_free(response); */
  
  /* Create and return response */
  return create_http_response(HTTP_OK, response_str, "application/json");
}

http_response_t* api_handle_role_get(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  if (!ctx->rbac) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"RBAC system not initialized\"}", "application/json");
  }
  
  /* Extract role ID from path */
  const char* path = request->path;
  if (strncmp(path, "/api/roles/", 11) != 0) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid path\"}", "application/json");
  }
  
  const char* role_id = path + 11;
  
  /* Check if the user has appropriate permissions */
  /* Extract token and get user ID */
  char* token = api_extract_token(request);
  if (!token) {
    /* This should not happen since authorization is already checked in api_dispatch_request */
    return create_http_response(HTTP_UNAUTHORIZED, 
                 "{\"error\":\"Unauthorized\"}", "application/json");
  }
  
  /* Decode token */
  jwt_token_t* jwt = jwt_decode(token);
  BUFFER_FREE(token);
  
  if (!jwt || !jwt->payload || !jwt->payload->sub) {
    if (jwt) jwt_free(jwt);
    return create_http_response(HTTP_UNAUTHORIZED, 
                 "{\"error\":\"Invalid token\"}", "application/json");
  }
  
  const char* user_id = jwt->payload->sub;
  
  /* Check if user has permission to view role */
  if (!rbac_check_permission(ctx->rbac, user_id, RBAC_ROLE, role_id, RBAC_READ) &&
    !rbac_check_permission(ctx->rbac, user_id, RBAC_ROLE, "*", RBAC_READ)) {
    jwt_free(jwt);
    return create_http_response(HTTP_FORBIDDEN, 
                 "{\"error\":\"Permission denied\"}", "application/json");
  }
  
  jwt_free(jwt);
  
  /* Get role */
  rbac_role_t* role = rbac_get_role(ctx->rbac, role_id);
  if (!role) {
    return create_http_response(HTTP_NOT_FOUND, 
                 "{\"error\":\"Role not found\"}", "application/json");
  }
  
  /* Create response */
  json_value_t* response = json_create_object();
  json_value_t* role_obj = json_create_object();
  
  json_object_set(role_obj, "id", json_create_string(role->id));
  json_object_set(role_obj, "name", json_create_string(role->name));
  
  /* Add permissions object */
  json_value_t* permissions_obj = json_create_object();
  
  /* Copy permissions from role */
  if (role->permissions) {
    for (size_t i = 0; i < role->permissions->value.object.size; i++) {
      const char* perm_key = role->permissions->value.object.entries[i].key;
      json_value_t* perm_val = role->permissions->value.object.entries[i].value;
      
      if (perm_val && (perm_val->type == JSON_NUMBER || perm_val->type == JSON_INTEGER)) {
        json_object_set(permissions_obj, perm_key, 
                json_create_number(perm_val->type == JSON_NUMBER ? 
                         perm_val->value.number : perm_val->value.integer));
      }
    }
  }
  
  json_object_set(role_obj, "permissions", permissions_obj);
  
  /* Add users array */
  json_value_t* users_array = json_create_array();
  
  /* Get role users using virtual layer */
  json_value_t* role_query = json_create_object();
  json_object_set(role_query, "type", json_create_string(DOC_TYPE_NAME_ROLE));
  json_object_set(role_query, "uuid", json_create_string(role_id));
  json_object_set(role_query, "library", json_create_string("system"));
  
  json_value_t* role_result = virtual_query(ctx->db, DOC_TYPE_NAME_ROLE, "system", VIRTUAL_COLLECTION_ROLES, role_query);
  /* CHECKPOINT: json_free(role_query); */
  
  if (role_result) {
    json_value_t* role_docs = json_object_get(role_result, "documents");
    if (role_docs && role_docs->type == JSON_ARRAY && role_docs->value.array.size > 0) {
      json_value_t* role_json = json_array_get(role_docs, 0);
      if (role_json && role_json->type == JSON_OBJECT) {
        json_value_t* users = json_object_get(role_json, "users");
        if (users && users->type == JSON_ARRAY) {
          for (size_t i = 0; i < users->value.array.size; i++) {
            json_value_t* user_id_val = users->value.array.items[i];
            if (user_id_val && user_id_val->type == JSON_STRING) {
              /* Add user ID to array */
              json_array_append(users_array, json_create_string(user_id_val->value.string));
              
              /* Optionally, add user details from database */
              const char* user_id_str = user_id_val->value.string;
              json_value_t* user_query = json_create_object();
              json_object_set(user_query, "type", json_create_string(DOC_TYPE_NAME_USER));
              json_object_set(user_query, "uuid", json_create_string(user_id_str));
              json_object_set(user_query, "library", json_create_string("system"));
              
              json_value_t* user_result = virtual_query(ctx->db, DOC_TYPE_NAME_USER, "system", VIRTUAL_COLLECTION_USERS, user_query);
              /* CHECKPOINT: json_free(user_query); */
              
              if (user_result) {
                json_value_t* user_docs = json_object_get(user_result, "documents");
                if (user_docs && user_docs->type == JSON_ARRAY && user_docs->value.array.size > 0) {
                  json_value_t* user_json = json_array_get(user_docs, 0);
                  if (user_json && user_json->type == JSON_OBJECT) {
                    json_value_t* username_val = json_object_get(user_json, "username");
                    if (username_val && username_val->type == JSON_STRING) {
                      /* Create user info object */
                      json_value_t* user_info = json_create_object();
                      json_object_set(user_info, "id", json_create_string(user_id_str));
                      json_object_set(user_info, "username", json_create_string(username_val->value.string));
                      
                      /* Add user info to array (replacing the simple ID string) */
                      users_array->value.array.items[users_array->value.array.size - 1] = user_info;
                    }
                  }
                }
                /* CHECKPOINT: json_free(user_result); */
              }
            }
          }
        }
      }
    }
    /* CHECKPOINT: json_free(role_result); */
  }
  
  json_object_set(role_obj, "users", users_array);
  
  json_object_set(response, "role", role_obj);
  
  /* Serialize response */
  char* response_str = json_stringify(response);
  
  /* Free resources */
  /* CHECKPOINT: json_free(response); */
  rbac_free_role(role);
  
  /* Create and return response */
  return create_http_response(HTTP_OK, response_str, "application/json");
}

http_response_t* api_handle_role_create(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request || !request->body) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  if (!ctx->rbac) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"RBAC system not initialized\"}", "application/json");
  }
  
  /* Check if the user has admin privileges */
  /* Extract token and get user ID */
  char* token = api_extract_token(request);
  if (!token) {
    /* This should not happen since authorization is already checked in api_dispatch_request */
    return create_http_response(HTTP_UNAUTHORIZED, 
                 "{\"error\":\"Unauthorized\"}", "application/json");
  }
  
  /* Decode token */
  jwt_token_t* jwt = jwt_decode(token);
  BUFFER_FREE(token);
  
  if (!jwt || !jwt->payload || !jwt->payload->sub) {
    if (jwt) jwt_free(jwt);
    return create_http_response(HTTP_UNAUTHORIZED, 
                 "{\"error\":\"Invalid token\"}", "application/json");
  }
  
  const char* user_id = jwt->payload->sub;
  
  /* Check if user has permission to create roles */
  if (!rbac_check_permission(ctx->rbac, user_id, RBAC_ROLE, "*", RBAC_WRITE)) {
    jwt_free(jwt);
    return create_http_response(HTTP_FORBIDDEN, 
                 "{\"error\":\"Permission denied\"}", "application/json");
  }
  
  jwt_free(jwt);
  
  /* Parse request body */
  json_value_t* body = json_parse(request->body);
  if (!body || body->type != JSON_OBJECT) {
    if (body) {

        /* CHECKPOINT: json_free(body); */

    }
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request body\"}", "application/json");
  }
  
  /* Extract role name and permissions */
  json_value_t* name_val = json_object_get(body, "name");
  json_value_t* permissions_val = json_object_get(body, "permissions");
  
  if (!name_val || name_val->type != JSON_STRING) {
    /* CHECKPOINT: json_free(body); */
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Role name is required\"}", "application/json");
  }
  
  const char* name = name_val->value.string;
  
  /* Validate input */
  if (strlen(name) < 2) {
    /* CHECKPOINT: json_free(body); */
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Role name must be at least 2 characters long\"}", "application/json");
  }
  
  /* Check if role name already exists in database */
  json_value_t* role_query = json_create_object();
  json_object_set(role_query, "type", json_create_string(DOC_TYPE_NAME_ROLE));
  json_object_set(role_query, "library", json_create_string("system"));
  json_object_set(role_query, "name", json_create_string(name));
  
  json_value_t* role_result = virtual_query(ctx->db, DOC_TYPE_NAME_ROLE, "system", VIRTUAL_COLLECTION_ROLES, role_query);
  /* CHECKPOINT: json_free(role_query); */
  
  if (role_result) {
    json_value_t* role_docs = json_object_get(role_result, "documents");
    if (role_docs && role_docs->type == JSON_ARRAY && role_docs->value.array.size > 0) {
      /* CHECKPOINT: json_free(role_result); */
      /* CHECKPOINT: json_free(body); */
      return create_http_response(HTTP_CONFLICT, 
                   "{\"error\":\"Role name already exists\"}", "application/json");
    }
    /* CHECKPOINT: json_free(role_result); */
  }
  
  /* Create role */
  rbac_role_t* role = rbac_create_role(ctx->rbac, name);
  if (!role) {
    /* CHECKPOINT: json_free(body); */
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to create role\"}", "application/json");
  }
  
  /* Process permissions if provided */
  if (permissions_val && permissions_val->type == JSON_OBJECT) {
    for (size_t i = 0; i < permissions_val->value.object.size; i++) {
      const char* resource_str = permissions_val->value.object.entries[i].key;
      json_value_t* perms_val = permissions_val->value.object.entries[i].value;
      
      /* Parse resource string into type and ID */
      char* colon = strchr(resource_str, ':');
      if (colon) {
        /* Split "type:id" string */
        int type_len = colon - resource_str;
        char type_str[32] = {0};
        strncpy(type_str, resource_str, type_len < 31 ? type_len : 31);
        
        /* Convert type string to enum */
        rbac_resource_type_t resource_type = RBAC_UNKNOWN;
        if (strcmp(type_str, "database") == 0) {
          resource_type = RBAC_DATABASE;
        } else if (strcmp(type_str, "collection") == 0) {
          resource_type = RBAC_COLLECTION;
        } else if (strcmp(type_str, "document") == 0) {
          resource_type = RBAC_DOCUMENT;
        } else if (strcmp(type_str, "user") == 0) {
          resource_type = RBAC_USER;
        } else if (strcmp(type_str, "role") == 0) {
          resource_type = RBAC_ROLE;
        }
        
        const char* resource_id = colon + 1;
        
        /* Get permission value */
        int permission = 0;
        if (perms_val->type == JSON_NUMBER) {
          permission = (int)perms_val->value.number;
        } else if (perms_val->type == JSON_INTEGER) {
          permission = (int)perms_val->value.integer;
        } else if (perms_val->type == JSON_OBJECT) {
          /* Process permission object with read, write, delete, admin flags */
          json_value_t* read_val = json_object_get(perms_val, "read");
          json_value_t* write_val = json_object_get(perms_val, "write");
          json_value_t* delete_val = json_object_get(perms_val, "delete");
          json_value_t* admin_val = json_object_get(perms_val, "admin");
          
          if (read_val && (read_val->type == JSON_BOOLEAN) && read_val->value.boolean) {
            permission |= RBAC_READ;
          }
          
          if (write_val && (write_val->type == JSON_BOOLEAN) && write_val->value.boolean) {
            permission |= RBAC_WRITE;
          }
          
          if (delete_val && (delete_val->type == JSON_BOOLEAN) && delete_val->value.boolean) {
            permission |= RBAC_DELETE;
          }
          
          if (admin_val && (admin_val->type == JSON_BOOLEAN) && admin_val->value.boolean) {
            permission |= RBAC_ADMIN;
          }
        }
        
        /* Grant permission */
        if (resource_type != RBAC_UNKNOWN && permission != 0) {
          rbac_grant_permission(ctx->rbac, role->id, resource_type, resource_id, permission);
        }
      }
    }
  }
  
  /* Create response */
  json_value_t* response = json_create_object();
  json_value_t* role_obj = json_create_object();
  
  json_object_set(role_obj, "id", json_create_string(role->id));
  json_object_set(role_obj, "name", json_create_string(role->name));
  
  /* Add permissions object */
  json_value_t* permissions_obj = json_create_object();
  
  /* Copy permissions from role */
  if (role->permissions) {
    for (size_t i = 0; i < role->permissions->value.object.size; i++) {
      const char* perm_key = role->permissions->value.object.entries[i].key;
      json_value_t* perm_val = role->permissions->value.object.entries[i].value;
      
      if (perm_val && (perm_val->type == JSON_NUMBER || perm_val->type == JSON_INTEGER)) {
        json_object_set(permissions_obj, perm_key, 
                json_create_number(perm_val->type == JSON_NUMBER ? 
                         perm_val->value.number : perm_val->value.integer));
      }
    }
  }
  
  json_object_set(role_obj, "permissions", permissions_obj);
  
  /* Add users array (empty for new role) */
  json_object_set(role_obj, "users", json_create_array());
  
  json_object_set(response, "role", role_obj);
  
  /* Serialize response */
  char* response_str = json_stringify(response);
  
  /* Free resources */
  /* CHECKPOINT: json_free(response); */
  /* CHECKPOINT: json_free(body); */
  rbac_free_role(role);
  
  /* Create and return response */
  return create_http_response(HTTP_CREATED, response_str, "application/json");
}

http_response_t* api_handle_role_update(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request || !request->body) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  if (!ctx->rbac) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"RBAC system not initialized\"}", "application/json");
  }
  
  /* Extract role ID from path */
  const char* path = request->path;
  if (strncmp(path, "/api/roles/", 11) != 0) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid path\"}", "application/json");
  }
  
  const char* role_id = path + 11;
  
  /* Check if the user has appropriate permissions */
  /* Extract token and get user ID */
  char* token = api_extract_token(request);
  if (!token) {
    /* This should not happen since authorization is already checked in api_dispatch_request */
    return create_http_response(HTTP_UNAUTHORIZED, 
                 "{\"error\":\"Unauthorized\"}", "application/json");
  }
  
  /* Decode token */
  jwt_token_t* jwt = jwt_decode(token);
  BUFFER_FREE(token);
  
  if (!jwt || !jwt->payload || !jwt->payload->sub) {
    if (jwt) jwt_free(jwt);
    return create_http_response(HTTP_UNAUTHORIZED, 
                 "{\"error\":\"Invalid token\"}", "application/json");
  }
  
  const char* user_id = jwt->payload->sub;
  
  /* Check if user has permission to update role */
  if (!rbac_check_permission(ctx->rbac, user_id, RBAC_ROLE, role_id, RBAC_WRITE) &&
    !rbac_check_permission(ctx->rbac, user_id, RBAC_ROLE, "*", RBAC_WRITE)) {
    jwt_free(jwt);
    return create_http_response(HTTP_FORBIDDEN, 
                 "{\"error\":\"Permission denied\"}", "application/json");
  }
  
  jwt_free(jwt);
  
  /* Check if role exists in database */
  json_value_t* role_query = json_create_object();
  json_object_set(role_query, "type", json_create_string(DOC_TYPE_NAME_ROLE));
  json_object_set(role_query, "uuid", json_create_string(role_id));
  json_object_set(role_query, "library", json_create_string("system"));
  
  json_value_t* role_result = virtual_query(ctx->db, DOC_TYPE_NAME_ROLE, "system", VIRTUAL_COLLECTION_ROLES, role_query);
  /* CHECKPOINT: json_free(role_query); */
  
  if (!role_result) {
    return create_http_response(HTTP_NOT_FOUND, 
                 "{\"error\":\"Role not found\"}", "application/json");
  }
  
  json_value_t* role_docs = json_object_get(role_result, "documents");
  if (!role_docs || role_docs->type != JSON_ARRAY || role_docs->value.array.size == 0) {
    /* CHECKPOINT: json_free(role_result); */
    return create_http_response(HTTP_NOT_FOUND, 
                 "{\"error\":\"Role not found\"}", "application/json");
  }
  
  json_value_t* role_json = json_array_get(role_docs, 0);
  if (!role_json || role_json->type != JSON_OBJECT) {
    /* CHECKPOINT: json_free(role_result); */
    return create_http_response(HTTP_NOT_FOUND, 
                 "{\"error\":\"Invalid role document\"}", "application/json");
  }
  
  /* Parse request body */
  json_value_t* body = json_parse(request->body);
  if (!body || body->type != JSON_OBJECT) {
    if (body) {

        /* CHECKPOINT: json_free(body); */

    }
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request body\"}", "application/json");
  }
  
  /* Extract fields to update */
  json_value_t* name_val = json_object_get(body, "name");
  json_value_t* permissions_val = json_object_get(body, "permissions");
  json_value_t* users_val = json_object_get(body, "users");
  
  /* Update name if provided */
  if (name_val && name_val->type == JSON_STRING) {
    const char* new_name = name_val->value.string;
    
    /* Validate name */
    if (strlen(new_name) < 2) {
      /* CHECKPOINT: json_free(body); */
      return create_http_response(HTTP_BAD_REQUEST, 
                   "{\"error\":\"Role name must be at least 2 characters long\"}", "application/json");
    }
    
    /* Check if name is already taken by another role using virtual layer */
    json_value_t* name_query = json_create_object();
    json_object_set(name_query, "type", json_create_string(DOC_TYPE_NAME_ROLE));
    json_object_set(name_query, "library", json_create_string("system"));
    json_object_set(name_query, "name", json_create_string(new_name));
    
    json_value_t* name_result = virtual_query(ctx->db, DOC_TYPE_NAME_ROLE, "system", VIRTUAL_COLLECTION_ROLES, name_query);
    /* CHECKPOINT: json_free(name_query); */
    
    if (name_result) {
      json_value_t* name_docs = json_object_get(name_result, "documents");
      if (name_docs && name_docs->type == JSON_ARRAY) {
        for (size_t i = 0; i < name_docs->value.array.size; i++) {
          json_value_t* existing_role = json_array_get(name_docs, i);
          if (existing_role && existing_role->type == JSON_OBJECT) {
            const char* existing_uuid = json_object_get_string(existing_role, "uuid");
            if (existing_uuid && strcmp(existing_uuid, role_id) != 0) {
              /* CHECKPOINT: json_free(name_result); */
              /* CHECKPOINT: json_free(body); */
              /* CHECKPOINT: json_free(role_result); */
              return create_http_response(HTTP_CONFLICT, 
                           "{\"error\":\"Role name already exists\"}", "application/json");
            }
          }
        }
      }
      /* CHECKPOINT: json_free(name_result); */
    }
    
    /* Update name */
    json_object_set(role_json, "name", json_create_string(new_name));
  }
  
  /* Update permissions if provided */
  if (permissions_val && permissions_val->type == JSON_OBJECT) {
    /* Get current permissions */
    json_value_t* current_permissions = json_object_get(role_json, "permissions");
    if (!current_permissions || current_permissions->type != JSON_OBJECT) {
      /* Create permissions object if it doesn't exist */
      current_permissions = json_create_object();
      json_object_set(role_json, "permissions", current_permissions);
    }
    
    /* Clear existing permissions */
    for (size_t i = 0; i < current_permissions->value.object.size; i++) {
      const char* key = current_permissions->value.object.entries[i].key;
      json_object_remove(current_permissions, key);
      /* Adjust index after removal */
      i--;
    }
    
    /* Add new permissions */
    for (size_t i = 0; i < permissions_val->value.object.size; i++) {
      const char* key = permissions_val->value.object.entries[i].key;
      json_value_t* value = permissions_val->value.object.entries[i].value;
      
      if (value && (value->type == JSON_NUMBER || value->type == JSON_INTEGER)) {
        json_object_set(current_permissions, key, 
                json_create_number(value->type == JSON_NUMBER ? 
                         value->value.number : value->value.integer));
      } else if (value && value->type == JSON_OBJECT) {
        /* Process permission object with read, write, delete, admin flags */
        json_value_t* read_val = json_object_get(value, "read");
        json_value_t* write_val = json_object_get(value, "write");
        json_value_t* delete_val = json_object_get(value, "delete");
        json_value_t* admin_val = json_object_get(value, "admin");
        
        int permission = 0;
        
        if (read_val && (read_val->type == JSON_BOOLEAN) && read_val->value.boolean) {
          permission |= RBAC_READ;
        }
        
        if (write_val && (write_val->type == JSON_BOOLEAN) && write_val->value.boolean) {
          permission |= RBAC_WRITE;
        }
        
        if (delete_val && (delete_val->type == JSON_BOOLEAN) && delete_val->value.boolean) {
          permission |= RBAC_DELETE;
        }
        
        if (admin_val && (admin_val->type == JSON_BOOLEAN) && admin_val->value.boolean) {
          permission |= RBAC_ADMIN;
        }
        
        if (permission != 0) {
          json_object_set(current_permissions, key, json_create_number(permission));
        }
      }
    }
  }
  
  /* Update users if provided */
  if (users_val && users_val->type == JSON_ARRAY) {
    /* Get current users */
    json_value_t* current_users = json_object_get(role_json, "users");
    if (!current_users || current_users->type != JSON_ARRAY) {
      /* Create users array if it doesn't exist */
      current_users = json_create_array();
      json_object_set(role_json, "users", current_users);
    }
    
    /* First, remove this role from all users' roles */
    for (size_t i = 0; i < current_users->value.array.size; i++) {
      json_value_t* user_id_val = current_users->value.array.items[i];
      if (user_id_val && user_id_val->type == JSON_STRING) {
        const char* user_id_str = user_id_val->value.string;
        
        /* Get user from database */
        json_value_t* user_query = json_create_object();
        json_object_set(user_query, "type", json_create_string(DOC_TYPE_NAME_USER));
        json_object_set(user_query, "uuid", json_create_string(user_id_str));
        json_object_set(user_query, "library", json_create_string("system"));
        
        json_value_t* user_result = virtual_query(ctx->db, DOC_TYPE_NAME_USER, "system", VIRTUAL_COLLECTION_USERS, user_query);
        /* CHECKPOINT: json_free(user_query); */
        
        if (user_result) {
          json_value_t* user_docs = json_object_get(user_result, "documents");
          if (user_docs && user_docs->type == JSON_ARRAY && user_docs->value.array.size > 0) {
            json_value_t* user_obj = json_array_get(user_docs, 0);
            if (user_obj && user_obj->type == JSON_OBJECT) {
              /* Get user roles */
              json_value_t* user_roles = json_object_get(user_obj, "roles");
              if (user_roles && user_roles->type == JSON_ARRAY) {
                /* Find role in user */
                for (size_t j = 0; j < user_roles->value.array.size; j++) {
                  json_value_t* id = user_roles->value.array.items[j];
                  if (id->type == JSON_STRING && strcmp(id->value.string, role_id) == 0) {
                    /* Remove role from user */
                    for (size_t k = j; k < user_roles->value.array.size - 1; k++) {
                      user_roles->value.array.items[k] = user_roles->value.array.items[k + 1];
                    }
                    user_roles->value.array.size--;
                    break;
                  }
                }
                /* Update user document in database */
                const char* user_uuid = json_object_get_string(user_obj, "uuid");
                if (user_uuid) {
                  virtual_update(ctx->db, user_uuid, user_obj);
                }
              }
            }
          }
          /* CHECKPOINT: json_free(user_result); */
        }
      }
    }
    
    /* Clear current users array */
    current_users->value.array.size = 0;
    
    /* Add new users */
    for (size_t i = 0; i < users_val->value.array.size; i++) {
      json_value_t* user_id_val = users_val->value.array.items[i];
      
      /* Handle both string and object formats */
      const char* user_id_str = NULL;
      if (user_id_val->type == JSON_STRING) {
        user_id_str = user_id_val->value.string;
      } else if (user_id_val->type == JSON_OBJECT) {
        json_value_t* id_val = json_object_get(user_id_val, "id");
        if (id_val && id_val->type == JSON_STRING) {
          user_id_str = id_val->value.string;
        }
      }
      
      if (user_id_str) {
        /* Check if user exists in database */
        json_value_t* user_query = json_create_object();
        json_object_set(user_query, "type", json_create_string(DOC_TYPE_NAME_USER));
        json_object_set(user_query, "uuid", json_create_string(user_id_str));
        json_object_set(user_query, "library", json_create_string("system"));
        
        json_value_t* user_result = virtual_query(ctx->db, DOC_TYPE_NAME_USER, "system", VIRTUAL_COLLECTION_USERS, user_query);
        /* CHECKPOINT: json_free(user_query); */
        
        if (user_result) {
          json_value_t* user_docs = json_object_get(user_result, "documents");
          if (user_docs && user_docs->type == JSON_ARRAY && user_docs->value.array.size > 0) {
            json_value_t* user_obj = json_array_get(user_docs, 0);
            if (user_obj && user_obj->type == JSON_OBJECT) {
              /* Add user to role */
              json_array_append(current_users, json_create_string(user_id_str));
              
              /* Add role to user's roles */
              json_value_t* user_roles = json_object_get(user_obj, "roles");
              if (!user_roles || user_roles->type != JSON_ARRAY) {
                /* Create roles array if it doesn't exist */
                user_roles = json_create_array();
                json_object_set(user_obj, "roles", user_roles);
              }
              
              /* Check if role is already in user's roles */
              int role_found = 0;
              for (size_t j = 0; j < user_roles->value.array.size; j++) {
                json_value_t* id = user_roles->value.array.items[j];
                if (id->type == JSON_STRING && strcmp(id->value.string, role_id) == 0) {
                  role_found = 1;
                  break;
                }
              }
              
              /* Add role to user if not already present */
              if (!role_found) {
                json_array_append(user_roles, json_create_string(role_id));
              }
              
              /* Update user document in database */
              const char* user_uuid = json_object_get_string(user_obj, "uuid");
              if (user_uuid) {
                virtual_update(ctx->db, user_uuid, user_obj);
              }
            }
          }
          /* CHECKPOINT: json_free(user_result); */
        }
      }
    }
  }
  
  /* Get updated role for response */
  rbac_role_t* role = rbac_get_role(ctx->rbac, role_id);
  
  /* Create response */
  json_value_t* response = json_create_object();
  json_value_t* role_obj = json_create_object();
  
  json_object_set(role_obj, "id", json_create_string(role->id));
  json_object_set(role_obj, "name", json_create_string(role->name));
  
  /* Add permissions object */
  json_value_t* permissions_obj = json_create_object();
  
  /* Copy permissions from role */
  if (role->permissions) {
    for (size_t i = 0; i < role->permissions->value.object.size; i++) {
      const char* perm_key = role->permissions->value.object.entries[i].key;
      json_value_t* perm_val = role->permissions->value.object.entries[i].value;
      
      if (perm_val && (perm_val->type == JSON_NUMBER || perm_val->type == JSON_INTEGER)) {
        json_object_set(permissions_obj, perm_key, 
                json_create_number(perm_val->type == JSON_NUMBER ? 
                         perm_val->value.number : perm_val->value.integer));
      }
    }
  }
  
  json_object_set(role_obj, "permissions", permissions_obj);
  
  /* Add users array */
  json_value_t* users_array = json_create_array();
  
  /* Get role users from the role_json we already have */
  json_value_t* users = json_object_get(role_json, "users");
  if (users && users->type == JSON_ARRAY) {
    for (size_t i = 0; i < users->value.array.size; i++) {
      json_value_t* user_id_val = users->value.array.items[i];
      if (user_id_val && user_id_val->type == JSON_STRING) {
        /* Add user ID to array */
        json_array_append(users_array, json_create_string(user_id_val->value.string));
      }
    }
  }
  
  json_object_set(role_obj, "users", users_array);
  
  json_object_set(response, "role", role_obj);
  
  /* Serialize response */
  char* response_str = json_stringify(response);
  
  /* Update the role document in the database */
  const char* role_uuid = json_object_get_string(role_json, "uuid");
  if (role_uuid) {
    virtual_update(ctx->db, role_uuid, role_json);
  }
  
  /* Free resources */
  /* CHECKPOINT: json_free(response); */
  /* CHECKPOINT: json_free(body); */
  rbac_free_role(role);
  /* CHECKPOINT: json_free(role_result); */
  
  /* Create and return response */
  return create_http_response(HTTP_OK, response_str, "application/json");
}

http_response_t* api_handle_role_delete(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  if (!ctx->rbac) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"RBAC system not initialized\"}", "application/json");
  }
  
  /* Extract role ID from path */
  const char* path = request->path;
  if (strncmp(path, "/api/roles/", 11) != 0) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid path\"}", "application/json");
  }
  
  const char* role_id = path + 11;
  
  /* Check if the user has appropriate permissions */
  /* Extract token and get user ID */
  char* token = api_extract_token(request);
  if (!token) {
    /* This should not happen since authorization is already checked in api_dispatch_request */
    return create_http_response(HTTP_UNAUTHORIZED, 
                 "{\"error\":\"Unauthorized\"}", "application/json");
  }
  
  /* Decode token */
  jwt_token_t* jwt = jwt_decode(token);
  BUFFER_FREE(token);
  
  if (!jwt || !jwt->payload || !jwt->payload->sub) {
    if (jwt) jwt_free(jwt);
    return create_http_response(HTTP_UNAUTHORIZED, 
                 "{\"error\":\"Invalid token\"}", "application/json");
  }
  
  const char* user_id = jwt->payload->sub;
  
  /* Check if user has permission to delete role */
  if (!rbac_check_permission(ctx->rbac, user_id, RBAC_ROLE, role_id, RBAC_DELETE) &&
    !rbac_check_permission(ctx->rbac, user_id, RBAC_ROLE, "*", RBAC_DELETE)) {
    jwt_free(jwt);
    return create_http_response(HTTP_FORBIDDEN, 
                 "{\"error\":\"Permission denied\"}", "application/json");
  }
  
  jwt_free(jwt);
  
  /* Check if role exists in database */
  json_value_t* role_query = json_create_object();
  json_object_set(role_query, "type", json_create_string(DOC_TYPE_NAME_ROLE));
  json_object_set(role_query, "uuid", json_create_string(role_id));
  json_object_set(role_query, "library", json_create_string("system"));
  
  json_value_t* role_result = virtual_query(ctx->db, DOC_TYPE_NAME_ROLE, "system", VIRTUAL_COLLECTION_ROLES, role_query);
  /* CHECKPOINT: json_free(role_query); */
  
  if (!role_result) {
    return create_http_response(HTTP_NOT_FOUND, 
                 "{\"error\":\"Role not found\"}", "application/json");
  }
  
  json_value_t* role_docs = json_object_get(role_result, "documents");
  if (!role_docs || role_docs->type != JSON_ARRAY || role_docs->value.array.size == 0) {
    /* CHECKPOINT: json_free(role_result); */
    return create_http_response(HTTP_NOT_FOUND, 
                 "{\"error\":\"Role not found\"}", "application/json");
  }
  
  /* Special case: Don't allow deletion of the admin role */
  json_value_t* role_json = json_array_get(role_docs, 0);
  if (role_json && role_json->type == JSON_OBJECT) {
    json_value_t* name_val = json_object_get(role_json, "name");
    if (name_val && name_val->type == JSON_STRING && 
      strcmp(name_val->value.string, "admin") == 0) {
      /* CHECKPOINT: json_free(role_result); */
      return create_http_response(HTTP_FORBIDDEN, 
                   "{\"error\":\"Cannot delete the admin role\"}", "application/json");
    }
  }
  /* CHECKPOINT: json_free(role_result); */
  
  /* Delete role */
  if (!rbac_delete_role(ctx->rbac, role_id)) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to delete role\"}", "application/json");
  }
  
  /* Return success with no content */
  return create_http_response(HTTP_NO_CONTENT, NULL, "application/json");
}
#endif /* RBAC handlers moved to api_rbac.c */

/* Configuration handlers are now in config_api.c */

/* Metrics handlers - MOVED TO api_metrics.c as part of Phase 2.5A API module extraction */
#if 0
/* Metrics handler for authenticated users accessing /api/metrics endpoints */
http_response_t* api_handle_metrics_get(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
#ifndef TOOLS_BUILD
  if (!g_metrics_registry) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Metrics registry not initialized\"}", "application/json");
  }
  
  /* Parse the path to determine which metrics to return */
  char* path = request->path;
  char* category = NULL;
  
  /* Extract the category from the path if present */
  if (strcmp(path, "/api/metrics/stats") == 0) {
    category = "stats";
  } else if (strcmp(path, "/api/metrics/activity") == 0) {
    category = "activity";
  }
  
  /* Check for query parameters */
  char* type_filter = NULL;
  char* name_filter = NULL;
  
  if (request->query) {
    char* query = BUFFER_STRDUP(request->query);
    if (query) {
      char* token = strtok(query, "&");
      while (token) {
        if (strncmp(token, "type=", 5) == 0) {
          type_filter = BUFFER_STRDUP(token + 5);
        } else if (strncmp(token, "name=", 5) == 0) {
          name_filter = BUFFER_STRDUP(token + 5);
        }
        token = strtok(NULL, "&");
      }
      BUFFER_FREE(query);
    }
  }
  
  /* Get all metrics as JSON */
  char* metrics_json = metrics_get_json(g_metrics_registry);
  if (!metrics_json) {
    if (type_filter) BUFFER_FREE(type_filter);
    if (name_filter) BUFFER_FREE(name_filter);
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Failed to get metrics\"}", "application/json");
  }
  
  /* If no filters are specified, return all metrics */
  if (!category && !type_filter && !name_filter) {
    /* Create response */
    http_response_t* response = create_http_response(HTTP_OK, metrics_json, "application/json");
    
    /* Free metrics JSON */
    BUFFER_FREE(metrics_json);
    
    return response;
  }
  
  /* Parse the metrics JSON to filter based on the criteria */
  json_value_t* root = json_parse(metrics_json);
  if (!root) {
    if (type_filter) BUFFER_FREE(type_filter);
    if (name_filter) BUFFER_FREE(name_filter);
    BUFFER_FREE(metrics_json);
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to parse metrics JSON\"}", "application/json");
  }
  
  /* Create a new metrics array for the filtered metrics */
  json_value_t* filtered_metrics = json_create_array();
  if (!filtered_metrics) {
    if (type_filter) BUFFER_FREE(type_filter);
    if (name_filter) BUFFER_FREE(name_filter);
    /* CHECKPOINT: json_free(root); */
    BUFFER_FREE(metrics_json);
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Memory allocation failure\"}", "application/json");
  }
  
  /* Get the metrics array */
  json_value_t* metrics_array = json_object_get(root, "metrics");
  if (metrics_array && metrics_array->type == JSON_ARRAY) {
    for (size_t i = 0; i < metrics_array->value.array.size; i++) {
      json_value_t* metric = json_array_get(metrics_array, i);
      
      /* Extract metric properties for filtering */
      json_value_t* name_val = json_object_get(metric, "name");
      json_value_t* type_val = json_object_get(metric, "type");
      
      if (!name_val || name_val->type != JSON_STRING || 
        !type_val || type_val->type != JSON_STRING) {
        continue; /* Skip metrics with missing or invalid required fields */
      }
      
      const char* name = name_val->value.string;
      const char* type = type_val->value.string;
      
      /* Check if this metric matches the filters */
      int include_metric = 1;
      
      /* Apply category filter */
      if (category) {
        /* For now, we use simple prefix matching for categories */
        if (strcmp(category, "stats") == 0) {
          /* Include metrics related to statistics */
          if (!(strstr(name, "server_") == name || 
             strstr(name, "db_") == name || 
             strstr(name, "system_") == name)) {
            include_metric = 0;
          }
        } else if (strcmp(category, "activity") == 0) {
          /* Include metrics related to activity */
          if (!(strstr(name, "active_") == name || 
             strstr(name, "requests_") == name || 
             strstr(name, "operations_") == name || 
             strstr(name, "queries_") == name)) {
            include_metric = 0;
          }
        }
      }
      
      /* Apply type filter */
      if (include_metric && type_filter) {
        if (strcmp(type, type_filter) != 0) {
          include_metric = 0;
        }
      }
      
      /* Apply name filter */
      if (include_metric && name_filter) {
        if (strstr(name, name_filter) == NULL) {
          include_metric = 0;
        }
      }
      
      /* If all filters pass, include this metric */
      if (include_metric) {
        /* Create a deep copy of the metric */
        json_value_t* metric_copy = json_clone(metric);
        if (metric_copy) {
          json_array_append(filtered_metrics, metric_copy);
        }
      }
    }
  }
  
  /* Create a new JSON object with the filtered metrics */
  json_value_t* filtered_obj = json_create_object();
  if (!filtered_obj) {
    if (type_filter) BUFFER_FREE(type_filter);
    if (name_filter) BUFFER_FREE(name_filter);
    /* CHECKPOINT: json_free(filtered_metrics); */
    /* CHECKPOINT: json_free(root); */
    BUFFER_FREE(metrics_json);
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Memory allocation failure\"}", "application/json");
  }
  
  json_object_set(filtered_obj, "metrics", filtered_metrics);
  
  /* Convert to JSON string */
  char* filtered_json = json_stringify(filtered_obj);
  
  /* Clean up */
  if (type_filter) BUFFER_FREE(type_filter);
  if (name_filter) BUFFER_FREE(name_filter);
  /* CHECKPOINT: json_free(filtered_obj); */ /* This will also free filtered_metrics */
  /* CHECKPOINT: json_free(root); */
  BUFFER_FREE(metrics_json);
  
  if (!filtered_json) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to generate response\"}", "application/json");
  }
  
  /* Create response */
  http_response_t* response = create_http_response(HTTP_OK, filtered_json, "application/json");
  
  /* Free filtered JSON */
  BUFFER_FREE(filtered_json);
  
  return response;
#else
  /* In tools build, return empty metrics */
  return create_http_response(HTTP_OK, "{\"metrics\":[]}", "application/json");
#endif
}

/* Metrics handler for public /metrics endpoint (Prometheus/OpenMetrics format) */
http_response_t* api_handle_metrics(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
#ifndef TOOLS_BUILD
  if (!g_metrics_registry) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Metrics registry not initialized\"}", "application/json");
  }
  
  /* Get metrics JSON representation */
  char* metrics_json = metrics_get_json(g_metrics_registry);
  if (!metrics_json) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to get metrics\"}", "application/json");
  }
  
  /* Convert JSON to Prometheus format (simplified) */
  json_value_t* root = json_parse(metrics_json);
  if (!root) {
    BUFFER_FREE(metrics_json);
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to parse metrics JSON\"}", "application/json");
  }
  
  /* Create a buffer for the Prometheus format output */
  size_t buffer_size = 10240; /* Start with 10KB buffer */
  char* prom_buffer = (char*)BUFFER_ALLOC(buffer_size);
  if (!prom_buffer) {
    /* CHECKPOINT: json_free(root); */
    BUFFER_FREE(metrics_json);
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Memory allocation failure\"}", "application/json");
  }
  
  /* Initialize buffer */
  size_t buffer_used = 0;
  prom_buffer[0] = '\0';
  
  /* Get the metrics array */
  json_value_t* metrics_array = json_object_get(root, "metrics");
  if (metrics_array && metrics_array->type == JSON_ARRAY) {
    for (size_t i = 0; i < metrics_array->value.array.size; i++) {
      json_value_t* metric = json_array_get(metrics_array, i);
      
      /* Extract metric properties */
      json_value_t* name_val = json_object_get(metric, "name");
      json_value_t* description_val = json_object_get(metric, "description");
      json_value_t* type_val = json_object_get(metric, "type");
      
      if (!name_val || name_val->type != JSON_STRING || 
        !type_val || type_val->type != JSON_STRING) {
        continue; /* Skip metrics with missing or invalid required fields */
      }
      
      const char* name = name_val->value.string;
      const char* type = type_val->value.string;
      const char* description = description_val && description_val->type == JSON_STRING ? 
                   description_val->value.string : "";
      
      /* Add metric description as a comment */
      buffer_used += snprintf(prom_buffer + buffer_used, buffer_size - buffer_used, 
                  "# HELP %s %s\n", name, description);
      
      /* Add metric type */
      const char* prom_type = "untyped";
      if (strcmp(type, "counter") == 0) {
        prom_type = "counter";
      } else if (strcmp(type, "gauge") == 0) {
        prom_type = "gauge";
      } else if (strcmp(type, "timer") == 0) {
        prom_type = "summary";
      } else if (strcmp(type, "histogram") == 0) {
        prom_type = "histogram";
      }
      
      buffer_used += snprintf(prom_buffer + buffer_used, buffer_size - buffer_used, 
                  "# TYPE %s %s\n", name, prom_type);
      
      /* Add metric value(s) based on type */
      if (strcmp(type, "counter") == 0) {
        json_value_t* value = json_object_get(metric, "value");
        if (value && value->type == JSON_NUMBER) {
          buffer_used += snprintf(prom_buffer + buffer_used, buffer_size - buffer_used, 
                     "%s %g\n", name, value->value.number);
        }
      } else if (strcmp(type, "gauge") == 0) {
        json_value_t* value = json_object_get(metric, "value");
        if (value && value->type == JSON_NUMBER) {
          buffer_used += snprintf(prom_buffer + buffer_used, buffer_size - buffer_used, 
                     "%s %g\n", name, value->value.number);
        }
      } else if (strcmp(type, "timer") == 0) {
        json_value_t* count = json_object_get(metric, "count");
        json_value_t* sum = json_object_get(metric, "sum");
        json_value_t* min = json_object_get(metric, "min");
        json_value_t* max = json_object_get(metric, "max");
        
        if (count && count->type == JSON_NUMBER) {
          buffer_used += snprintf(prom_buffer + buffer_used, buffer_size - buffer_used, 
                     "%s_count %g\n", name, count->value.number);
        }
        
        if (sum && sum->type == JSON_NUMBER) {
          buffer_used += snprintf(prom_buffer + buffer_used, buffer_size - buffer_used, 
                     "%s_sum %g\n", name, sum->value.number);
        }
        
        if (min && min->type == JSON_NUMBER) {
          buffer_used += snprintf(prom_buffer + buffer_used, buffer_size - buffer_used, 
                     "%s_min %g\n", name, min->value.number);
        }
        
        if (max && max->type == JSON_NUMBER) {
          buffer_used += snprintf(prom_buffer + buffer_used, buffer_size - buffer_used, 
                     "%s_max %g\n", name, max->value.number);
        }
      } else if (strcmp(type, "histogram") == 0) {
        json_value_t* count = json_object_get(metric, "count");
        json_value_t* sum = json_object_get(metric, "sum");
        json_value_t* buckets = json_object_get(metric, "buckets");
        
        if (count && count->type == JSON_NUMBER) {
          buffer_used += snprintf(prom_buffer + buffer_used, buffer_size - buffer_used, 
                     "%s_count %g\n", name, count->value.number);
        }
        
        if (sum && sum->type == JSON_NUMBER) {
          buffer_used += snprintf(prom_buffer + buffer_used, buffer_size - buffer_used, 
                     "%s_sum %g\n", name, sum->value.number);
        }
        
        if (buckets && buckets->type == JSON_ARRAY) {
          for (size_t j = 0; j < buckets->value.array.size; j++) {
            json_value_t* bucket = json_array_get(buckets, j);
            json_value_t* le = json_object_get(bucket, "le");
            json_value_t* bucket_count = json_object_get(bucket, "count");
            
            if (le && le->type == JSON_NUMBER && 
              bucket_count && bucket_count->type == JSON_NUMBER) {
              buffer_used += snprintf(prom_buffer + buffer_used, buffer_size - buffer_used, 
                         "%s_bucket{le=\"%g\"} %g\n", 
                         name, le->value.number, bucket_count->value.number);
            }
          }
          
          /* Add +Inf bucket with the total count */
          if (count && count->type == JSON_NUMBER) {
            buffer_used += snprintf(prom_buffer + buffer_used, buffer_size - buffer_used, 
                       "%s_bucket{le=\"+Inf\"} %g\n", name, count->value.number);
          }
        }
      }
      
      /* Add extra newline between metrics for readability */
      buffer_used += snprintf(prom_buffer + buffer_used, buffer_size - buffer_used, "\n");
      
      /* Check for buffer overflow and resize if needed */
      if (buffer_used >= buffer_size - 1024) {
        buffer_size *= 2;
        char* new_buffer = (char*)BUFFER_REALLOC(prom_buffer, buffer_size);
        if (!new_buffer) {
          BUFFER_FREE(prom_buffer);
          /* CHECKPOINT: json_free(root); */
          BUFFER_FREE(metrics_json);
          return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                       "{\"error\":\"Memory allocation failure\"}", "application/json");
        }
        prom_buffer = new_buffer;
      }
    }
  }
  
  /* Create response */
  http_response_t* response = create_http_response(HTTP_OK, prom_buffer, "text/plain");
  
  /* Clean up */
  BUFFER_FREE(prom_buffer);
  /* CHECKPOINT: json_free(root); */
  BUFFER_FREE(metrics_json);
  
  return response;
#else
  /* In tools build, return empty metrics */
  return create_http_response(HTTP_OK, "# No metrics available in tools build\n", "text/plain");
#endif
}

/* Handler for metrics export to file */
http_response_t* api_handle_metrics_export(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
#ifndef TOOLS_BUILD
  if (!g_metrics_registry) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Metrics registry not initialized\"}", "application/json");
  }
  
  /* Check if request is POST with JSON body */
  if (request->method != HTTP_POST) {
    return create_http_response(HTTP_METHOD_NOT_ALLOWED,
                 "{\"error\":\"Method not allowed\"}", "application/json");
  }
  
  /* Parse JSON request body */
  json_value_t* req_body = json_parse(request->body);
  if (!req_body) {
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid JSON request body\"}", "application/json");
  }
  
  /* Extract export path */
  json_value_t* export_path_val = json_object_get(req_body, "export_path");
  if (!export_path_val || export_path_val->type != JSON_STRING) {
    /* CHECKPOINT: json_free(req_body); */
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"export_path is required in request body\"}", 
                 "application/json");
  }
  
  const char* export_path = export_path_val->value.string;
  
  /* Export metrics to file */
  int result = metrics_registry_export(g_metrics_registry, export_path);
  
  /* Clean up */
  /* CHECKPOINT: json_free(req_body); */
  
  if (!result) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 "{\"error\":\"Failed to export metrics to file\"}", 
                 "application/json");
  }
  
  /* Return success response */
  char success_response[256];
  snprintf(success_response, sizeof(success_response),
      "{\"success\":true,\"message\":\"Metrics exported to %s\"}", export_path);
  
  return create_http_response(HTTP_OK, success_response, "application/json");
#else
  /* In tools build, return error */
  return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
               "{\"error\":\"Metrics export not available in tools build\"}", 
               "application/json");
#endif
}

/* Handler to list available metrics */
http_response_t* api_handle_metrics_available(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
#ifndef TOOLS_BUILD
  if (!g_metrics_registry) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Metrics registry not initialized\"}", "application/json");
  }
  
  /* Get metrics JSON representation */
  char* metrics_json = metrics_get_json(g_metrics_registry);
  if (!metrics_json) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to get metrics\"}", "application/json");
  }
  
  /* Parse the JSON to extract just the names and types */
  json_value_t* root = json_parse(metrics_json);
  if (!root) {
    BUFFER_FREE(metrics_json);
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to parse metrics JSON\"}", "application/json");
  }
  
  /* Create a new JSON object for the response */
  json_value_t* response_obj = json_create_object();
  if (!response_obj) {
    /* CHECKPOINT: json_free(root); */
    BUFFER_FREE(metrics_json);
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Memory allocation failure\"}", "application/json");
  }
  
  /* Create arrays for each metric type */
  json_value_t* counters_array = json_create_array();
  json_value_t* gauges_array = json_create_array();
  json_value_t* timers_array = json_create_array();
  json_value_t* histograms_array = json_create_array();
  
  if (!counters_array || !gauges_array || !timers_array || !histograms_array) {
    if (counters_array) {

        /* CHECKPOINT: json_free(counters_array); */

    }
    if (gauges_array) {

        /* CHECKPOINT: json_free(gauges_array); */

    }
    if (timers_array) {

        /* CHECKPOINT: json_free(timers_array); */

    }
    if (histograms_array) {

        /* CHECKPOINT: json_free(histograms_array); */

    }
    /* CHECKPOINT: json_free(response_obj); */
    /* CHECKPOINT: json_free(root); */
    BUFFER_FREE(metrics_json);
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Memory allocation failure\"}", "application/json");
  }
  
  /* Get the metrics array */
  json_value_t* metrics_array = json_object_get(root, "metrics");
  if (metrics_array && metrics_array->type == JSON_ARRAY) {
    for (size_t i = 0; i < metrics_array->value.array.size; i++) {
      json_value_t* metric = json_array_get(metrics_array, i);
      
      /* Extract metric properties */
      json_value_t* name_val = json_object_get(metric, "name");
      json_value_t* type_val = json_object_get(metric, "type");
      json_value_t* description_val = json_object_get(metric, "description");
      
      if (!name_val || name_val->type != JSON_STRING || 
        !type_val || type_val->type != JSON_STRING) {
        continue; /* Skip metrics with missing or invalid required fields */
      }
      
      /* Create a simple object with name and description */
      json_value_t* metric_info = json_create_object();
      if (!metric_info) {
        continue; /* Skip on memory allocation failure */
      }
      
      /* Add name and description */
      json_object_set(metric_info, "name", json_create_string(name_val->value.string));
      
      if (description_val && description_val->type == JSON_STRING) {
        json_object_set(metric_info, "description", 
                json_create_string(description_val->value.string));
      } else {
        json_object_set(metric_info, "description", json_create_string(""));
      }
      
      /* Add to the appropriate array based on type */
      if (strcmp(type_val->value.string, "counter") == 0) {
        json_array_append(counters_array, metric_info);
      } else if (strcmp(type_val->value.string, "gauge") == 0) {
        json_array_append(gauges_array, metric_info);
      } else if (strcmp(type_val->value.string, "timer") == 0) {
        json_array_append(timers_array, metric_info);
      } else if (strcmp(type_val->value.string, "histogram") == 0) {
        json_array_append(histograms_array, metric_info);
      } else {
        /* CHECKPOINT: json_free(metric_info); */ /* Unknown type, free the object */
      }
    }
  }
  
  /* Add arrays to response object */
  json_object_set(response_obj, "counters", counters_array);
  json_object_set(response_obj, "gauges", gauges_array);
  json_object_set(response_obj, "timers", timers_array);
  json_object_set(response_obj, "histograms", histograms_array);
  
  /* Convert to JSON string */
  char* response_json = json_stringify(response_obj);
  
  /* Clean up JSON objects */
  /* CHECKPOINT: json_free(response_obj); */
  /* CHECKPOINT: json_free(root); */
  BUFFER_FREE(metrics_json);
  
  if (!response_json) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to generate response\"}", "application/json");
  }
  
  /* Create response */
  http_response_t* response = create_http_response(HTTP_OK, response_json, "application/json");
  
  /* Free response JSON */
  BUFFER_FREE(response_json);
  
  return response;
#else
  /* In tools build, return empty metrics */
  return create_http_response(HTTP_OK, 
               "{\"counters\":[],\"gauges\":[],\"timers\":[],\"histograms\":[]}", 
               "application/json");
#endif
}
#endif /* Metrics handlers moved to api_metrics.c */

