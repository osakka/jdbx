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
  
  /* Store original values to free later */
  char* old_db_path = config->db_file;
  char* old_pid_file = config->pid_file;
  char* old_log_file = config->log_file;
  char* old_web_root = config->web_root;
  char* old_cert_path = config->cert_path;
  char* old_key_path = config->key_path;
  
  /* Normalize all paths */
  if (config->db_file) {
    config->db_file = ensure_absolute_path(config->db_file, base_dir);
  }
  
  
  if (config->pid_file) {
    config->pid_file = ensure_absolute_path(config->pid_file, base_dir);
  }
  
  if (config->log_file) {
    config->log_file = ensure_absolute_path(config->log_file, base_dir);
  }
  
  if (config->web_root) {
    config->web_root = ensure_absolute_path(config->web_root, base_dir);
  }
  
  if (config->cert_path) {
    config->cert_path = ensure_absolute_path(config->cert_path, base_dir);
  }
  
  if (config->key_path) {
    config->key_path = ensure_absolute_path(config->key_path, base_dir);
  }
  
  /* Free original strings */
  BUFFER_FREE(old_db_path);
  BUFFER_FREE(old_pid_file);
  BUFFER_FREE(old_log_file);
  BUFFER_FREE(old_web_root);
  BUFFER_FREE(old_cert_path);
  BUFFER_FREE(old_key_path);
  
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
  
  /* Normalize all paths to absolute */
  return normalize_config_paths(config, base_dir);
}