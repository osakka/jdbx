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
#include "utils/buffer_pool.h"

/* Function to load environment variables from a file */
static int load_env_file(const char* filename) {
  if (!filename) {
    return 0;
  }
  
  FILE* file = fopen(filename, "r");
  if (!file) {
    return 0;
  }
  
  char line[512];
  int line_num = 0;
  int success = 1;
  
  while (fgets(line, sizeof(line), file)) {
    line_num++;
    
    /* Skip empty lines and comments */
    char* trimmed = line;
    while (*trimmed && (*trimmed == ' ' || *trimmed == '\t')) trimmed++;
    if (*trimmed == '\0' || *trimmed == '\n' || *trimmed == '#') {
      continue;
    }
    
    /* Remove newline */
    char* newline = strchr(trimmed, '\n');
    if (newline) *newline = '\0';
    
    /* Find equals sign */
    char* equals = strchr(trimmed, '=');
    if (!equals) {
      fprintf(stderr, "Warning: Invalid env file line %d in '%s': %s\n", 
              line_num, filename, trimmed);
      continue;
    }
    
    /* Split into key and value */
    *equals = '\0';
    char* key = trimmed;
    char* value = equals + 1;
    
    /* Trim key */
    char* key_end = key + strlen(key) - 1;
    while (key_end > key && (*key_end == ' ' || *key_end == '\t')) {
      *key_end = '\0';
      key_end--;
    }
    
    /* Trim value and handle quotes */
    while (*value && (*value == ' ' || *value == '\t')) value++;
    if (*value == '"' || *value == '\'') {
      char quote = *value;
      value++;
      char* end_quote = strrchr(value, quote);
      if (end_quote) *end_quote = '\0';
    }
    
    /* Set environment variable (do not overwrite existing) */
    if (setenv(key, value, 0) != 0) {
      fprintf(stderr, "Warning: Failed to set environment variable '%s' from '%s'\n", 
              key, filename);
      success = 0;
    }
  }
  
  fclose(file);
  return success;
}

/* Function to parse command line arguments and initialize configuration */
init_status_t init_config(int argc, char** argv, server_config_t** config_out) {
  INIT_LOG_PROGRESS("CONFIG", "Initializing configuration");

  /* Variables for command line options */
  int foreground_mode = 0;
  int show_help = 0;
  int terminate_server = 0;
  int show_version = 0;
  char* log_level_str = NULL;
  char* trace_categories_str = NULL;
  char* db_file = NULL;
  char* pid_file = NULL;
  char* log_file = NULL;
  char* web_root = NULL;
  char* config_file = NULL;
  char* js_file = NULL;
  char* port_str = NULL;
  char* host_str = NULL;
  int use_ssl = -1;  /* -1 = not set, 0 = disabled, 1 = enabled */
  char* ssl_cert = NULL;
  char* ssl_key = NULL;
  char* jdbx_initial_size = NULL;
  char* jdbx_wal_size = NULL;
  char* env_file = NULL;
  char* db_extension = NULL;
  char* wal_extension = NULL;
  char* bootstrap_admin_user = NULL;
  char* bootstrap_admin_pass = NULL;
  char* admin_email = NULL;
  char* jwt_secret = NULL;
  
  /* Initialize binary directory for path resolution */
  config_init_binary_dir();
  
  /* Initialize the server config with defaults from centralized configuration */
  server_config_t* heap_config =BUFFER_ALLOC(sizeof(server_config_t));
  if (!heap_config) {
    INIT_LOG_FAILURE("CONFIG", "Out of memory");
    return INIT_CONFIG_ERROR;
  }
  memset(heap_config, 0, sizeof(server_config_t));
  
  /* Initialize with default values directly on heap-allocated config */
  /* This avoids issues with stack-allocated configs containing heap pointers */
  config_init_defaults(heap_config);
  
  /* Define long options - short flags reserved for essential operations only */
  static struct option long_options[] = {
    /* ESSENTIAL SHORT FLAGS ONLY */
    {"help",         no_argument,       0, 'h'},
    {"version",      no_argument,       0, 'v'},
    {"daemon",       no_argument,       0, 0},
    {"foreground",   no_argument,       0, 0},
    {"config",       required_argument, 0, 0},
    
    /* ALL OTHER OPTIONS USE LONG FLAGS ONLY */
    {"terminate",    no_argument,       0, 400},
    {"log-level",    required_argument, 0, 401},
    {"trace-categories", required_argument, 0, 402},
    {"db-file",      required_argument, 0, 403},
    {"rbac-file",    required_argument, 0, 404},
    {"pid-file",     required_argument, 0, 405}, 
    {"log-file",     required_argument, 0, 406},
    {"web-root",     required_argument, 0, 407},
    {"js-file",      required_argument, 0, 408},
    {"port",         required_argument, 0, 409},
    {"host",         required_argument, 0, 410},
    {"validators-dir", required_argument, 0, 411},
    {"transforms-dir", required_argument, 0, 412},
    {"metrics-dir",  required_argument, 0, 413},
    {"ssl",          no_argument,       0, 414},
    {"no-ssl",       no_argument,       0, 415},
    {"ssl-cert",     required_argument, 0, 416},
    {"ssl-key",      required_argument, 0, 417},
    {"ssl-ignore-unexpected-eof", no_argument, 0, 418},
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
    /* JDBX options */
    {"jdbx-initial-size", required_argument, 0, 315},
    {"jdbx-wal-size", required_argument, 0, 316},
    /* Environment file option */
    {"env-file", required_argument, 0, 317},
    /* File extension options */
    {"db-extension", required_argument, 0, 318},
    {"wal-extension", required_argument, 0, 319},
    /* Security options */
    {"bootstrap-admin-user", required_argument, 0, 320},
    {"bootstrap-admin-pass", required_argument, 0, 321},
    {"admin-email", required_argument, 0, 322},
    {"jwt-secret", required_argument, 0, 323},
    /* Socket configuration options */
    {"socket-backlog", required_argument, 0, 330},
    {"socket-keepalive", no_argument, 0, 331},
    {"socket-reuseport", no_argument, 0, 332},
    /* SSL security options */
    {"ssl-verify-peer", no_argument, 0, 340},
    {"ssl-verify-depth", required_argument, 0, 341},
    {"ssl-session-timeout", required_argument, 0, 342},
    /* JWT cache options */
    {"jwt-cache-buckets", required_argument, 0, 350},
    {"jwt-cache-ttl", required_argument, 0, 351},
    {"jwt-cache-max-entries", required_argument, 0, 352},
    /* Password policy options */
    {"min-password-length", required_argument, 0, 360},
    {"max-login-attempts", required_argument, 0, 361},
    {"login-lockout-time", required_argument, 0, 362},
    {0, 0, 0, 0}
  };
  
  /* Parse command line options */
  int opt;
  int option_index = 0;
  
  optind = 1; /* Reset getopt index */
  while ((opt = getopt_long(argc, argv, "hv", long_options, &option_index)) != -1) {
    switch (opt) {
      case 'h':
        show_help = 1;
        break;
      case 0:
        /* Long options with no short equivalent */
        if (strcmp(long_options[option_index].name, "daemon") == 0) {
          /* Daemon mode - handle in run_daemon check later */
        } else if (strcmp(long_options[option_index].name, "foreground") == 0) {
          foreground_mode = 1;
        } else if (strcmp(long_options[option_index].name, "config") == 0) {
          config_file = optarg;
        }
        break;
      case 'v':
        show_version = 1;
        break;
      case 400: /* --terminate */
        terminate_server = 1;
        break;
      case 401: /* --log-level */
        log_level_str = optarg;
        break;
      case 402: /* --trace-categories */
        trace_categories_str = optarg;
        break;
      case 403: /* --db-file */
        db_file = optarg;
        break;
      case 405: /* --pid-file */
        pid_file = optarg;
        break;
      case 406: /* --log-file */
        log_file = optarg;
        break;
      case 407: /* --web-root */
        web_root = optarg;
        break;
      case 408: /* --js-file */
        js_file = optarg;
        break;
      case 409: /* --port */
        port_str = optarg;
        break;
      case 410: /* --host */
        host_str = optarg;
        break;
      case 414: /* --ssl */
        use_ssl = 1;
        break;
      case 415: /* --no-ssl */
        use_ssl = 0;
        break;
      case 416: /* --ssl-cert */
        ssl_cert = optarg;
        break;
      case 417: /* --ssl-key */
        ssl_key = optarg;
        break;
      case 418: /* --ssl-ignore-unexpected-eof */
        heap_config->ssl_ignore_unexpected_eof = 1;
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
      case 315:
        jdbx_initial_size = optarg;
        break;
      case 316:
        jdbx_wal_size = optarg;
        break;
      case 317:
        env_file = optarg;
        break;
      case 318: /* --db-extension */
        db_extension = optarg;
        break;
      case 319: /* --wal-extension */
        wal_extension = optarg;
        break;
      case 320: /* --bootstrap-admin-user */
        bootstrap_admin_user = optarg;
        break;
      case 321: /* --bootstrap-admin-pass */
        bootstrap_admin_pass = optarg;
        break;
      case 322: /* --admin-email */
        admin_email = optarg;
        break;
      case 323: /* --jwt-secret */
        jwt_secret = optarg;
        break;
      /* Socket configuration options */
      case 330: /* --socket-backlog */
        heap_config->socket_backlog = atoi(optarg);
        break;
      case 331: /* --socket-keepalive */
        heap_config->socket_keepalive = 1;
        break;
      case 332: /* --socket-reuseport */
        heap_config->socket_reuseport = 1;
        break;
      /* SSL security options */
      case 340: /* --ssl-verify-peer */
        heap_config->ssl_verify_peer = 1;
        break;
      case 341: /* --ssl-verify-depth */
        heap_config->ssl_verify_depth = atoi(optarg);
        break;
      case 342: /* --ssl-session-timeout */
        heap_config->ssl_session_timeout = atoi(optarg);
        break;
      /* JWT cache options */
      case 350: /* --jwt-cache-buckets */
        heap_config->jwt_cache_buckets = atoi(optarg);
        break;
      case 351: /* --jwt-cache-ttl */
        heap_config->jwt_cache_ttl = atoi(optarg);
        break;
      case 352: /* --jwt-cache-max-entries */
        heap_config->jwt_cache_max_entries = atoi(optarg);
        break;
      /* Password policy options */
      case 360: /* --min-password-length */
        heap_config->min_password_length = atoi(optarg);
        break;
      case 361: /* --max-login-attempts */
        heap_config->max_login_attempts = atoi(optarg);
        break;
      case 362: /* --login-lockout-time */
        heap_config->login_lockout_time = atoi(optarg);
        break;
      default:
        INIT_LOG_FAILURE("CONFIG", "Invalid command line option");
        BUFFER_FREE(heap_config);
        return INIT_CONFIG_ERROR;
    }
  }
  
  /* Remove verbose mode - use log levels instead */
  
  /* Show help if requested or if no arguments provided */
  if (show_help) {
    BUFFER_FREE(heap_config);
    /* Return special code to indicate help should be shown */
    return INIT_SHOW_HELP;
  }
  
  /* Show version if requested */
  if (show_version) {
    BUFFER_FREE(heap_config);
    /* Return special code to indicate version should be shown */
    return INIT_SHOW_VERSION;
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
      BUFFER_FREE(heap_config);
      return INIT_CONFIG_ERROR;
    }
    INIT_LOG_SUCCESS("CONFIG", "Configuration loaded from '%s'", config_file);
  }

  /* Load environment file if specified or auto-discover (lowest priority) */
  if (env_file) {
    INIT_LOG_PROGRESS("CONFIG", "Loading environment file '%s'", env_file);
    if (!load_env_file(env_file)) {
      INIT_LOG_FAILURE("CONFIG", "Failed to load environment file '%s'", env_file);
      BUFFER_FREE(heap_config);
      return INIT_CONFIG_ERROR;
    }
    INIT_LOG_SUCCESS("CONFIG", "Environment file loaded from '%s'", env_file);
  } else {
    /* Auto-discover environment files in priority order */
    char* var_env_path = NULL;
    char* config_env_path = NULL;
    
    /* Get dynamic paths */
    char* var_dir = config_get_var_dir();
    char* config_dir = config_construct_path(DEFAULT_CONFIG_DIR_RELATIVE);
    
    if (var_dir) {
      var_env_path =BUFFER_ALLOC(strlen(var_dir) + strlen(DEFAULT_ENV_FILE_BASENAME) + 2);
      if (var_env_path) {
        sprintf(var_env_path, "%s/%s", var_dir, DEFAULT_ENV_FILE_BASENAME);
      }
      BUFFER_FREE(var_dir);
    }
    
    if (config_dir) {
      config_env_path =BUFFER_ALLOC(strlen(config_dir) + strlen(DEFAULT_ENV_FILE_BASENAME) + 2);
      if (config_env_path) {
        sprintf(config_env_path, "%s/%s", config_dir, DEFAULT_ENV_FILE_BASENAME);
      }
      BUFFER_FREE(config_dir);
    }
    
    const char* env_files[] = {
      DEFAULT_ENV_FILE_BASENAME,                  /* Local directory override */
      var_env_path,                               /* Running configuration */
      config_env_path,                            /* Template defaults */
      NULL
    };
    
    for (int i = 0; env_files[i]; i++) {
      if (access(env_files[i], R_OK) == 0) {
        INIT_LOG_PROGRESS("CONFIG", "Auto-discovered environment file '%s'", env_files[i]);
        if (load_env_file(env_files[i])) {
          INIT_LOG_SUCCESS("CONFIG", "Environment file loaded from '%s'", env_files[i]);
          break;
        } else {
          INIT_LOG_WARNING("CONFIG", "Failed to load environment file '%s', trying next", env_files[i]);
        }
      }
    }
    
    /* Clean up allocated paths */
    if (var_env_path) BUFFER_FREE(var_env_path);
    if (config_env_path) BUFFER_FREE(config_env_path);
  }

  /* Load configuration from environment variables (lowest priority) */
  INIT_LOG_PROGRESS("CONFIG", "Loading settings from environment variables");
  load_environment_config(heap_config);
  
  /* Override configuration with command line arguments (medium priority) */
  /* Default to daemon mode unless foreground (-f) is explicitly set */
  heap_config->verbose_mode = 0;
  
  if (foreground_mode) {
    heap_config->verbose_mode = 1;
    INIT_LOG_PROGRESS("CONFIG", "Foreground mode enabled - daemon mode disabled");
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
  
  if (db_file) {
    /* Free previous value if allocated */
    if (heap_config->db_file) {
      BUFFER_FREE(heap_config->db_file);
    }
    heap_config->db_file = BUFFER_STRDUP(db_file);
    INIT_LOG_PROGRESS("CONFIG", "Database file set to %s", db_file);
  }
  
  
  if (web_root) {
    /* Free previous value if allocated */
    if (heap_config->web_root) {
      BUFFER_FREE(heap_config->web_root);
    }
    heap_config->web_root = BUFFER_STRDUP(web_root);
    INIT_LOG_PROGRESS("CONFIG", "Web root set to %s", web_root);
  }
  
  if (pid_file) {
    /* Free previous value if allocated */
    if (heap_config->pid_file) {
      BUFFER_FREE(heap_config->pid_file);
    }
    heap_config->pid_file = BUFFER_STRDUP(pid_file);
    INIT_LOG_PROGRESS("CONFIG", "PID file set to %s", pid_file);
  }
  
  if (log_file) {
    /* Free previous value if allocated */
    if (heap_config->log_file) {
      BUFFER_FREE(heap_config->log_file);
    }
    heap_config->log_file = BUFFER_STRDUP(log_file);
    INIT_LOG_PROGRESS("CONFIG", "Log file set to %s", log_file);
  }
  
  /* Process port argument */
  if (port_str) {
    int port = atoi(port_str);
    if (port <= 0 || port > 65535) {
      INIT_LOG_FAILURE("CONFIG", "Invalid port number '%s'", port_str);
      BUFFER_FREE(heap_config);
      return INIT_CONFIG_ERROR;
    }
    heap_config->port = port;
    INIT_LOG_PROGRESS("CONFIG", "Port set to %d", port);
  }
  
  /* Process host argument */
  if (host_str) {
    /* Free previous value if allocated */
    if (heap_config->host) {
      BUFFER_FREE(heap_config->host);
    }
    heap_config->host = BUFFER_STRDUP(host_str);
    INIT_LOG_PROGRESS("CONFIG", "Host set to %s", host_str);
  }
  
  /* Directory arguments removed - JS functions stored in database */
  
  /* JDBX is the only storage backend - no configuration needed */
  
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
      BUFFER_FREE(heap_config->cert_path);
    }
    heap_config->cert_path = BUFFER_STRDUP(ssl_cert);
    INIT_LOG_PROGRESS("CONFIG", "SSL certificate set to %s", ssl_cert);
  }
  
  if (ssl_key) {
    /* Free previous value if allocated */
    if (heap_config->key_path) {
      BUFFER_FREE(heap_config->key_path);
    }
    heap_config->key_path = BUFFER_STRDUP(ssl_key);
    INIT_LOG_PROGRESS("CONFIG", "SSL private key set to %s", ssl_key);
  }

  /* Process JavaScript file argument */
  if (js_file) {
    /* Store in js_enabled field to indicate JavaScript execution mode */
    heap_config->js_enabled = 1;
    
    /* Store JS file path in error field (reuse existing fields) */
    INIT_LOG_PROGRESS("CONFIG", "JavaScript file set to %s", js_file);
  }
  
  /* Process new configuration options */
  if (db_extension) {
    /* Note: DB extension would need implementation in config structure */
    INIT_LOG_PROGRESS("CONFIG", "Database extension set to %s", db_extension);
  }
  
  if (wal_extension) {
    /* Note: WAL extension would need implementation in config structure */
    INIT_LOG_PROGRESS("CONFIG", "WAL extension set to %s", wal_extension);
  }
  
  /* Process security configuration via environment variables */
  if (bootstrap_admin_user) {
    setenv("JDBX_BOOTSTRAP_ADMIN_USER", bootstrap_admin_user, 1);
    INIT_LOG_PROGRESS("CONFIG", "Bootstrap admin user set via CLI");
  }
  
  if (bootstrap_admin_pass) {
    setenv("JDBX_BOOTSTRAP_ADMIN_PASS", bootstrap_admin_pass, 1);
    INIT_LOG_PROGRESS("CONFIG", "Bootstrap admin password set via CLI");
  }
  
  if (admin_email) {
    setenv("JDBX_DEFAULT_ADMIN_EMAIL", admin_email, 1);
    INIT_LOG_PROGRESS("CONFIG", "Admin email set via CLI");
  }
  
  if (jwt_secret) {
    if (heap_config->jwt_secret) {
      BUFFER_FREE(heap_config->jwt_secret);
    }
    heap_config->jwt_secret = BUFFER_STRDUP(jwt_secret);
    INIT_LOG_PROGRESS("CONFIG", "JWT secret set via CLI (highest priority)");
  }
  
  /* Create required directories */
  init_status_t status = create_required_directories(heap_config);
  if (status != INIT_OK) {
    BUFFER_FREE(heap_config);
    return status;
  }

  /* Normalize paths to absolute paths */
  INIT_LOG_PROGRESS("CONFIG", "Normalizing paths to absolute");
  if (!normalize_config_paths(heap_config, config_get_binary_dir())) {
    INIT_LOG_FAILURE("CONFIG", "Failed to normalize configuration paths");
    BUFFER_FREE(heap_config);
    return INIT_CONFIG_ERROR;
  }
  
  /* Log all configuration values for debugging */
  INIT_LOG_DEBUG("CONFIG", "Final configuration settings:");
  INIT_LOG_DEBUG("CONFIG", " Port: %d", heap_config->port);
  INIT_LOG_DEBUG("CONFIG", " Host: %s", heap_config->host ? heap_config->host : "(null).");
  INIT_LOG_DEBUG("CONFIG", " Database file: %s", heap_config->db_file ? heap_config->db_file : "(null).");
  INIT_LOG_DEBUG("CONFIG", " PID file: %s", heap_config->pid_file ? heap_config->pid_file : "(null).");
  INIT_LOG_DEBUG("CONFIG", " Log file: %s", heap_config->log_file ? heap_config->log_file : "(null).");
  INIT_LOG_DEBUG("CONFIG", " Web root: %s", heap_config->web_root ? heap_config->web_root : "(null).");
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
  /* Admin cookie configuration */
  INIT_LOG_DEBUG("CONFIG", " Admin cookie name: %s", heap_config->admin_cookie_name ? heap_config->admin_cookie_name : "(null)");
  INIT_LOG_DEBUG("CONFIG", " Admin cookie TTL: %d", heap_config->admin_cookie_ttl);
  INIT_LOG_DEBUG("CONFIG", " Admin cookie secure: %s", heap_config->admin_cookie_secure ? "yes" : "no");
  INIT_LOG_DEBUG("CONFIG", " Admin cookie HTTP-only: %s", heap_config->admin_cookie_httponly ? "yes" : "no");
  INIT_LOG_DEBUG("CONFIG", " Admin cookie SameSite: %s", heap_config->admin_cookie_samesite ? heap_config->admin_cookie_samesite : "(null)");
  /* Persistence configuration */
  INIT_LOG_DEBUG("CONFIG", " Persistence ops threshold: %d", heap_config->persistence_ops_threshold);
  INIT_LOG_DEBUG("CONFIG", " Persistence size threshold: %zu bytes", heap_config->persistence_size_threshold);
  INIT_LOG_DEBUG("CONFIG", " Persistence save interval: %d seconds", heap_config->persistence_save_interval);
  /* Input validation limits */
  INIT_LOG_DEBUG("CONFIG", " Max collection name length: %zu", heap_config->max_collection_name_length);
  INIT_LOG_DEBUG("CONFIG", " Max document ID length: %zu", heap_config->max_document_id_length);
  INIT_LOG_DEBUG("CONFIG", " Max path length: %zu", heap_config->max_path_length);
  INIT_LOG_DEBUG("CONFIG", " Max URL length: %zu", heap_config->max_url_length);
  INIT_LOG_DEBUG("CONFIG", " Max email length: %zu", heap_config->max_email_length);
  /* Advanced indexing and performance configuration */
  INIT_LOG_DEBUG("CONFIG", " Query tracker max patterns: %d", heap_config->query_tracker_max_patterns);
  INIT_LOG_DEBUG("CONFIG", " Adaptive index min documents: %d", heap_config->adaptive_index_min_documents);
  INIT_LOG_DEBUG("CONFIG", " Adaptive index max per collection: %d", heap_config->adaptive_index_max_per_collection);
  INIT_LOG_DEBUG("CONFIG", " Index cleanup min age hours: %d", heap_config->index_cleanup_min_age_hours);
  INIT_LOG_DEBUG("CONFIG", " Index cleanup min queries: %d", heap_config->index_cleanup_min_queries);
  INIT_LOG_DEBUG("CONFIG", " Index cleanup ROI threshold: %.2f", heap_config->index_cleanup_roi_threshold);
  INIT_LOG_DEBUG("CONFIG", " Index cleanup effectiveness threshold: %.2f", heap_config->index_cleanup_effectiveness_threshold);
  INIT_LOG_DEBUG("CONFIG", " Index cleanup interval: %d seconds", heap_config->index_cleanup_interval);
  INIT_LOG_DEBUG("CONFIG", " Query tracker cleanup interval: %d seconds", heap_config->query_tracker_cleanup_interval);
  
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
  if (config->db_file) {
    char db_dir[PATH_MAX];
    strncpy(db_dir, config->db_file, PATH_MAX - 1);
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
