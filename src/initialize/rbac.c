#include "init.h"
#include "rbac/rbac.h"
#include "rbac/rbac_database.h"
#include "rbac/rbac_permissions.h"
#include "rbac/rbac.h"
#include "rbac/rbac_refcount.h"
#include "database/database.h"
#include "database/system_schemas.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

/* Initialize RBAC system */
init_status_t init_rbac(server_config_t* config, database_t* database, 
           rbac_system_t** rbac_out, rbac_refcount_t** rbac_ref_out) {
  INIT_LOG_PROGRESS("RBAC", "Initializing RBAC");
  
  if (!config) {
    INIT_LOG_FAILURE("RBAC", "NULL server configuration");
    return INIT_RBAC_ERROR;
  }
  
  if (!database) {
    INIT_LOG_FAILURE("RBAC", "NULL database");
    return INIT_RBAC_ERROR;
  }
  
  if (!rbac_out || !rbac_ref_out) {
    INIT_LOG_FAILURE("RBAC", "NULL output parameters");
    return INIT_RBAC_ERROR;
  }
  
  /* Use database-backed RBAC implementation */
  INIT_LOG_PROGRESS("RBAC", "Initializing database-backed RBAC system");
  LOG_TRACE("RBAC_TRACE: Database pointer: %p", database);
  LOG_TRACE("RBAC_TRACE: JWT secret: %s", config->jwt_secret ? "***" : "NULL");
  
  /* Initialize with database backend */
  rbac_system_t* rbac = rbac_database_init(database, config->jwt_secret);
  if (!rbac) {
    INIT_LOG_FAILURE("RBAC", "Failed to initialize database-backed RBAC system");
    return INIT_RBAC_ERROR;
  }
  INIT_LOG_SUCCESS("RBAC", "Database-backed RBAC system initialized");
  
  LOG_TRACE("RBAC_TRACE: RBAC system pointer: %p", rbac);
  
  /* Initialize reference counting wrapper */
  rbac_refcount_t* rbac_ref = rbac_to_refcount(rbac);
  if (!rbac_ref) {
    INIT_LOG_FAILURE("RBAC", "Failed to create RBAC reference counting wrapper");
    free(rbac);
    return INIT_RBAC_ERROR;
  }
  
  INIT_LOG_SUCCESS("RBAC", "RBAC reference counting wrapper created");
  
  /* Set output parameters */
  *rbac_out = rbac;
  *rbac_ref_out = rbac_ref;
  
  /* Register with cleanup system */
  init_register_rbac(rbac, rbac_ref);
  
  /* Complete bootstrap if we were in bootstrap mode */
  if (database->is_bootstrap_mode) {
    INIT_LOG_PROGRESS("RBAC", "Completing bootstrap mode");
    db_complete_bootstrap(database);
    INIT_LOG_SUCCESS("RBAC", "Bootstrap mode completed - normal operation enabled");
  }
  
  /* 
   * Try to load the enhanced RBAC system in a background thread
   * This allows the server to start and function with the minimal RBAC
   * while the enhanced system is being initialized asynchronously
   */
  
  return INIT_OK;
}