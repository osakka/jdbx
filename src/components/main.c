#include "init.h"
#include "utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>

/* External global variables needed for cross-component integration */
extern server_config_t* g_server_config;

/* Display banner and help information */
static void display_banner(void) {
  printf("JSON Database Server\n");
  printf("===================\n\n");
}

/* Print usage information */
static void print_usage(const char* program_name) {
  printf("Usage: %s [OPTIONS]\n\n", program_name);
  printf("JSON Database Server - A lightweight JSON document database\n\n");
  printf("Options:\n");
  printf(" -h, --help          Display this help message and exit\n");
  printf(" -d, --daemon         Run as a daemon (background mode)\n");
  printf(" -t, --terminate        Terminate running server instance\n");
  printf(" -V, --verbose         Enable verbose logging\n");
  printf(" -l, --log-level=LEVEL     Set log level (error, warn, info, debug, trace)\n");
  printf(" -x, --trace-categories=CATS Set trace categories (database,rbac,api,auth,transaction,binary,javascript,network,metrics,memory,all)\n");
  printf(" -b, --db-dir=DIRECTORY    Set database directory\n");
  printf(" -r, --rbac-file=FILE     Set RBAC file path\n");
  printf(" -i, --pid-file=FILE      Set PID file path\n");
  printf(" -o, --log-file=FILE      Set log file path\n");
  printf(" -w, --web-root=DIRECTORY   Set web admin interface root directory\n");
  printf(" -p, --port=PORT        Set server port (default: 5000)\n");
  printf(" -H, --host=HOST        Set server bind address (default: 0.0.0.0)\n");
  printf(" -Q, --validators-dir=DIR   Set validators directory\n");
  printf(" -T, --transforms-dir=DIR   Set transforms directory\n");
  printf(" -M, --metrics-dir=DIR     Set metrics directory\n");
  printf(" -c, --config=FILE       Load configuration from file\n");
  printf(" -S, --ssl           Enable SSL/TLS encryption\n");
  printf(" -N, --no-ssl          Disable SSL/TLS encryption\n");
  printf(" -C, --ssl-cert=FILE      Set SSL certificate file path\n");
  printf(" -K, --ssl-key=FILE       Set SSL private key file path\n");
  printf(" -v, --version         Display version information and exit\n");
  printf(" -j, --js-file=FILE      Execute JavaScript file and exit\n");
  printf("\n");
}

/* Removed unused print_version function */

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
  
  /* Display banner */
  display_banner();
  
  /* DO NOT register cleanup handler here - wait until after daemon initialization */
  
  /* Initialize configuration */
  status = init_config(argc, argv, &config);
  if (status != INIT_OK) {
    /* Handle special cases for initialization with terminate mode requested */
    if (config && status == INIT_ERROR) {
      pid_t pid = read_pid_file();
      if (pid > 0) {
        printf("Terminating server with PID %d...\n", pid);
        if (kill(pid, SIGTERM) == 0) {
          printf("Server terminated successfully\n");
          free(config);
          return 0;
        } else {
          fprintf(stderr, "Error: Failed to terminate server (errno=%d: %s)\n", 
              errno, strerror(errno));
          free(config);
          return 1;
        }
      } else {
        fprintf(stderr, "Error: No running server found\n");
        free(config);
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
  printf("Database path: %s\n", config->db_path ? config->db_path : "not set");
  printf("RBAC file: %s\n", config->rbac_path ? config->rbac_path : "not set");
  printf("Web root: %s\n", config->web_root ? config->web_root : "not set");
  printf("PID file: %s\n", config->pid_file ? config->pid_file : "not set");
  printf("Log file: %s\n", config->log_file ? config->log_file : "not set");
  printf("Server host: %s\n", config->host ? config->host : "0.0.0.0");
  printf("Server port: %d\n", config->port);
  printf("Validators directory: %s\n", config->validators_dir ? config->validators_dir : "not set");
  printf("Transforms directory: %s\n", config->transforms_dir ? config->transforms_dir : "not set");
  printf("Metrics directory: %s\n", config->metrics_dir ? config->metrics_dir : "not set");
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
    free(config);
    return 1;
  }
  
  /* Special handling for JavaScript file execution - removed js_mode/js_file as this is 
    handled by the general initialization sequence now */
  
  /* CHANGED INITIALIZATION SEQUENCE: 
   * Moving database, RBAC and API initialization AFTER daemon and socket initialization
   * to ensure everything happens in the final process context
   */
  LOG_DEBUG("Using improved initialization sequence");
  
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
  
  LOG_DEBUG("Initializing Writing PID if FG mode");
  /* Write PID file if in foreground mode */
  if (config->verbose_mode && config->pid_file) {
    /* Debug logging in verbose mode */
    if (g_logger && g_logger->log_level >= LOG_LEVEL_DEBUG) {
      LOG_DEBUG("[DAEMON] Verbose mode enabled, PID: %d", getpid());
      
      char cwd[PATH_MAX];
      if (getcwd(cwd, sizeof(cwd)) != NULL) {
        LOG_DEBUG("[DAEMON] Working directory: %s", cwd);
      }
    }
    
    FILE* pid_fp = fopen(config->pid_file, "w");
    if (pid_fp) {
      fprintf(pid_fp, "%d\n", getpid());
      fclose(pid_fp);
      
      if (g_logger) {
        LOG_INFO("[DAEMON] PID file written: %s (PID: %d)", config->pid_file, getpid());
      } else {
        printf("[DEBUG] PID file written: %s (PID: %d)\n", config->pid_file, getpid());
      }
    } else {
      if (g_logger) {
        LOG_ERROR("[DAEMON] Failed to write PID file '%s': %s (errno=%d)", 
            config->pid_file, strerror(errno), errno);
      } else {
        fprintf(stderr, "[DEBUG] Failed to write PID file '%s': %s\n", 
           config->pid_file, strerror(errno));
      }
    }
  }
  
  /* Initialize daemon process if in daemon mode */
  LOG_DEBUG("Initializing Daemonization");
  if (!config->verbose_mode) {
    /* Using our enhanced daemon initialization with proper logging */
    if (g_logger && g_logger->log_level >= LOG_LEVEL_DEBUG) {
      LOG_DEBUG("[DAEMON] Starting daemon mode initialization");
    }
    
    status = init_daemon(config);
    if (status == INIT_DAEMON_ERROR) {
      INIT_LOG_FAILURE("DAEMON", "Failed to initialize daemon process");
      free(config);
      return 1;
    } else if (status == INIT_DAEMON_PARENT_EXIT) {
      /* Parent process should exit without cleanup */
      INIT_LOG_PROGRESS("DAEMON", "Daemon started, parent process exiting");
      /* Free config before exit to avoid memory leak */
      free(config);
      return 0;
    }
    
    /* Child process continues here */
    INIT_LOG_SUCCESS("DAEMON", "Daemon process initialized, continuing with child process");
  }
  
  /* NOW register cleanup handler - we're in the final server process */
  init_register_cleanup();
  
  /* Initialize socket - AFTER daemon process is fully established */
  LOG_DEBUG("Initializing Socket");
  status = init_socket(config);
  if (status != INIT_OK) {
    INIT_LOG_FAILURE("MAIN", "Initialization failed");
    free(config);
    return 1;
  }
  
  /* Now that we have a socket and proper daemon context, initialize the database */
  LOG_DEBUG("Initializing Database");
  status = init_database(config, &database);
  if (status != INIT_OK) {
    INIT_LOG_FAILURE("MAIN", "Initialization failed");
    free(config);
    return 1;
  }
  
  /* Initialize thread pool before other initialization */
  LOG_DEBUG("Initializing Thread Pool");
  status = init_threads(config);
  if (status != INIT_OK) {
    INIT_LOG_FAILURE("MAIN", "Failed to initialize thread pool");
    free(config);
    return 1;
  }
  
  /* Initialize metrics system */
  LOG_DEBUG("Initializing Metrics");
  status = init_metrics(config);
  if (status != INIT_OK) {
    INIT_LOG_FAILURE("MAIN", "Failed to initialize metrics system");
    free(config);
    return 1;
  }
  
  /* Initialize RBAC AFTER thread pool and database are ready, but BEFORE API */
  LOG_DEBUG("Initializing RBAC");
  status = init_rbac(config, database, &rbac, &rbac_ref);
  if (status != INIT_OK) {
    INIT_LOG_FAILURE("MAIN", "Failed to initialize RBAC system");
    free(config);
    return 1;
  }
  
  /* Initialize API context with properly initialized RBAC */
  LOG_DEBUG("Initializing API");
  status = init_api(config, database, rbac, &api_ctx);
  if (status != INIT_OK) {
    INIT_LOG_FAILURE("MAIN", "Failed to initialize API context");
    free(config);
    return 1;
  }
  
  /* Store API context in config for sharing with other components */
  config->api_ctx = api_ctx;
  
  /* Initialize persistence thread AFTER daemonization and all other components */
  LOG_DEBUG("Initializing Persistence Thread");
  status = init_persistence_thread(database);
  if (status != INIT_OK) {
    LOG_WARNING("Failed to initialize persistence thread - continuing without automatic persistence");
    /* This is not fatal - we can continue without automatic persistence */
  }
  
  LOG_INFO("All components initialized in the correct sequence");
  
  /* Run server main loop */
  INIT_LOG_PROGRESS("MAIN", "All components initialized, starting server main loop");
  status = run_server(config);
  
  /* The server has shut down */
  INIT_LOG_PROGRESS("MAIN", "Server has shut down, status: %d", status);
  
  /* Cleanup handled by registered atexit handler */
  
  return (status == INIT_OK) ? 0 : 1;
}
