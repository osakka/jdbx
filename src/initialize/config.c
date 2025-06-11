#include "init.h"
#include "utils/config_loader.h"
#include "utils/config_defaults.h"
#include "utils/logger.h"
#include "utils/environment.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <libgen.h>
#include <errno.h>
#include <sys/stat.h>

/* Function to parse command line arguments and initialize configuration */
init_status_t init_config(int argc, char** argv, server_config_t** config_out) {
  INIT_LOG_PROGRESS("CONFIG", "Initializing configuration");

  /* Variables for command line options */
  int verbose_mode = 0;
  int show_help = 0;
  int terminate_server = 0;
  int show_version = 0;
  char* log_level_str = NULL;
  char* trace_categories_str = NULL;
  char* db_dir = NULL;
  char* pid_file = NULL;
  char* log_file = NULL;
  char* rbac_file = NULL;
  char* web_root = NULL;
  char* config_file = NULL;
  char* js_file = NULL;
  char* port_str = NULL;
  char* host_str = NULL;
  char* validators_dir = NULL;
  char* transforms_dir = NULL;
  char* metrics_dir = NULL;
  int use_ssl = -1;  /* -1 = not set, 0 = disabled, 1 = enabled */
  char* ssl_cert = NULL;
  char* ssl_key = NULL;
  char* storage_backend = NULL;
  char* jdbx_initial_size = NULL;
  char* jdbx_wal_size = NULL;
  
  /* Initialize binary directory for path resolution */
  config_init_binary_dir();
  
  /* Initialize the server config with defaults from centralized configuration */
  server_config_t* heap_config = malloc(sizeof(server_config_t));
  if (!heap_config) {
    INIT_LOG_FAILURE("CONFIG", "Out of memory");
    return INIT_CONFIG_ERROR;
  }
  memset(heap_config, 0, sizeof(server_config_t));
  
  /* Initialize with default values */
  server_config_t local_config = {0};
  config_init_defaults(&local_config);
  
  /* Copy to heap allocated config */
  memcpy(heap_config, &local_config, sizeof(server_config_t));
  
  /* Define long options */
  static struct option long_options[] = {
    {"help",      no_argument,    0, 'h'},
    {"daemon",     no_argument,    0, 'd'},
    {"terminate",   no_argument,    0, 't'},
    {"log-level",   required_argument, 0, 'l'},
    {"trace-categories", required_argument, 0, 'x'},
    {"db-dir",     required_argument, 0, 'b'},
    {"rbac-file",   required_argument, 0, 'r'},
    {"pid-file",    required_argument, 0, 'i'}, 
    {"log-file",    required_argument, 0, 'o'},
    {"web-root",    required_argument, 0, 'w'},
    {"config",     required_argument, 0, 'c'},
    {"version",    no_argument,    0, 'v'},
    {"verbose",    no_argument,    0, 'V'},
    {"js-file",    required_argument, 0, 'j'},
    {"port",      required_argument, 0, 'p'},
    {"host",      required_argument, 0, 'H'},
    {"validators-dir", required_argument, 0, 'Q'},
    {"transforms-dir", required_argument, 0, 'T'},
    {"metrics-dir",  required_argument, 0, 'M'},
    {"ssl",      no_argument,    0, 'S'},
    {"no-ssl",     no_argument,    0, 'N'},
    {"ssl-cert",    required_argument, 0, 'C'},
    {"ssl-key",    required_argument, 0, 'K'},
    /* Thread pool options */
    {"thread-pool-min", required_argument, 0, 301},
    {"thread-pool-max", required_argument, 0, 302},
    {"thread-pool-queue-size", required_argument, 0, 303},
    {"thread-pool-idle-timeout", required_argument, 0, 304},
    /* Cache options */
    {"cache-enabled", required_argument, 0, 305},
    {"cache-max-size", required_argument, 0, 306},
    {"cache-ttl", required_argument, 0, 307},
    /* Metrics options */
    {"metrics-enabled", required_argument, 0, 308},
    {"metrics-retention", required_argument, 0, 309},
    /* Indexing options */
    {"index-query-threshold", required_argument, 0, 310},
    {"index-time-threshold", required_argument, 0, 311},
    {"index-startup-delay", required_argument, 0, 312},
    {"index-check-interval", required_argument, 0, 313},
    /* Storage backend options */
    {"storage-backend", required_argument, 0, 314},
    {"jdbx-initial-size", required_argument, 0, 315},
    {"jdbx-wal-size", required_argument, 0, 316},
    {0, 0, 0, 0}
  };
  
  /* Parse command line options */
  int opt;
  int option_index = 0;
  
  optind = 1; /* Reset getopt index */
  while ((opt = getopt_long(argc, argv, "hdtl:x:b:r:i:o:w:c:vVj:p:H:Q:T:M:SNC:K:", long_options, &option_index)) != -1) {
    switch (opt) {
      case 'h':
        show_help = 1;
        break;
      case 'd':
        /* Daemon mode - handle in run_daemon check later */
        break;
      case 'V':
        verbose_mode = 1;
        break;
      case 't':
        terminate_server = 1;
        break;
      case 'l':
        log_level_str = optarg;
        break;
      case 'x':
        trace_categories_str = optarg;
        break;
      case 'b':
        db_dir = optarg;
        break;
      case 'r':
        rbac_file = optarg;
        break;
      case 'i': 
        pid_file = optarg;
        break;
      case 'o':
        log_file = optarg;
        break;
      case 'w':
        web_root = optarg;
        break;
      case 'c':
        config_file = optarg;
        break;
      case 'v':
        show_version = 1;
        break;
      case 'j':
        js_file = optarg;
        break;
      case 'p':
        port_str = optarg;
        break;
      case 'H':
        host_str = optarg;
        break;
      case 'Q':
        validators_dir = optarg;
        break;
      case 'T':
        transforms_dir = optarg;
        break;
      case 'M':
        metrics_dir = optarg;
        break;
      case 'S':
        use_ssl = 1;
        break;
      case 'N':
        use_ssl = 0;
        break;
      case 'C':
        ssl_cert = optarg;
        break;
      case 'K':
        ssl_key = optarg;
        break;
      /* Thread pool options */
      case 301:
        heap_config->thread_pool_min = atoi(optarg);
        break;
      case 302:
        heap_config->thread_pool_max = atoi(optarg);
        break;
      case 303:
        heap_config->thread_pool_queue_size = atoi(optarg);
        break;
      case 304:
        heap_config->thread_pool_idle_timeout = atoi(optarg);
        break;
      /* Cache options */
      case 305:
        heap_config->cache_enabled = (strcmp(optarg, "true") == 0 || strcmp(optarg, "1") == 0);
        break;
      case 306:
        heap_config->cache_max_size = atol(optarg);
        break;
      case 307:
        heap_config->cache_ttl = atoi(optarg);
        break;
      /* Metrics options */
      case 308:
        heap_config->metrics_enabled = (strcmp(optarg, "true") == 0 || strcmp(optarg, "1") == 0);
        break;
      case 309:
        heap_config->metrics_retention = atoi(optarg);
        break;
      /* Indexing options */
      case 310:
        heap_config->index_query_threshold = atoi(optarg);
        break;
      case 311:
        heap_config->index_time_threshold = atoi(optarg);
        break;
      case 312:
        heap_config->index_startup_delay = atoi(optarg);
        break;
      case 313:
        heap_config->index_check_interval = atoi(optarg);
        break;
      case 314:
        storage_backend = optarg;
        break;
      case 315:
        jdbx_initial_size = optarg;
        break;
      case 316:
        jdbx_wal_size = optarg;
        break;
      default:
        INIT_LOG_FAILURE("CONFIG", "Invalid command line option");
        free(heap_config);
        return INIT_CONFIG_ERROR;
    }
  }
  
  /* Set global verbose mode flag */
  init_set_verbose(verbose_mode);
  
  /* Show help if requested or if no arguments provided */
  if (show_help) {
    free(heap_config);
    /* Return special code to indicate help should be shown */
    return INIT_CONFIG_ERROR;
  }
  
  /* Show version if requested */
  if (show_version) {
    free(heap_config);
    /* Return special code to indicate version should be shown */
    return INIT_CONFIG_ERROR;
  }
  
  /* Handle terminate command */
  if (terminate_server) {
    /* We'll handle this in main.c */
    heap_config->error = 1; /* Use error field for termination request */
    *config_out = heap_config;
    return INIT_OK;
  }
  
  /* Load configuration from file if specified */
  if (config_file) {
    INIT_LOG_PROGRESS("CONFIG", "Loading configuration from '%s'", config_file);
    if (!config_load(config_file, heap_config)) {
      INIT_LOG_FAILURE("CONFIG", "Failed to load configuration from '%s'", config_file);
      free(heap_config);
      return INIT_CONFIG_ERROR;
    }
    INIT_LOG_SUCCESS("CONFIG", "Configuration loaded from '%s'", config_file);
  }

  /* Load configuration from environment variables (lowest priority) */
  INIT_LOG_PROGRESS("CONFIG", "Loading settings from environment variables");
  load_environment_config(heap_config);
  
  /* Override configuration with command line arguments (medium priority) */
  /* Default to daemon mode (verbose_mode=0) unless verbose (-V) is explicitly set */
  heap_config->verbose_mode = 0;
  
  if (verbose_mode) {
    heap_config->verbose_mode = 1;
    INIT_LOG_PROGRESS("CONFIG", "Verbose mode enabled, daemon mode disabled");
  }
  
  if (log_level_str) {
    log_level_t level = parse_log_level(log_level_str);
    heap_config->log_level = level;
    INIT_LOG_PROGRESS("CONFIG", "Log level set to %s", log_level_str);
    
    /* Apply immediately to active logger if it exists */
    if (g_logger) {
      logger_set_level(level);
    }
  }
  
  if (trace_categories_str) {
    INIT_LOG_PROGRESS("CONFIG", "Trace categories set to %s", trace_categories_str);
    
    /* Apply immediately to active logger if it exists */
    if (g_logger) {
      trace_category_t mask = logger_parse_trace(trace_categories_str);
      logger_set_trace_mask(mask);
    }
  }
  
  if (db_dir) {
    /* Free previous value if allocated */
    if (heap_config->db_path) {
      free(heap_config->db_path);
    }
    heap_config->db_path = strdup(db_dir);
    INIT_LOG_PROGRESS("CONFIG", "Database path set to %s", db_dir);
  }
  
  if (rbac_file) {
    /* Free previous value if allocated */
    if (heap_config->rbac_path) {
      free(heap_config->rbac_path);
    }
    heap_config->rbac_path = strdup(rbac_file);
    INIT_LOG_PROGRESS("CONFIG", "RBAC file set to %s", rbac_file);
  }
  
  if (web_root) {
    /* Free previous value if allocated */
    if (heap_config->web_root) {
      free(heap_config->web_root);
    }
    heap_config->web_root = strdup(web_root);
    INIT_LOG_PROGRESS("CONFIG", "Web root set to %s", web_root);
  }
  
  if (pid_file) {
    /* Free previous value if allocated */
    if (heap_config->pid_file) {
      free(heap_config->pid_file);
    }
    heap_config->pid_file = strdup(pid_file);
    INIT_LOG_PROGRESS("CONFIG", "PID file set to %s", pid_file);
  }
  
  if (log_file) {
    /* Free previous value if allocated */
    if (heap_config->log_file) {
      free(heap_config->log_file);
    }
    heap_config->log_file = strdup(log_file);
    INIT_LOG_PROGRESS("CONFIG", "Log file set to %s", log_file);
  }
  
  /* Process port argument */
  if (port_str) {
    int port = atoi(port_str);
    if (port <= 0 || port > 65535) {
      INIT_LOG_FAILURE("CONFIG", "Invalid port number '%s'", port_str);
      free(heap_config);
      return INIT_CONFIG_ERROR;
    }
    heap_config->port = port;
    INIT_LOG_PROGRESS("CONFIG", "Port set to %d", port);
  }
  
  /* Process host argument */
  if (host_str) {
    /* Free previous value if allocated */
    if (heap_config->host) {
      free(heap_config->host);
    }
    heap_config->host = strdup(host_str);
    INIT_LOG_PROGRESS("CONFIG", "Host set to %s", host_str);
  }
  
  /* Process validators directory argument */
  if (validators_dir) {
    /* Free previous value if allocated */
    if (heap_config->validators_dir) {
      free(heap_config->validators_dir);
    }
    heap_config->validators_dir = strdup(validators_dir);
    INIT_LOG_PROGRESS("CONFIG", "Validators directory set to %s", validators_dir);
  }
  
  /* Process transforms directory argument */
  if (transforms_dir) {
    /* Free previous value if allocated */
    if (heap_config->transforms_dir) {
      free(heap_config->transforms_dir);
    }
    heap_config->transforms_dir = strdup(transforms_dir);
    INIT_LOG_PROGRESS("CONFIG", "Transforms directory set to %s", transforms_dir);
  }
  
  /* Process metrics directory argument */
  if (metrics_dir) {
    /* Free previous value if allocated */
    if (heap_config->metrics_dir) {
      free(heap_config->metrics_dir);
    }
    heap_config->metrics_dir = strdup(metrics_dir);
    INIT_LOG_PROGRESS("CONFIG", "Metrics directory set to %s", metrics_dir);
  }
  
  /* Process storage backend arguments */
  if (storage_backend) {
    /* Free previous value if allocated */
    if (heap_config->storage_backend) {
      free(heap_config->storage_backend);
    }
    heap_config->storage_backend = strdup(storage_backend);
    INIT_LOG_PROGRESS("CONFIG", "Storage backend set to %s", storage_backend);
  }
  
  if (jdbx_initial_size) {
    heap_config->jdbx_initial_size = (size_t)atoll(jdbx_initial_size);
    INIT_LOG_PROGRESS("CONFIG", "JDBX initial size set to %zu bytes", heap_config->jdbx_initial_size);
  }
  
  if (jdbx_wal_size) {
    heap_config->jdbx_wal_size = (size_t)atoll(jdbx_wal_size);
    INIT_LOG_PROGRESS("CONFIG", "JDBX WAL size set to %zu bytes", heap_config->jdbx_wal_size);
  }
  
  /* Process SSL arguments */
  if (use_ssl != -1) {
    heap_config->use_ssl = use_ssl;
    INIT_LOG_PROGRESS("CONFIG", "SSL %s via command line", use_ssl ? "enabled" : "disabled");
  }
  
  if (ssl_cert) {
    /* Free previous value if allocated */
    if (heap_config->cert_path) {
      free(heap_config->cert_path);
    }
    heap_config->cert_path = strdup(ssl_cert);
    INIT_LOG_PROGRESS("CONFIG", "SSL certificate set to %s", ssl_cert);
  }
  
  if (ssl_key) {
    /* Free previous value if allocated */
    if (heap_config->key_path) {
      free(heap_config->key_path);
    }
    heap_config->key_path = strdup(ssl_key);
    INIT_LOG_PROGRESS("CONFIG", "SSL private key set to %s", ssl_key);
  }

  /* Process JavaScript file argument */
  if (js_file) {
    /* Store in js_enabled field to indicate JavaScript execution mode */
    heap_config->js_enabled = 1;
    
    /* Store JS file path in error field (reuse existing fields) */
    INIT_LOG_PROGRESS("CONFIG", "JavaScript file set to %s", js_file);
  }
  
  /* Create required directories */
  init_status_t status = create_required_directories(heap_config);
  if (status != INIT_OK) {
    free(heap_config);
    return status;
  }

  /* Normalize paths to absolute paths */
  INIT_LOG_PROGRESS("CONFIG", "Normalizing paths to absolute");
  if (!normalize_config_paths(heap_config, config_get_binary_dir())) {
    INIT_LOG_FAILURE("CONFIG", "Failed to normalize configuration paths");
    free(heap_config);
    return INIT_CONFIG_ERROR;
  }
  
  /* Log all configuration values for debugging */
  INIT_LOG_DEBUG("CONFIG", "Final configuration settings:");
  INIT_LOG_DEBUG("CONFIG", " Port: %d", heap_config->port);
  INIT_LOG_DEBUG("CONFIG", " Host: %s", heap_config->host ? heap_config->host : "(null).");
  INIT_LOG_DEBUG("CONFIG", " Database path: %s", heap_config->db_path ? heap_config->db_path : "(null).");
  INIT_LOG_DEBUG("CONFIG", " RBAC file: %s", heap_config->rbac_path ? heap_config->rbac_path : "(null).");
  INIT_LOG_DEBUG("CONFIG", " PID file: %s", heap_config->pid_file ? heap_config->pid_file : "(null).");
  INIT_LOG_DEBUG("CONFIG", " Log file: %s", heap_config->log_file ? heap_config->log_file : "(null).");
  INIT_LOG_DEBUG("CONFIG", " Web root: %s", heap_config->web_root ? heap_config->web_root : "(null).");
  INIT_LOG_DEBUG("CONFIG", " Validators dir: %s", heap_config->validators_dir ? heap_config->validators_dir : "(null).");
  INIT_LOG_DEBUG("CONFIG", " Transforms dir: %s", heap_config->transforms_dir ? heap_config->transforms_dir : "(null).");
  INIT_LOG_DEBUG("CONFIG", " Metrics dir: %s", heap_config->metrics_dir ? heap_config->metrics_dir : "(null).");
  INIT_LOG_DEBUG("CONFIG", " SSL enabled: %s", heap_config->use_ssl ? "yes" : "no");
  INIT_LOG_DEBUG("CONFIG", " SSL certificate: %s", heap_config->cert_path ? heap_config->cert_path : "(null).");
  INIT_LOG_DEBUG("CONFIG", " SSL private key: %s", heap_config->key_path ? heap_config->key_path : "(null).");
  /* Thread pool configuration */
  INIT_LOG_DEBUG("CONFIG", " Thread pool min: %d", heap_config->thread_pool_min);
  INIT_LOG_DEBUG("CONFIG", " Thread pool max: %d", heap_config->thread_pool_max);
  INIT_LOG_DEBUG("CONFIG", " Thread pool queue size: %d", heap_config->thread_pool_queue_size);
  INIT_LOG_DEBUG("CONFIG", " Thread pool idle timeout: %d", heap_config->thread_pool_idle_timeout);
  /* Cache configuration */
  INIT_LOG_DEBUG("CONFIG", " Cache enabled: %s", heap_config->cache_enabled ? "yes" : "no");
  INIT_LOG_DEBUG("CONFIG", " Cache max size: %zu", heap_config->cache_max_size);
  INIT_LOG_DEBUG("CONFIG", " Cache TTL: %d", heap_config->cache_ttl);
  /* Metrics configuration */
  INIT_LOG_DEBUG("CONFIG", " Metrics enabled: %s", heap_config->metrics_enabled ? "yes" : "no");
  INIT_LOG_DEBUG("CONFIG", " Metrics retention: %d", heap_config->metrics_retention);
  /* Indexing configuration */
  INIT_LOG_DEBUG("CONFIG", " Index query threshold: %d", heap_config->index_query_threshold);
  INIT_LOG_DEBUG("CONFIG", " Index time threshold: %d", heap_config->index_time_threshold);
  INIT_LOG_DEBUG("CONFIG", " Index startup delay: %d", heap_config->index_startup_delay);
  INIT_LOG_DEBUG("CONFIG", " Index check interval: %d", heap_config->index_check_interval);
  
  /* Set the output parameter */
  *config_out = heap_config;
  
  /* Register config with global system */
  init_register_config(heap_config);
  
  INIT_LOG_SUCCESS("CONFIG", "Configuration initialized");
  
  return INIT_OK;
}

/* Helper function to create required directories */
init_status_t create_required_directories(server_config_t* config) {
  /* Create directories for files that need them */
  if (config->pid_file) {
    char pid_dir[PATH_MAX];
    strncpy(pid_dir, config->pid_file, PATH_MAX - 1);
    pid_dir[PATH_MAX - 1] = '\0';
    
    char* dir = dirname(pid_dir);
    struct stat st = {0};
    
    if (stat(dir, &st) == -1) {
      INIT_LOG_PROGRESS("CONFIG", "Creating PID file directory: %s", dir);
      
      char cmd[PATH_MAX + 50];
      snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", dir);
      
      if (system(cmd) != 0) {
        INIT_LOG_FAILURE("CONFIG", "Failed to create PID file directory '%s': %s", 
            dir, strerror(errno));
        return INIT_CONFIG_ERROR;
      }
    }
  }
  
  /* Create log file directory if it doesn't exist */
  if (config->log_file) {
    char log_dir[PATH_MAX];
    strncpy(log_dir, config->log_file, PATH_MAX - 1);
    log_dir[PATH_MAX - 1] = '\0';
    
    char* dir = dirname(log_dir);
    struct stat st = {0};
    
    if (stat(dir, &st) == -1) {
      INIT_LOG_PROGRESS("CONFIG", "Creating log file directory: %s", dir);
      
      char cmd[PATH_MAX + 50];
      snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", dir);
      
      if (system(cmd) != 0) {
        INIT_LOG_FAILURE("CONFIG", "Failed to create log file directory '%s': %s", 
            dir, strerror(errno));
        return INIT_CONFIG_ERROR;
      }
    }
  }
  
  /* Create database directory if it doesn't exist */
  if (config->db_path) {
    char db_dir[PATH_MAX];
    strncpy(db_dir, config->db_path, PATH_MAX - 1);
    db_dir[PATH_MAX - 1] = '\0';
    
    char* dir = dirname(db_dir);
    struct stat st = {0};
    
    if (stat(dir, &st) == -1) {
      INIT_LOG_PROGRESS("CONFIG", "Creating database directory: %s", dir);
      
      char cmd[PATH_MAX + 50];
      snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", dir);
      
      if (system(cmd) != 0) {
        INIT_LOG_FAILURE("CONFIG", "Failed to create database directory '%s': %s", 
            dir, strerror(errno));
        return INIT_CONFIG_ERROR;
      }
    }
  }
  
  /* Create web root directory if it doesn't exist */
  if (config->web_root) {
    struct stat st = {0};
    
    if (stat(config->web_root, &st) == -1) {
      INIT_LOG_PROGRESS("CONFIG", "Creating web root directory: %s", config->web_root);
      
      char cmd[PATH_MAX + 50];
      snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", config->web_root);
      
      if (system(cmd) != 0) {
        INIT_LOG_FAILURE("CONFIG", "Failed to create web root directory '%s': %s", 
            config->web_root, strerror(errno));
        return INIT_CONFIG_ERROR;
      }
    }
  }
  
  return INIT_OK;
}

/* Helper function to parse log level */
log_level_t parse_log_level(const char* level_str) {
  if (!level_str) {
    return DEFAULT_LOG_LEVEL;
  }
  
  if (strcasecmp(level_str, "error") == 0) {
    return LOG_LEVEL_ERROR;
  } else if (strcasecmp(level_str, "warn") == 0 || strcasecmp(level_str, "warning") == 0) {
    return LOG_LEVEL_WARNING;
  } else if (strcasecmp(level_str, "info") == 0) {
    return LOG_LEVEL_INFO;
  } else if (strcasecmp(level_str, "debug") == 0) {
    return LOG_LEVEL_DEBUG;
  } else if (strcasecmp(level_str, "trace") == 0) {
    return LOG_LEVEL_TRACE;
  }
  
  LOG_WARNING("Unknown log level '%s', using default (info)", level_str);
  return DEFAULT_LOG_LEVEL;
}