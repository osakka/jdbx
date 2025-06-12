#include "init.h"
#include "database/index_cleanup.h"
#include "database/database.h"
#include "database/unified_documents.h"
#include "database/system_schemas.h"
#include "database/adaptive_indexer.h"
#include "database/index_metrics.h"
#include "database/index_cleanup.h"
#include "database/collection_defaults.h"
#include "database/library_metadata.h"
#include "js/js_native_storage.h"
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
  
  /* Check if JDBX mode is enabled */
  const char* use_jdbx = getenv("JSONDB_USE_JDBX");
  if (use_jdbx && strcmp(use_jdbx, "1") == 0) {
    INIT_LOG_PROGRESS("DATABASE", "Using JDBX single-file database format");
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
    
    /* Initialize native JavaScript storage system for in-memory database */
    INIT_LOG_PROGRESS("DATABASE", "Initializing native JavaScript storage system for in-memory database");
    if (!js_native_storage_init(db)) {
      INIT_LOG_FAILURE("DATABASE", "Failed to initialize native JavaScript storage system");
      db_close(db);
      return INIT_DATABASE_ERROR;
    }
    INIT_LOG_SUCCESS("DATABASE", "Native JavaScript storage system initialized for in-memory database");
    
    /* Initialize adaptive indexing for in-memory database */
    if (adaptive_indexer_init(db)) {
      if (adaptive_indexer_start()) {
        INIT_LOG_SUCCESS("DATABASE", "Adaptive indexing initialized for in-memory database");
      } else {
        INIT_LOG_FAILURE("DATABASE", "Failed to start adaptive indexer for in-memory database");
      }
    } else {
      INIT_LOG_FAILURE("DATABASE", "Failed to initialize adaptive indexer for in-memory database");
    }
    
    /* Initialize index metrics for in-memory database */
    if (index_metrics_init() == 0) {
      INIT_LOG_SUCCESS("DATABASE", "Index metrics initialized for in-memory database");
      
      /* Initialize index cleanup for in-memory database */
      index_metrics_t* metrics = index_metrics_get_collector();
      index_cleanup_t* cleanup = index_cleanup_init(db, metrics);
      if (cleanup) {
        index_cleanup_start(cleanup);
        index_cleanup_api_init(db, metrics);
        INIT_LOG_SUCCESS("DATABASE", "Index cleanup initialized for in-memory database");
      }
    }
    
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
    /* JDBX-only implementation handles bootstrap mode internally */
    
    /* Initialize system schemas */
    if (!db_init_system_schemas(db)) {
      INIT_LOG_FAILURE("DATABASE", "Failed to initialize system schemas");
      db_close(db);
      return INIT_DATABASE_ERROR;
    }
    INIT_LOG_SUCCESS("DATABASE", "System schemas initialized for bootstrap");
  }
  
  /* Always check for unified documents system - it may be missing even if system/config exists */
  if (!db_collection_exists(db, "documents")) {
    INIT_LOG_PROGRESS("DATABASE", "Initializing unified documents system");
    if (!unified_documents_init(db)) {
      INIT_LOG_FAILURE("DATABASE", "Failed to initialize unified documents system");
      db_close(db);
      return INIT_DATABASE_ERROR;
    }
    INIT_LOG_SUCCESS("DATABASE", "Unified documents system initialized");
  } else {
    INIT_LOG_SUCCESS("DATABASE", "Unified documents system already initialized");
  }
  
  /* Build indices for faster lookups */
  INIT_LOG_PROGRESS("DATABASE", "Building document indices for faster lookups");
  db_rebuild_indices(db);
  INIT_LOG_SUCCESS("DATABASE", "Document indices built successfully");
  
  /* Initialize native JavaScript storage system */
  INIT_LOG_PROGRESS("DATABASE", "Initializing native JavaScript storage system");
  if (!js_native_storage_init(db)) {
    INIT_LOG_FAILURE("DATABASE", "Failed to initialize native JavaScript storage system");
    db_close(db);
    return INIT_DATABASE_ERROR;
  }
  INIT_LOG_SUCCESS("DATABASE", "Native JavaScript storage system initialized");
  
  /* Initialize adaptive indexing system */
  INIT_LOG_PROGRESS("DATABASE", "Initializing adaptive indexing system");
  if (!adaptive_indexer_init(db)) {
    INIT_LOG_FAILURE("DATABASE", "Failed to initialize adaptive indexer");
    db_close(db);
    return INIT_DATABASE_ERROR;
  }
  
  if (!adaptive_indexer_start()) {
    INIT_LOG_FAILURE("DATABASE", "Failed to start adaptive indexer");
    adaptive_indexer_stop();
    db_close(db);
    return INIT_DATABASE_ERROR;
  }
  INIT_LOG_SUCCESS("DATABASE", "Adaptive indexing system initialized");
  
  /* Initialize index metrics system */
  INIT_LOG_PROGRESS("DATABASE", "Initializing index metrics system");
  if (index_metrics_init() != 0) {
    INIT_LOG_FAILURE("DATABASE", "Failed to initialize index metrics");
    adaptive_indexer_stop();
    db_close(db);
    return INIT_DATABASE_ERROR;
  }
  INIT_LOG_SUCCESS("DATABASE", "Index metrics system initialized");
  
  /* Initialize index cleanup system */
  INIT_LOG_PROGRESS("DATABASE", "Initializing index cleanup system");
  index_metrics_t* metrics = index_metrics_get_collector();
  index_cleanup_t* cleanup = index_cleanup_init(db, metrics);
  if (!cleanup) {
    INIT_LOG_FAILURE("DATABASE", "Failed to initialize index cleanup");
    index_metrics_cleanup();
    adaptive_indexer_stop();
    db_close(db);
    return INIT_DATABASE_ERROR;
  }
  
  if (index_cleanup_start(cleanup) != 0) {
    INIT_LOG_FAILURE("DATABASE", "Failed to start index cleanup");
    index_cleanup_destroy(cleanup);
    index_metrics_cleanup();
    adaptive_indexer_stop();
    db_close(db);
    return INIT_DATABASE_ERROR;
  }
  INIT_LOG_SUCCESS("DATABASE", "Index cleanup system initialized");
  
  /* Initialize cleanup API */
  extern void index_cleanup_api_init(database_t* db, index_metrics_t* metrics);
  index_cleanup_api_init(db, metrics);
  
  /* Set output parameter */
  *database_out = db;
  
  /* Register with cleanup system */
  init_register_database(db);
  
  INIT_LOG_SUCCESS("DATABASE", "Database initialized from '%s'", config->db_path);
  
  return INIT_OK;
}