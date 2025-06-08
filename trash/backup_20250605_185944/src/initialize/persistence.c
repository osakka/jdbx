#include "init.h"
#include "database/database.h"
#include "utils/logger.h"
#include "utils/metrics_persistence.h"

/* Start persistence thread after daemonization */
init_status_t init_persistence_thread(database_t* database) {
  INIT_LOG_PROGRESS("PERSISTENCE", "Starting persistence thread");
  
  if (!database) {
    INIT_LOG_FAILURE("PERSISTENCE", "NULL database provided");
    return INIT_PERSISTENCE_ERROR;
  }
  
  /* Start persistence thread */
  if (!db_start_persistence_thread(database)) {
    INIT_LOG_FAILURE("PERSISTENCE", "Failed to start persistence thread");
    return INIT_PERSISTENCE_ERROR;
  }
  
  INIT_LOG_SUCCESS("PERSISTENCE", "Persistence thread started successfully");
  
  /* Initialize metrics persistence */
  INIT_LOG_PROGRESS("PERSISTENCE", "Initializing metrics persistence");
  if (!metrics_persistence_init(database)) {
    INIT_LOG_WARNING("PERSISTENCE", "Failed to initialize metrics persistence (non-fatal)");
    /* Don't fail server startup if metrics persistence fails */
  } else {
    INIT_LOG_SUCCESS("PERSISTENCE", "Metrics persistence initialized");
  }
  
  return INIT_OK;
}