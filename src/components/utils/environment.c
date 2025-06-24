#include "utils/environment.h"
#include "utils/buffer_pool.h"
#include "utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>

/**
 * Ensure a path is absolute
 * 
 * If the path is already absolute, it is returned as-is.
 * If it's relative, it's converted to absolute based on base_dir.
 * If base_dir is NULL, current working directory is used.
 * If path is NULL, NULL is returned.
 * 
 * @param path Path to check/convert
 * @param base_dir Base directory for relative paths
 * @return Newly allocated absolute path (caller must free)
 */
char* ensure_absolute_path(const char* path, const char* base_dir) {
  if (!path) {
    return NULL;
  }
  
  char* result = NULL;
  
  /* Already absolute path */
  if (path[0] == '/') {
    result = BUFFER_STRDUP(path);
    return result;
  }
  
  /* Get base directory for relative paths */
  char resolved_base[PATH_MAX];
  if (base_dir) {
    if (realpath(base_dir, resolved_base) == NULL) {
      /* fallback to current directory if base_dir is invalid */
      if (getcwd(resolved_base, sizeof(resolved_base)) == NULL) {
        return BUFFER_STRDUP(path); /* last resort: return original path */
      }
    }
  } else {
    /* Use current working directory as base */
    if (getcwd(resolved_base, sizeof(resolved_base)) == NULL) {
      return BUFFER_STRDUP(path); /* last resort: return original path */
    }
  }
  
  /* Combine base directory with relative path */
  size_t base_len = strlen(resolved_base);
  size_t path_len = strlen(path);
  
  /* Allocate space for combined path, extra slash, and null terminator */
  result = (char*)BUFFER_ALLOC(base_len + path_len + 2); 
  if (!result) {
    return NULL; /* Memory allocation failed */
  }
  
  /* Copy base directory */
  strcpy(result, resolved_base);
  
  /* Add slash if needed */
  if (resolved_base[base_len - 1] != '/') {
    result[base_len] = '/';
    strcpy(result + base_len + 1, path);
  } else {
    strcpy(result + base_len, path);
  }
  
  return result;
}

/**
 * Normalize all paths in server configuration to absolute paths
 * 
 * @param config Server configuration to normalize
 * @param base_dir Base directory for relative paths
 * @return 1 on success, 0 on failure
 */
int normalize_config_paths(server_config_t* config, const char* base_dir) {
  if (!config) {
    return 0;
  }
  
  /* Debug logging temporarily disabled - SSL memory corruption fixed */
  
  /* Store original values to free later - DO NOT FREE UNTIL THE END */
  char* old_db_path = config->db_file;
  char* old_pid_file = config->pid_file;
  char* old_log_file = config->log_file;
  char* old_web_root = config->web_root;
  char* old_cert_path = config->cert_path;
  char* old_key_path = config->key_path;
  
  /* Create new normalized paths WITHOUT freeing old ones yet */
  char* new_db_path = NULL;
  char* new_pid_file = NULL;
  char* new_log_file = NULL;
  char* new_web_root = NULL;
  char* new_cert_path = NULL;
  char* new_key_path = NULL;
  
  /* Generate all new paths first */
  if (config->db_file) {
    new_db_path = ensure_absolute_path(config->db_file, base_dir);
  }
  if (config->pid_file) {
    new_pid_file = ensure_absolute_path(config->pid_file, base_dir);
  }
  if (config->log_file) {
    new_log_file = ensure_absolute_path(config->log_file, base_dir);
  }
  if (config->web_root) {
    new_web_root = ensure_absolute_path(config->web_root, base_dir);
  }
  if (config->cert_path) {
    new_cert_path = ensure_absolute_path(config->cert_path, base_dir);
  }
  if (config->key_path) {
    new_key_path = ensure_absolute_path(config->key_path, base_dir);
  }
  
  /* Now update all config fields atomically */
  if (new_db_path) {
    config->db_file = new_db_path;
  }
  if (new_pid_file) {
    config->pid_file = new_pid_file;
  }
  if (new_log_file) {
    config->log_file = new_log_file;
  }
  if (new_web_root) {
    config->web_root = new_web_root;
  }
  if (new_cert_path) {
    config->cert_path = new_cert_path;
  }
  if (new_key_path) {
    config->key_path = new_key_path;
  }
  
  /* Finally, free all old values */
  if (old_db_path) BUFFER_FREE(old_db_path);
  if (old_pid_file) BUFFER_FREE(old_pid_file);
  if (old_log_file) BUFFER_FREE(old_log_file);
  if (old_web_root) BUFFER_FREE(old_web_root);
  if (old_cert_path) BUFFER_FREE(old_cert_path);
  if (old_key_path) BUFFER_FREE(old_key_path);
  
  /* SSL memory corruption fixed - paths now correctly normalized */
  
  return 1;
}

/**
 * Load configuration from environment variables
 * 
 * @param config Server configuration to update
 * @return 1 on success, 0 on failure
 */
int load_environment_config(server_config_t* config) {
  if (!config) {
    return 0;
  }
  
  /* Get base paths */
  const char* base_dir = getenv("JDBX_BASE_PATH");
  if (!base_dir) {
    /* Try alternative name for backward compatibility */
    base_dir = getenv("JDBX_BASE_DIR");
  }
  
  /* Get additional path configuration */
//   const char* doc_path = getenv("JDBX_DOC_PATH");
//   const char* var_path = getenv("JDBX_VAR_PATH");
  
  /* Get paths from environment variables with defaults */
  const char* db_dir = getenv("JDBX_DB_DIR");
  const char* log_file = getenv("JDBX_LOG_FILE");
  const char* pid_file = getenv("JDBX_PID_FILE");
  const char* web_root = getenv("JDBX_WEB_ROOT");
  const char* ssl_cert_file = getenv("JDBX_SSL_CERT");
  const char* ssl_key_file = getenv("JDBX_SSL_KEY");
  
  /* Update config with environment variables if they exist */
  if (db_dir) {
    BUFFER_FREE(config->db_file);
    config->db_file = BUFFER_STRDUP(db_dir);
  }
  
  
  if (log_file) {
    BUFFER_FREE(config->log_file);
    config->log_file = BUFFER_STRDUP(log_file);
  }
  
  if (pid_file) {
    BUFFER_FREE(config->pid_file);
    config->pid_file = BUFFER_STRDUP(pid_file);
  }
  
  if (web_root) {
    BUFFER_FREE(config->web_root);
    config->web_root = BUFFER_STRDUP(web_root);
  }
  
  
  if (ssl_cert_file) {
    BUFFER_FREE(config->cert_path);
    config->cert_path = BUFFER_STRDUP(ssl_cert_file);
  }
  
  if (ssl_key_file) {
    BUFFER_FREE(config->key_path);
    config->key_path = BUFFER_STRDUP(ssl_key_file);
  }
  
  /* JDBX is the only storage backend - ignore env variable */
  
  /* Get numeric configuration from environment */
  const char* port_str = getenv("JDBX_PORT");
  if (port_str) {
    config->port = atoi(port_str);
  }
  
  const char* host = getenv("JDBX_HOST");
  if (host) {
    BUFFER_FREE(config->host);
    config->host = BUFFER_STRDUP(host);
  }
  
  const char* verbose_mode = getenv("JDBX_VERBOSE");
  if (verbose_mode) {
    config->verbose_mode = (strcmp(verbose_mode, "true") == 0 || strcmp(verbose_mode, "1") == 0);
  }
  
  const char* log_level = getenv("JDBX_LOG_LEVEL");
  if (log_level) {
    if (strcmp(log_level, "error") == 0) {
      config->log_level = LOG_LEVEL_ERROR;
    } else if (strcmp(log_level, "warning") == 0 || strcmp(log_level, "warn") == 0) {
      config->log_level = LOG_LEVEL_WARNING;
    } else if (strcmp(log_level, "info") == 0) {
      config->log_level = LOG_LEVEL_INFO;
    } else if (strcmp(log_level, "debug") == 0) {
      config->log_level = LOG_LEVEL_DEBUG;
    } else if (strcmp(log_level, "trace") == 0) {
      config->log_level = LOG_LEVEL_TRACE;
    }
  }
  
  const char* max_connections = getenv("JDBX_MAX_CONNECTIONS");
  if (max_connections) {
    config->max_connections = atoi(max_connections);
  }
  
  const char* use_ssl = getenv("JDBX_USE_SSL");
  if (use_ssl) {
    config->use_ssl = (strcmp(use_ssl, "true") == 0 || strcmp(use_ssl, "1") == 0);
  }
  
  const char* ssl_ignore_unexpected_eof = getenv("JDBX_SSL_IGNORE_UNEXPECTED_EOF");
  if (ssl_ignore_unexpected_eof) {
    config->ssl_ignore_unexpected_eof = (strcmp(ssl_ignore_unexpected_eof, "true") == 0 || strcmp(ssl_ignore_unexpected_eof, "1") == 0);
    if (g_logger) {
      LOG_INFO("Loaded JDBX_SSL_IGNORE_UNEXPECTED_EOF='%s' -> %d", ssl_ignore_unexpected_eof, config->ssl_ignore_unexpected_eof);
    } else {
      fprintf(stderr, "[ENV] Loaded JDBX_SSL_IGNORE_UNEXPECTED_EOF='%s' -> %d\n", ssl_ignore_unexpected_eof, config->ssl_ignore_unexpected_eof);
    }
  } else {
    if (g_logger) {
      LOG_INFO("JDBX_SSL_IGNORE_UNEXPECTED_EOF not set in environment");
    } else {
      fprintf(stderr, "[ENV] JDBX_SSL_IGNORE_UNEXPECTED_EOF not set in environment\n");
    }
  }
  
  /* Thread pool configuration */
  const char* thread_pool_min = getenv("JDBX_THREAD_POOL_MIN");
  if (thread_pool_min) {
    config->thread_pool_min = atoi(thread_pool_min);
  }
  
  const char* thread_pool_max = getenv("JDBX_THREAD_POOL_MAX");
  if (thread_pool_max) {
    config->thread_pool_max = atoi(thread_pool_max);
  }
  
  const char* thread_pool_queue_size = getenv("JDBX_THREAD_POOL_QUEUE_SIZE");
  if (thread_pool_queue_size) {
    config->thread_pool_queue_size = atoi(thread_pool_queue_size);
  }
  
  const char* thread_pool_idle_timeout = getenv("JDBX_THREAD_POOL_IDLE_TIMEOUT");
  if (thread_pool_idle_timeout) {
    config->thread_pool_idle_timeout = atoi(thread_pool_idle_timeout);
  }
  
  /* Cache configuration */
  const char* cache_enabled = getenv("JDBX_CACHE_ENABLED");
  if (cache_enabled) {
    config->cache_enabled = (strcmp(cache_enabled, "true") == 0 || strcmp(cache_enabled, "1") == 0);
  }
  
  const char* cache_max_size = getenv("JDBX_CACHE_MAX_SIZE");
  if (cache_max_size) {
    config->cache_max_size = atol(cache_max_size);
  }
  
  const char* cache_ttl = getenv("JDBX_CACHE_TTL");
  if (cache_ttl) {
    config->cache_ttl = atoi(cache_ttl);
  }
  
  /* Metrics configuration */
  const char* metrics_enabled = getenv("JDBX_METRICS_ENABLED");
  if (metrics_enabled) {
    config->metrics_enabled = (strcmp(metrics_enabled, "true") == 0 || strcmp(metrics_enabled, "1") == 0);
  }
  
  const char* metrics_retention = getenv("JDBX_METRICS_RETENTION");
  if (metrics_retention) {
    config->metrics_retention = atoi(metrics_retention);
  }
  
  /* Adaptive indexing configuration */
  const char* index_query_threshold = getenv("JDBX_INDEX_QUERY_THRESHOLD");
  if (index_query_threshold) {
    config->index_query_threshold = atoi(index_query_threshold);
  }
  
  const char* index_time_threshold = getenv("JDBX_INDEX_TIME_THRESHOLD");
  if (index_time_threshold) {
    config->index_time_threshold = atoi(index_time_threshold);
  }
  
  const char* index_query_threshold_system = getenv("JDBX_INDEX_QUERY_THRESHOLD_SYSTEM");
  if (index_query_threshold_system) {
    config->index_query_threshold_system = atoi(index_query_threshold_system);
  }
  
  const char* index_time_threshold_system = getenv("JDBX_INDEX_TIME_THRESHOLD_SYSTEM");
  if (index_time_threshold_system) {
    config->index_time_threshold_system = atoi(index_time_threshold_system);
  }
  
  const char* index_startup_delay = getenv("JDBX_INDEX_STARTUP_DELAY");
  if (index_startup_delay) {
    config->index_startup_delay = atoi(index_startup_delay);
  }
  
  const char* index_check_interval = getenv("JDBX_INDEX_CHECK_INTERVAL");
  if (index_check_interval) {
    config->index_check_interval = atoi(index_check_interval);
  }
  
  /* JWT secret configuration (environment variable support) */
  const char* jwt_secret = getenv("JDBX_JWT_SECRET");
  if (jwt_secret && strlen(jwt_secret) >= 16) {
    BUFFER_FREE(config->jwt_secret);
    config->jwt_secret = BUFFER_STRDUP(jwt_secret);
    LOG_INFO("JWT secret loaded from environment variable JDBX_JWT_SECRET");
    
    /* Security validation */
    if (strlen(jwt_secret) < 32) {
      LOG_WARNING("JWT secret from environment is shorter than recommended 32 characters");
    }
  }
  
  /* Admin cookie configuration */
  const char* admin_cookie_name = getenv("JDBX_ADMIN_COOKIE_NAME");
  if (admin_cookie_name) {
    BUFFER_FREE(config->admin_cookie_name);
    config->admin_cookie_name = BUFFER_STRDUP(admin_cookie_name);
  }
  
  const char* admin_cookie_ttl = getenv("JDBX_ADMIN_COOKIE_TTL");
  if (admin_cookie_ttl) {
    config->admin_cookie_ttl = atoi(admin_cookie_ttl);
  }
  
  const char* admin_cookie_secure = getenv("JDBX_ADMIN_COOKIE_SECURE");
  if (admin_cookie_secure) {
    config->admin_cookie_secure = (strcmp(admin_cookie_secure, "true") == 0 || strcmp(admin_cookie_secure, "1") == 0);
  }
  
  const char* admin_cookie_httponly = getenv("JDBX_ADMIN_COOKIE_HTTPONLY");
  if (admin_cookie_httponly) {
    config->admin_cookie_httponly = (strcmp(admin_cookie_httponly, "true") == 0 || strcmp(admin_cookie_httponly, "1") == 0);
  }
  
  const char* admin_cookie_samesite = getenv("JDBX_ADMIN_COOKIE_SAMESITE");
  if (admin_cookie_samesite) {
    BUFFER_FREE(config->admin_cookie_samesite);
    config->admin_cookie_samesite = BUFFER_STRDUP(admin_cookie_samesite);
  }

  /* Persistence configuration */
  const char* persistence_ops_threshold = getenv("JDBX_PERSISTENCE_OPS_THRESHOLD");
  if (persistence_ops_threshold) {
    config->persistence_ops_threshold = atoi(persistence_ops_threshold);
  }
  
  const char* persistence_size_threshold = getenv("JDBX_PERSISTENCE_SIZE_THRESHOLD");
  if (persistence_size_threshold) {
    config->persistence_size_threshold = (size_t)atoll(persistence_size_threshold);
  }
  
  const char* persistence_save_interval = getenv("JDBX_PERSISTENCE_SAVE_INTERVAL");
  if (persistence_save_interval) {
    config->persistence_save_interval = atoi(persistence_save_interval);
  }

  /* Input validation limits configuration */
  const char* max_collection_name_length = getenv("JDBX_MAX_COLLECTION_NAME_LENGTH");
  if (max_collection_name_length) {
    config->max_collection_name_length = (size_t)atoll(max_collection_name_length);
  }
  
  const char* max_document_id_length = getenv("JDBX_MAX_DOCUMENT_ID_LENGTH");
  if (max_document_id_length) {
    config->max_document_id_length = (size_t)atoll(max_document_id_length);
  }
  
  const char* max_path_length = getenv("JDBX_MAX_PATH_LENGTH");
  if (max_path_length) {
    config->max_path_length = (size_t)atoll(max_path_length);
  }
  
  const char* max_url_length = getenv("JDBX_MAX_URL_LENGTH");
  if (max_url_length) {
    config->max_url_length = (size_t)atoll(max_url_length);
  }
  
  const char* max_email_length = getenv("JDBX_MAX_EMAIL_LENGTH");
  if (max_email_length) {
    config->max_email_length = (size_t)atoll(max_email_length);
  }

  /* Advanced indexing and performance configuration */
  const char* query_tracker_max_patterns = getenv("JDBX_QUERY_TRACKER_MAX_PATTERNS");
  if (query_tracker_max_patterns) {
    config->query_tracker_max_patterns = atoi(query_tracker_max_patterns);
  }
  
  const char* adaptive_index_min_documents = getenv("JDBX_ADAPTIVE_INDEX_MIN_DOCUMENTS");
  if (adaptive_index_min_documents) {
    config->adaptive_index_min_documents = atoi(adaptive_index_min_documents);
  }
  
  const char* adaptive_index_max_per_collection = getenv("JDBX_ADAPTIVE_INDEX_MAX_PER_COLLECTION");
  if (adaptive_index_max_per_collection) {
    config->adaptive_index_max_per_collection = atoi(adaptive_index_max_per_collection);
  }
  
  const char* index_cleanup_min_age_hours = getenv("JDBX_INDEX_CLEANUP_MIN_AGE_HOURS");
  if (index_cleanup_min_age_hours) {
    config->index_cleanup_min_age_hours = atoi(index_cleanup_min_age_hours);
  }
  
  const char* index_cleanup_min_queries = getenv("JDBX_INDEX_CLEANUP_MIN_QUERIES");
  if (index_cleanup_min_queries) {
    config->index_cleanup_min_queries = atoi(index_cleanup_min_queries);
  }
  
  const char* index_cleanup_roi_threshold = getenv("JDBX_INDEX_CLEANUP_ROI_THRESHOLD");
  if (index_cleanup_roi_threshold) {
    config->index_cleanup_roi_threshold = atof(index_cleanup_roi_threshold);
  }
  
  const char* index_cleanup_effectiveness_threshold = getenv("JDBX_INDEX_CLEANUP_EFFECTIVENESS_THRESHOLD");
  if (index_cleanup_effectiveness_threshold) {
    config->index_cleanup_effectiveness_threshold = atof(index_cleanup_effectiveness_threshold);
  }
  
  const char* index_cleanup_interval = getenv("JDBX_INDEX_CLEANUP_INTERVAL");
  if (index_cleanup_interval) {
    config->index_cleanup_interval = atoi(index_cleanup_interval);
  }
  
  const char* query_tracker_cleanup_interval = getenv("JDBX_QUERY_TRACKER_CLEANUP_INTERVAL");
  if (query_tracker_cleanup_interval) {
    config->query_tracker_cleanup_interval = atoi(query_tracker_cleanup_interval);
  }

  /* Apply log configuration to active logger if it exists */
  if (g_logger) {
    const char* runtime_log_level = getenv("JDBX_LOG_LEVEL");
    if (runtime_log_level) {
      log_level_t level = logger_parse_level(runtime_log_level);
      logger_set_level(level);
    }
    
    const char* trace_categories = getenv("JDBX_TRACE_CATEGORIES");
    if (trace_categories) {
      trace_category_t mask = logger_parse_trace(trace_categories);
      logger_set_trace_mask(mask);
    }
  }
  
  /* Path normalization is handled by init_config() after all sources are loaded */
  return 1;
}