/**
 * @file main.c
 * @brief JDBX Database Server - Main Entry Point
 * 
 * This is the primary entry point for the JDBX database server daemon.
 * Handles command-line argument parsing, initialization sequence coordination,
 * and graceful startup/shutdown procedures.
 * 
 * Initialization Sequence:
 * 1. Configuration loading and validation
 * 2. Logger initialization with configured levels
 * 3. Database backend initialization (JDBX with WAL)
 * 4. RBAC system setup with JWT authentication
 * 5. API routing and handler registration
 * 6. Thread pool and network socket initialization
 * 7. Daemon mode setup (if requested)
 * 8. Main server event loop
 * 
 * Features:
 * - Command-line configuration overrides
 * - Daemon mode with PID file management  
 * - Graceful shutdown on SIGTERM/SIGINT
 * - Production-ready defaults with development overrides
 * - Comprehensive error handling and recovery
 */

#include "init.h"
#include "utils/logger.h"
#include "utils/config_loader.h"
#include "utils/memory_manager.h"
#include "utils/memory_allocator_config.h"
#include "core/server_thread_safe.h"
#include "rbac/jwt_cache.h"
#include "utils/production_config.h"
#include "utils/buffer_pool.h"
#include "core/rate_limiter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>

/* External global variables needed for cross-component integration */
extern server_config_t* g_server_config;

/* Global rate limiter instance - defined in globals.c */
extern rate_limiter_t* g_rate_limiter;

/* Display banner and help information */
static void display_banner(void) {
  printf("JSON Database Server\n");
  printf("===================\n\n");
}

/* Print usage information */
static void print_usage(const char* program_name) {
  printf("Usage: %s [OPTIONS]\n\n", program_name);
  printf("JSON Database Server - A lightweight JSON document database\n\n");
  printf("Essential Options:\n");
  printf(" -h, --help              Display this help message and exit\n");
  printf(" -v, --version           Display version information and exit\n");
  printf("     --daemon            Run as a daemon (background mode)\n");
  printf("     --foreground        Run in foreground mode (do not daemonize)\n");
  printf("     --config=FILE       Load configuration from file\n");
  printf("\nConfiguration Options (long flags only):\n");
  printf("     --terminate         Terminate running server instance\n");
  printf("     --log-level=LEVEL   Set log level (error, warn, info, debug, trace)\n");
  printf("     --trace-categories=CATS Set trace categories (database,rbac,api,auth,transaction,binary,javascript,network,metrics,memory,all)\n");
  printf("     --db-file=FILE      Set database file basename (auto-generates .jdbx and .wal)\n");
  printf("     --pid-file=FILE     Set PID file path\n");
  printf("     --log-file=FILE     Set log file path\n");
  printf("     --web-root=DIR      Set web admin interface root directory\n");
  printf("     --port=PORT         Set server port (default: 5000)\n");
  printf("     --host=HOST         Set server bind address (default: 0.0.0.0)\n");
  printf("     --ssl               Enable SSL/TLS encryption\n");
  printf("     --no-ssl            Disable SSL/TLS encryption\n");
  printf("     --ssl-cert=FILE     Set SSL certificate file path\n");
  printf("     --ssl-key=FILE      Set SSL private key file path\n");
  printf("     --js-file=FILE      Execute JavaScript file and exit\n");
  printf("     --env-file=FILE     Load environment variables from file\n");
  printf("\nAdvanced Options:\n");
  printf("     --jdbx-initial-size=SIZE Set JDBX initial file size (bytes)\n");
  printf("     --jdbx-wal-size=SIZE     Set JDBX WAL size (bytes)\n");
  printf("     --thread-pool-min=N      Minimum thread pool size\n");
  printf("     --thread-pool-max=N      Maximum thread pool size\n");
  printf("\nConfiguration Priority (lowest to highest):\n");
  printf("  1. Environment file (jdbx.env) - auto-discovered or --env-file\n");
  printf("  2. Command line flags - override environment values\n");
  printf("  3. Database configuration - runtime changes via _system_config\n");
  printf("\n");
}

/* Print version information */
static void print_version(void) {
  printf("JDBX Database Server v7.2.5\n");
  printf("Built with: ART Engine, Checkpoint Memory, SSL/TLS, QuickJS\n");
  printf("Copyright (c) 2025 JDBX Project\n");
}

/* Read PID from PID file - needed by init_config for terminate mode */
pid_t read_pid_file() {
  /* Maximum path length using a reasonable size */
  char pid_file_path[4096];
  
  /* Check if config is available */
  server_config_t* config = g_server_config;
  if (config && config->pid_file) {
    strncpy(pid_file_path, config->pid_file, sizeof(pid_file_path) - 1);
    pid_file_path[sizeof(pid_file_path) - 1] = '\0';
  } else {
    /* No PID file path set */
    return -1;
  }
  
  /* Check if file exists first */
  if (access(pid_file_path, F_OK) != 0) {
    /* PID file doesn't exist, meaning no running instance */
    return -1;
  }
  
  /* Then try to open it */
  FILE* pid_fp = fopen(pid_file_path, "r");
  if (!pid_fp) {
    /* PID file exists but can't be opened - could be a permissions issue */
    fprintf(stderr, "Warning: Failed to open PID file '%s': %s\n", 
        pid_file_path, strerror(errno));
    return -1;
  }
  
  pid_t pid;
  if (fscanf(pid_fp, "%d", &pid) != 1) {
    fclose(pid_fp);
    
    /* PID file exists but is invalid */
    fprintf(stderr, "Warning: Failed to read valid PID from file '%s'\n", pid_file_path);
    
    /* Invalid PID file should be removed */
    unlink(pid_file_path);
    return -1;
  }
  
  fclose(pid_fp);
  return pid;
}

/* Main function with improved initialization sequence */
int main(int argc, char** argv) {
  /* Variables for initialization */
  server_config_t* config = NULL;
  database_t* database = NULL;
  rbac_system_t* rbac = NULL;
  rbac_refcount_t* rbac_ref = NULL;
  api_context_t* api_ctx = NULL;
  init_status_t status;
  
  /* Initialize memory manager FIRST - before anything else */
  memory_manager_init();
  
  /* Display banner */
  display_banner();
  
  /* DO NOT register cleanup handler here - wait until after daemon initialization */
  
  /* Initialize configuration */
  status = init_config(argc, argv, &config);
  if (status != INIT_OK) {
    /* Handle special cases */
    if (status == INIT_SHOW_HELP) {
      print_usage(argv[0]);
      return 0;  /* Exit successfully after showing help */
    }
    
    if (status == INIT_SHOW_VERSION) {
      print_version();
      return 0;  /* Exit successfully after showing version */
    }
    
    /* Handle terminate mode */
    if (config && status == INIT_ERROR) {
      pid_t pid = read_pid_file();
      if (pid > 0) {
        printf("Terminating server with PID %d...\n", pid);
        if (kill(pid, SIGTERM) == 0) {
          printf("Server terminated successfully\n");
          BUFFER_FREE(config);
          return 0;
        } else {
          fprintf(stderr, "Error: Failed to terminate server (errno=%d: %s)\n", 
              errno, strerror(errno));
          BUFFER_FREE(config);
          return 1;
        }
      } else {
        fprintf(stderr, "Error: No running server found\n");
        BUFFER_FREE(config);
        return 1;
      }
    }
    
    /* For other errors, show help */
    print_usage(argv[0]);
    return 1;
  }
  
  /* Display configuration information */
  printf("Binary directory: %s\n", config_get_binary_dir());
  printf("Log level: %s\n", config->log_level == LOG_LEVEL_ERROR ? "error" :
              config->log_level == LOG_LEVEL_WARNING ? "warning" :
              config->log_level == LOG_LEVEL_INFO ? "info" :
              config->log_level == LOG_LEVEL_DEBUG ? "debug" :
              config->log_level == LOG_LEVEL_TRACE ? "trace" : "unknown");
  printf("Database file: %s\n", config->db_file ? config->db_file : "not set");
  printf("Web root: %s\n", config->web_root ? config->web_root : "not set");
  printf("PID file: %s\n", config->pid_file ? config->pid_file : "not set");
  printf("Log file: %s\n", config->log_file ? config->log_file : "not set");
  printf("Server host: %s\n", config->host ? config->host : "0.0.0.0");
  printf("Server port: %d\n", config->port);
  printf("SSL enabled: %s\n", config->use_ssl ? "yes" : "no");
  if (config->use_ssl) {
    printf("SSL certificate: %s\n", config->cert_path ? config->cert_path : "not set");
    printf("SSL private key: %s\n", config->key_path ? config->key_path : "not set");
  }
  printf("Foreground mode: %s\n", config->verbose_mode ? "yes" : "no");
  
  /* Initialize logger */
  status = init_logger(config);
  if (status != INIT_OK) {
    fprintf(stderr, "Failed to initialize logger\n");
    BUFFER_FREE(config);
    return 1;
  }
  
  /* Special handling for JavaScript file execution - removed js_mode/js_file as this is 
    handled by the general initialization sequence now */
  
  /* CHANGED INITIALIZATION SEQUENCE: 
   * Moving database, RBAC and API initialization AFTER daemon and socket initialization
   * to ensure everything happens in the final process context
   */
  LOG_DEBUG("Using improved initialization sequence.");
  
  /* Database, RBAC, and API will be initialized after socket setup */
  database = NULL;
  rbac = NULL;
  rbac_ref = NULL;
  api_ctx = NULL;
  
  /*
   * CRITICAL SEQUENCE FOR SOCKET BINDING ISSUE:
   * 1. First daemonize (if in daemon mode)
   * 2. Then create and bind socket in the FINAL daemon process
   * 3. Finally initialize thread pool and run server
   */
  
  LOG_DEBUG("Initializing Writing PID if FG mode.");
  /* Write PID file if in foreground mode */
  if (config->verbose_mode && config->pid_file) {
    /* Debug logging in verbose mode */
    if (g_logger && g_logger->log_level >= LOG_LEVEL_DEBUG) {
      LOG_DEBUG("Verbose mode enabled, PID: %d", getpid());
      
      char cwd[PATH_MAX];
      if (getcwd(cwd, sizeof(cwd)) != NULL) {
        LOG_DEBUG("Working directory: %s", cwd);
      }
    }
    
    FILE* pid_fp = fopen(config->pid_file, "w");
    if (pid_fp) {
      fprintf(pid_fp, "%d\n", getpid());
      fclose(pid_fp);
      
      if (g_logger) {
        LOG_INFO("PID file written: %s (PID: %d)", config->pid_file, getpid());
      } else {
        printf("PID file written: %s (PID: %d)\n", config->pid_file, getpid());
      }
    } else {
      if (g_logger) {
        LOG_ERROR("Failed to write PID file '%s': %s (errno=%d)", 
            config->pid_file, strerror(errno), errno);
      } else {
        fprintf(stderr, "Failed to write PID file '%s': %s\n", 
           config->pid_file, strerror(errno));
      }
    }
  }
  
  /* Initialize daemon process if in daemon mode */
  LOG_DEBUG("Initializing daemon process");
  if (!config->verbose_mode) {
    /* Using our enhanced daemon initialization with proper logging */
    if (g_logger && g_logger->log_level >= LOG_LEVEL_DEBUG) {
      LOG_DEBUG("Starting daemon mode initialization");
    }
    
    status = init_daemon(config);
    if (status == INIT_DAEMON_ERROR) {
      INIT_LOG_FAILURE("DAEMON", "Failed to initialize daemon process");
      BUFFER_FREE(config);
      return 1;
    } else if (status == INIT_DAEMON_PARENT_EXIT) {
      /* Parent process should exit without cleanup */
      INIT_LOG_PROGRESS("DAEMON", "Daemon started, parent process exiting");
      /* Free config before exit to avoid memory leak */
      BUFFER_FREE(config);
      return 0;
    }
    
    /* Child process continues here */
    INIT_LOG_SUCCESS("DAEMON", "Daemon process initialized, continuing with child process");
  }
  
  /* NOW register cleanup handler - we're in the final server process */
  init_register_cleanup();
  
  /* 🕵️ INSPECTOR CLOUSEAU'S DUAL-PHASE INITIALIZATION */
  /* Phase 1: Emergency disable exotic allocators before SSL initialization (only if enabled) */
  bool exotic_allocators_were_enabled = false;
  if (config->use_ssl && memory_allocator_config_exotic_enabled()) {
    LOG_DEBUG("SSL enabled - temporarily disabling exotic allocators for SSL context creation");
    memory_allocator_emergency_disable();
    exotic_allocators_were_enabled = true;
  }
  
  /* Initialize socket - AFTER daemon process is fully established */
  LOG_DEBUG("Initializing Socket.");
  status = init_socket(config);
  if (status != INIT_OK) {
    INIT_LOG_FAILURE("MAIN", "Initialization failed");
    BUFFER_FREE(config);
    return 1;
  }
  
  /* 🚀 REVOLUTIONARY SSL SEMANTIC ALLOCATOR ACTIVATION */
  /* Phase 2: Enable SSL semantic allocator instead of blanket disable */
  if (config->use_ssl && exotic_allocators_were_enabled) {
    LOG_INFO("SSL enabled - activating revolutionary SSL semantic allocator");
    LOG_INFO("SSL operations will use optimized semantic memory management");
    memory_allocator_emergency_enable(); /* Re-enable for SSL semantic allocator */
  }
  
  /* Initialize production configuration before database */
  const char* config_level = getenv("JDBX_CONFIG_LEVEL");
  if (!config_level) {
    config_level = "development";  /* Default to development */
  }
  LOG_DEBUG("Initializing production configuration: %s", config_level);
  production_config_init(config_level);
  
  /* 🕵️ INSPECTOR CLOUSEAU'S DATABASE DUAL-PHASE INITIALIZATION */
  /* Phase 1: Emergency disable exotic allocators before database initialization (only if enabled) */
  bool database_exotic_allocators_were_enabled = false;
  if (memory_allocator_config_exotic_enabled()) {
    LOG_DEBUG("Database initialization - temporarily disabling exotic allocators for JDBX WAL memory operations");
    memory_allocator_emergency_disable();
    database_exotic_allocators_were_enabled = true;
  }
  
  /* Now that we have a socket and proper daemon context, initialize the database */
  LOG_DEBUG("Initializing Database.");
  status = init_database(config, &database);
  if (status != INIT_OK) {
    INIT_LOG_FAILURE("MAIN", "Initialization failed");
    BUFFER_FREE(config);
    return 1;
  }
  
  /* Phase 2: Re-enable exotic allocators after database initialization complete (only if SSL is disabled) */
  if (database_exotic_allocators_were_enabled && !config->use_ssl) {
    LOG_DEBUG("Database initialization complete - re-enabling exotic allocators (SSL disabled)");
    memory_allocator_enable(true, true);  /* Enable both Arena and TLSF */
  } else if (database_exotic_allocators_were_enabled && config->use_ssl) {
    LOG_WARNING("Database initialization complete - exotic allocators remain disabled due to SSL compatibility");
  }
  
  /* Apply database configuration (highest priority) */
  LOG_DEBUG("Applying database configuration settings.");
  if (config_apply_database_settings(config, database) != 0) {
    LOG_WARNING("No database configuration found or failed to apply - using defaults.");
  }
  
  /* Initialize rate limiter with database backend */
  LOG_DEBUG("Initializing rate limiter with database backend.");
  g_rate_limiter = rate_limiter_init(database);
  if (!g_rate_limiter) {
    LOG_ERROR("Failed to initialize rate limiter");
    INIT_LOG_FAILURE("MAIN", "Failed to initialize rate limiter");
    BUFFER_FREE(config);
    return 1;
  }
  
  /* Create default rate limiter configuration if it doesn't exist */
  rate_limiter_create_default_config(database);
  
  INIT_LOG_SUCCESS("RATE_LIMITER", "Rate limiter initialized with database backend");
  
  /* Enable thread-safe mode with comprehensive tracing */
  LOG_DEBUG("Enabling thread-safe connection management for enhanced stability.");
  server_enable_thread_safe_mode();
  
  /* Initialize thread pool before other initialization */
  LOG_DEBUG("Initializing Thread Pool.");
  status = init_threads(config);
  if (status != INIT_OK) {
    INIT_LOG_FAILURE("MAIN", "Failed to initialize thread pool");
    BUFFER_FREE(config);
    return 1;
  }
  
  /* Initialize thread-safe server components */
  LOG_DEBUG("Initializing thread-safe server components.");
  if (server_init_thread_safe(config) != 0) {
    INIT_LOG_FAILURE("MAIN", "Failed to initialize thread-safe server components");
    BUFFER_FREE(config);
    return 1;
  }
  
  /* Initialize metrics system */
  LOG_DEBUG("Initializing Metrics.");
  status = init_metrics(config);
  if (status != INIT_OK) {
    INIT_LOG_FAILURE("MAIN", "Failed to initialize metrics system");
    BUFFER_FREE(config);
    return 1;
  }
  
  /* Initialize RBAC AFTER thread pool and database are ready, but BEFORE API */
  LOG_DEBUG("Initializing RBAC.");
  status = init_rbac(config, database, &rbac, &rbac_ref);
  if (status != INIT_OK) {
    INIT_LOG_FAILURE("MAIN", "Failed to initialize RBAC system");
    BUFFER_FREE(config);
    return 1;
  }
  
  /* Initialize JWT cache for performance */
  LOG_DEBUG("Initializing JWT cache with %d max entries.", config->jwt_cache_max_entries);
  /* Configure JWT cache with server configuration values */
  jwt_cache_configure(config->jwt_cache_buckets, config->jwt_cache_ttl, config->jwt_cache_max_entries);
  
  if (jwt_cache_init(config->jwt_cache_max_entries) != 0) {
    INIT_LOG_FAILURE("MAIN", "Failed to initialize JWT cache");
    BUFFER_FREE(config);
    return 1;
  }
  LOG_DEBUG("JWT cache initialized successfully.");
  
  /* Initialize API context with properly initialized RBAC */
  LOG_DEBUG("Initializing API context.");
  status = init_api(config, database, rbac, &api_ctx);
  if (status != INIT_OK) {
    INIT_LOG_FAILURE("MAIN", "Failed to initialize API context");
    BUFFER_FREE(config);
    return 1;
  }
  
  /* Store API context in config for sharing with other components */
  config->api_ctx = api_ctx;
  
  /* Initialize persistence thread AFTER daemonization and all other components */
  LOG_DEBUG("Initializing Persistence Thread.");
  status = init_persistence_thread(database);
  if (status != INIT_OK) {
    LOG_WARNING("Cannot initialize persistence thread - continuing without automatic persistence.");
    /* This is not fatal - we can continue without automatic persistence */
  }
  
  LOG_DEBUG("All components initialized in the correct sequence.");
  
  /* Verify API context before running server */
  if (!config->api_ctx) {
    LOG_ERROR("API context is NULL before running server!");
  } else {
    LOG_INFO("API context is valid: %p", config->api_ctx);
  }
  
  LOG_INFO("About to call run_server with config=%p", config);
  
  /* Run server main loop */
  INIT_LOG_PROGRESS("MAIN", "All components initialized, starting server main loop");
  status = run_server(config);
  
  LOG_INFO("run_server returned with status: %d", status);
  
  /* The server has shut down */
  INIT_LOG_PROGRESS("MAIN", "Server has shut down, status: %d", status);
  
  /* Cleanup handled by registered atexit handler */
  
  /* Shutdown memory manager */
  memory_manager_shutdown();
  
  return (status == INIT_OK) ? 0 : 1;
}
