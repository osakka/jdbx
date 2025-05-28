#include "init.h"
#include "database/database.h"
#include "database/system_schemas.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

/* Initialize database */
init_status_t init_database(server_config_t* config, database_t** database_out) {
  INIT_LOG_PROGRESS("DATABASE", "Initializing database");
  
  if (!config) {
    INIT_LOG_FAILURE("DATABASE", "NULL server configuration");
    return INIT_DATABASE_ERROR;
  }
  
  if (!database_out) {
    INIT_LOG_FAILURE("DATABASE", "NULL output parameter");
    return INIT_DATABASE_ERROR;
  }
  
  /* Check if path is specified */
  if (!config->db_path || strlen(config->db_path) == 0) {
    INIT_LOG_PROGRESS("DATABASE", "No database path specified, using in-memory database");
    /* Use db_init with NULL path for in-memory */
    database_t* db = db_init(NULL);
    if (!db) {
      INIT_LOG_FAILURE("DATABASE", "Failed to create in-memory database");
      return INIT_DATABASE_ERROR;
    }
    
    INIT_LOG_SUCCESS("DATABASE", "In-memory database initialized");
    *database_out = db;
    
    /* Register with cleanup system */
    init_register_database(db);
    
    return INIT_OK;
  }
  
  /* Initialize with file database */
  INIT_LOG_PROGRESS("DATABASE", "Initializing database from '%s'", config->db_path);
  
  /* Use db_init per database.h */
  database_t* db = db_init(config->db_path);
  if (!db) {
    INIT_LOG_FAILURE("DATABASE", "Failed to open database from '%s', creating new database", config->db_path);
    return INIT_DATABASE_ERROR;
  }
  
  /* Check if database needs bootstrap */
  if (db_needs_bootstrap(db)) {
    INIT_LOG_PROGRESS("DATABASE", "Database needs bootstrap initialization");
    db->is_bootstrap_mode = 1;
    
    /* Initialize system schemas */
    if (!db_init_system_schemas(db)) {
      INIT_LOG_FAILURE("DATABASE", "Failed to initialize system schemas");
      db_close(db);
      return INIT_DATABASE_ERROR;
    }
    INIT_LOG_SUCCESS("DATABASE", "System schemas initialized for bootstrap");
  }
  
  /* Build indices for faster lookups */
  INIT_LOG_PROGRESS("DATABASE", "Building document indices for faster lookups");
  db_rebuild_indices(db);
  INIT_LOG_SUCCESS("DATABASE", "Document indices built successfully");
  
  /* Set output parameter */
  *database_out = db;
  
  /* Register with cleanup system */
  init_register_database(db);
  
  INIT_LOG_SUCCESS("DATABASE", "Database initialized from '%s'", config->db_path);
  
  return INIT_OK;
}