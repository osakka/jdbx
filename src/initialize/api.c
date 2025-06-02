#include "init.h"
#include "api/api.h"
#include "api/rbac_api.h"
#include "api/health_api.h"
#include "js/js_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Initialize API context and register routes */
init_status_t init_api(server_config_t* config, database_t* database, 
          rbac_system_t* rbac, api_context_t** api_ctx_out) {
  INIT_LOG_PROGRESS("API", "Initializing API context");
  
  if (!config) {
    INIT_LOG_FAILURE("API", "NULL server configuration");
    return INIT_API_ERROR;
  }
  
  if (!database) {
    INIT_LOG_FAILURE("API", "NULL database");
    return INIT_API_ERROR;
  }
  
  if (!rbac) {
    INIT_LOG_FAILURE("API", "NULL RBAC system");
    return INIT_API_ERROR;
  }
  
  if (!api_ctx_out) {
    INIT_LOG_FAILURE("API", "NULL output parameter");
    return INIT_API_ERROR;
  }
  
  /* Create API context */
  INIT_LOG_PROGRESS("API", "Creating API context with JWT secret: '%s'", config->jwt_secret);
  
  api_context_t* api_ctx = api_create_context(database, rbac, config->jwt_secret);
  if (!api_ctx) {
    INIT_LOG_FAILURE("API", "Failed to create API context");
    return INIT_API_ERROR;
  }
  
  /* Initialize JavaScript API */
  INIT_LOG_PROGRESS("API", "Initializing JavaScript engine");
  js_api_init(database);

  /* Register native JavaScript API routes */
  INIT_LOG_PROGRESS("API", "Registering native JavaScript API routes");
  if (api_ctx) {
    int routes_before = api_ctx->num_routes;
    api_ctx->num_routes = register_js_native_api_routes(api_ctx->routes, api_ctx->num_routes);
    int routes_added = api_ctx->num_routes - routes_before;
    INIT_LOG_SUCCESS("API", "Native JavaScript API routes registered successfully - added %d routes (total: %d)", 
            routes_added, api_ctx->num_routes);
  } else {
    INIT_LOG_FAILURE("API", "Cannot register native JavaScript API routes - NULL context");
  }
  
  /* Register RBAC API routes */
  INIT_LOG_PROGRESS("API", "Registering RBAC API routes");
  
  if (api_ctx && database && rbac) {
    int routes_before = api_ctx->num_routes;
    api_ctx->num_routes = rbac_api_register_routes(api_ctx->routes, api_ctx->num_routes, 
                           database, rbac);
    int routes_added = api_ctx->num_routes - routes_before;
    INIT_LOG_SUCCESS("API", "RBAC API routes registered successfully - added %d routes (total: %d)", 
            routes_added, api_ctx->num_routes);
  } else {
    if (g_logger) {
      LOG_WARNING("[INIT:API] Cannot register RBAC API routes - missing context, database or RBAC system");
    } else {
      fprintf(stderr, "[INIT:API] WARNING: Cannot register RBAC API routes - missing context, database or RBAC system\n");
    }
  }
  
  /* Initialize health API */
  INIT_LOG_PROGRESS("API", "Initializing health monitoring API");
  health_api_init();

  /* Register health API endpoints */
  INIT_LOG_PROGRESS("API", "Registering health API endpoints");
  if (api_ctx) {
    register_health_api_endpoints(api_ctx);
    INIT_LOG_SUCCESS("API", "Health API endpoints registered");
  } else {
    if (g_logger) {
      LOG_WARNING("[INIT:API] API context not available, health endpoints not registered");
    } else {
      fprintf(stderr, "[INIT:API] WARNING: API context not available, health endpoints not registered\n");
    }
  }
  
  
  INIT_LOG_SUCCESS("API", "API context initialized with all routes successfully");
  
  /* Set output parameter */
  *api_ctx_out = api_ctx;
  
  /* Register with cleanup system */
  init_register_api(api_ctx);
  
  return INIT_OK;
}