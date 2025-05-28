#include "utils/environment.h"
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
    result = strdup(path);
    return result;
  }
  
  /* Get base directory for relative paths */
  char resolved_base[PATH_MAX];
  if (base_dir) {
    if (realpath(base_dir, resolved_base) == NULL) {
      /* fallback to current directory if base_dir is invalid */
      if (getcwd(resolved_base, sizeof(resolved_base)) == NULL) {
        return strdup(path); /* last resort: return original path */
      }
    }
  } else {
    /* Use current working directory as base */
    if (getcwd(resolved_base, sizeof(resolved_base)) == NULL) {
      return strdup(path); /* last resort: return original path */
    }
  }
  
  /* Combine base directory with relative path */
  size_t base_len = strlen(resolved_base);
  size_t path_len = strlen(path);
  
  /* Allocate space for combined path, extra slash, and null terminator */
  result = (char*)malloc(base_len + path_len + 2); 
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
  char* old_db_path = config->db_path;
  char* old_rbac_path = config->rbac_path;
  char* old_pid_file = config->pid_file;
  char* old_log_file = config->log_file;
  char* old_web_root = config->web_root;
  char* old_validators_dir = config->validators_dir;
  char* old_transforms_dir = config->transforms_dir;
  char* old_metrics_dir = config->metrics_dir;
  char* old_cert_path = config->cert_path;
  char* old_key_path = config->key_path;
  
  /* Normalize all paths */
  if (config->db_path) {
    config->db_path = ensure_absolute_path(config->db_path, base_dir);
  }
  
  if (config->rbac_path) {
    config->rbac_path = ensure_absolute_path(config->rbac_path, base_dir);
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
  
  if (config->validators_dir) {
    config->validators_dir = ensure_absolute_path(config->validators_dir, base_dir);
  }
  
  if (config->transforms_dir) {
    config->transforms_dir = ensure_absolute_path(config->transforms_dir, base_dir);
  }
  
  if (config->metrics_dir) {
    config->metrics_dir = ensure_absolute_path(config->metrics_dir, base_dir);
  }
  
  if (config->cert_path) {
    config->cert_path = ensure_absolute_path(config->cert_path, base_dir);
  }
  
  if (config->key_path) {
    config->key_path = ensure_absolute_path(config->key_path, base_dir);
  }
  
  /* Free original strings */
  free(old_db_path);
  free(old_rbac_path);
  free(old_pid_file);
  free(old_log_file);
  free(old_web_root);
  free(old_validators_dir);
  free(old_transforms_dir);
  free(old_metrics_dir);
  free(old_cert_path);
  free(old_key_path);
  
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
  const char* base_dir = getenv("JSONDB_BASE_DIR");
  
  /* Get paths from environment variables with defaults */
  const char* db_dir = getenv("JSONDB_DB_DIR");
  const char* rbac_file = getenv("JSONDB_RBAC_FILE");
  const char* log_file = getenv("JSONDB_LOG_FILE");
  const char* pid_file = getenv("JSONDB_PID_FILE");
  const char* web_root = getenv("JSONDB_WEB_ROOT");
  const char* validators_dir = getenv("JSONDB_VALIDATORS_DIR");
  const char* transforms_dir = getenv("JSONDB_TRANSFORMS_DIR");
  const char* metrics_dir = getenv("JSONDB_METRICS_DIR");
  const char* ssl_cert_file = getenv("JSONDB_SSL_CERT_FILE");
  const char* ssl_key_file = getenv("JSONDB_SSL_KEY_FILE");
  
  /* Update config with environment variables if they exist */
  if (db_dir) {
    free(config->db_path);
    config->db_path = strdup(db_dir);
  }
  
  if (rbac_file) {
    free(config->rbac_path);
    config->rbac_path = strdup(rbac_file);
  }
  
  if (log_file) {
    free(config->log_file);
    config->log_file = strdup(log_file);
  }
  
  if (pid_file) {
    free(config->pid_file);
    config->pid_file = strdup(pid_file);
  }
  
  if (web_root) {
    free(config->web_root);
    config->web_root = strdup(web_root);
  }
  
  if (validators_dir) {
    free(config->validators_dir);
    config->validators_dir = strdup(validators_dir);
  }
  
  if (transforms_dir) {
    free(config->transforms_dir);
    config->transforms_dir = strdup(transforms_dir);
  }
  
  if (metrics_dir) {
    free(config->metrics_dir);
    config->metrics_dir = strdup(metrics_dir);
  }
  
  if (ssl_cert_file) {
    free(config->cert_path);
    config->cert_path = strdup(ssl_cert_file);
  }
  
  if (ssl_key_file) {
    free(config->key_path);
    config->key_path = strdup(ssl_key_file);
  }
  
  /* Get numeric configuration from environment */
  const char* port_str = getenv("JSONDB_PORT");
  if (port_str) {
    config->port = atoi(port_str);
  }
  
  const char* host = getenv("JSONDB_HOST");
  if (host) {
    free(config->host);
    config->host = strdup(host);
  }
  
  const char* verbose_mode = getenv("JSONDB_VERBOSE");
  if (verbose_mode) {
    config->verbose_mode = (strcmp(verbose_mode, "true") == 0 || strcmp(verbose_mode, "1") == 0);
  }
  
  const char* log_level = getenv("JSONDB_LOG_LEVEL");
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
  
  const char* max_connections = getenv("JSONDB_MAX_CONNECTIONS");
  if (max_connections) {
    config->max_connections = atoi(max_connections);
  }
  
  const char* use_ssl = getenv("JSONDB_USE_SSL");
  if (use_ssl) {
    config->use_ssl = (strcmp(use_ssl, "true") == 0 || strcmp(use_ssl, "1") == 0);
  }
  
  /* Normalize all paths to absolute */
  return normalize_config_paths(config, base_dir);
}