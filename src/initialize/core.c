#include "init.h"
#include "utils/logger.h"
#include "utils/config_loader.h"
#include "core/server.h"
#include "utils/metrics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "utils/buffer_pool.h"

/* Global variables for cleanup handling */
static database_t* g_database = NULL;
static rbac_refcount_t* g_rbac_ref = NULL;
static rbac_system_t* g_rbac = NULL;
static api_context_t* g_api_ctx = NULL;

/* External globals required by other components */
struct metrics_registry* g_metrics_registry = NULL;
/* g_server_config is defined in config_loader.c */
extern server_config_t* g_server_config;

/* Global flag for verbose mode */
int g_verbose_mode = 0;

/* Flag to prevent multiple cleanup calls */
static int cleanup_registered = 0;
static int cleanup_in_progress = 0;

/* Process type tracking */
static int g_process_type = PROCESS_TYPE_MAIN;

/* Cleanup resources safely */
void init_cleanup(void) {
  /* Only cleanup in the actual server process */
  if (g_process_type != PROCESS_TYPE_SERVER) {
    /* Always log this at INFO level so we can see it */
    if (g_logger) {
      LOG_INFO("[INIT:CORE] Skipping cleanup in non-server process (type=%d)", g_process_type);
    } else {
      printf("[INIT:CORE] Skipping cleanup in non-server process (type=%d)\n", g_process_type);
    }
    return;
  }
  
  /* Prevent recursive cleanup */
  if (cleanup_in_progress) {
    return;
  }
  
  cleanup_in_progress = 1;
  
  INIT_LOG_PROGRESS("CORE", "Performing cleanup before shutdown in server process");

  /* Database persistence is automatic in unified documents architecture */
  if (g_database) {
    INIT_LOG_PROGRESS("CORE", "Database state automatically persisted");
    /* db_save() no longer needed - persistence is automatic */
  }

  if (g_rbac_ref && g_database && g_rbac) {
    INIT_LOG_PROGRESS("CORE", "Saving RBAC configuration");
    /* Use database-only RBAC save */
    rbac_permissions_save(g_database, g_rbac, NULL);
  }

  /* Clean up in a safe order to avoid double-free issues */

  /* 1. Free API context which might use DB and RBAC */
  if (g_api_ctx) {
    INIT_LOG_DEBUG("CORE", "Freeing API context");
    api_context_t* api = g_api_ctx;
    g_api_ctx = NULL; /* Clear global reference */
    api_free_context(api);
  }

  /* 2. Free database */
  if (g_database) {
    INIT_LOG_DEBUG("CORE", "Closing database");
    g_database = NULL; /* Clear global reference */
    /* Database cleanup is automatic in unified documents architecture */
  }

  /* 3. Request shutdown if the server is still running */
  INIT_LOG_DEBUG("CORE", "Requesting server shutdown");
  server_request_shutdown();

  /* 4. First free the regular RBAC reference if it exists */
  if (g_rbac) {
    INIT_LOG_DEBUG("CORE", "Freeing RBAC system");
    BUFFER_FREE(g_rbac); /* Just free the struct, not its contents (owned by g_rbac_ref) */
    g_rbac = NULL;
  }

  /* 5. Finally free RBAC system with reference counting */
  if (g_rbac_ref) {
    INIT_LOG_DEBUG("CORE", "Freeing RBAC reference counting system");
    rbac_refcount_t* rbac = g_rbac_ref;
    g_rbac_ref = NULL; /* Clear global reference */
    rbac_refcount_free(rbac);
  }
  
  /* Free metrics registry */
  if (g_metrics_registry) {
    INIT_LOG_DEBUG("CORE", "Freeing metrics registry");
    metrics_registry_t* metrics = g_metrics_registry;
    g_metrics_registry = NULL; /* Clear global reference */
    metrics_registry_free(metrics);
  }

  /* Close the logger last */
  if (g_logger) {
    INIT_LOG_PROGRESS("CORE", "Shutdown complete, closing logger");
    logger_close();
  } else {
    printf("[INIT:CORE] Shutdown complete\n");
  }
}

/* Register the database with the cleanup system */
void init_register_database(database_t* database) {
  g_database = database;
}

/* Register the RBAC system with the cleanup system */
void init_register_rbac(rbac_system_t* rbac, rbac_refcount_t* rbac_ref) {
  g_rbac = rbac;
  g_rbac_ref = rbac_ref;
}

/* Register the API context with the cleanup system */
void init_register_api(api_context_t* api_ctx) {
  g_api_ctx = api_ctx;
}

/* Register cleanup handler */
void init_register_cleanup(void) {
  if (!cleanup_registered) {
    atexit(init_cleanup);
    cleanup_registered = 1;
    INIT_LOG_DEBUG("CORE", "Registered cleanup handler");
  }
}

/* Set verbose mode flag */
void init_set_verbose(int verbose) {
  g_verbose_mode = verbose;
}

/* Register server configuration */
void init_register_config(server_config_t* config) {
  g_server_config = config;
}

/* Set process type */
void init_set_process_type(int type) {
  g_process_type = type;
  /* Always log this at INFO level so we can see it */
  if (g_logger) {
    LOG_INFO("[INIT:CORE] Process type set to: %d", type);
  } else {
    printf("[INIT:CORE] Process type set to: %d\n", type);
  }
}

/* Get process type */
int init_get_process_type(void) {
  return g_process_type;
}