#ifndef CONFIG_DEFAULTS_H
#define CONFIG_DEFAULTS_H

/**
 * @file config_defaults.h
 * @brief Centralized default configuration values for JDBX
 * 
 * This file contains all default configuration values used throughout
 * the JDBX server. It serves as a single source of truth for defaults,
 * making configuration management more maintainable and consistent.
 */

#include "utils/logger.h"  /* For log level definitions */

/*==============================================================================
 * Server Configuration Defaults
 *============================================================================*/

/** Default server port */
#define DEFAULT_PORT 5000

/** Default host to bind to (0.0.0.0 for all interfaces) */
#define DEFAULT_HOST "0.0.0.0"

/** Default maximum number of simultaneous connections */
#define DEFAULT_MAX_CONNECTIONS 1000

/** Default verbose mode (0 for normal, 1 for verbose) */
#define DEFAULT_VERBOSE_MODE 0

/*==============================================================================
 * Path Defaults (NO HARDCODED PATHS - Auto-detected from binary location)
 * 
 * All paths are dynamically determined based on:
 * 1. Environment variables (highest priority)
 * 2. Auto-detection from binary location 
 * 3. These relative path defaults (lowest priority)
 * 
 * Environment Variable Override System:
 * - JDBX_BASE_PATH: Base installation directory (auto-detected by default)
 * - JDBX_VAR_PATH: Variable data directory
 * - JDBX_WEB_ROOT: Web interface directory
 * - Individual overrides: JDBX_DB_FILE, JDBX_LOG_FILE, JDBX_PID_FILE, etc.
 *============================================================================*/

/** Relative path defaults (appended to auto-detected base path) */
#define DEFAULT_VAR_DIR_RELATIVE "build/var"
#define DEFAULT_WEB_DIR_RELATIVE "share/htdocs"
#define DEFAULT_CONFIG_DIR_RELATIVE "share/config"

/** Default file basenames (combined with var directory) */
#define DEFAULT_DB_FILE_BASENAME "jdbx"
#define DEFAULT_RBAC_FILE_BASENAME "rbac.json"
#define DEFAULT_PID_FILE_BASENAME "jdbxd.pid"
#define DEFAULT_LOG_FILE_BASENAME "jdbxd.log"

/** Standard configuration file names for auto-discovery */
#define DEFAULT_ENV_FILE_BASENAME "jdbx.env"

/** Directory-specific defaults (relative to var directory) */
#define DEFAULT_VALIDATORS_SUBDIR "validators"
#define DEFAULT_TRANSFORMS_SUBDIR "transforms"
#define DEFAULT_METRICS_SUBDIR "metrics"
#define DEFAULT_BACKUP_SUBDIR "backups"

/*==============================================================================
 * Security Defaults
 *============================================================================*/

/** Default JWT secret (SHOULD BE CHANGED in production) */
#define DEFAULT_JWT_SECRET "change-this-secret-in-production"

/*==============================================================================
 * Feature Defaults
 *============================================================================*/

/** Default logging level */
#define DEFAULT_LOG_LEVEL LOG_LEVEL_INFO

/** Default JavaScript engine enabled flag */
#define DEFAULT_JS_ENABLED 1

/** Default SSL enabled flag */
#define DEFAULT_SSL_ENABLED 1

/** Default SSL certificate file path */
#define DEFAULT_SSL_CERT_PATH "/etc/ssl/certs/server.pem"

/** Default SSL private key file path */
#define DEFAULT_SSL_KEY_PATH "/etc/ssl/private/server.key"

/*==============================================================================
 * Database Defaults
 *============================================================================*/

/** Legacy compatibility - will be dynamically constructed */
#define DEFAULT_DATABASE_DIR_RELATIVE DEFAULT_VAR_DIR_RELATIVE

/* JDBX is the ONLY storage backend - no selection needed */

/** Default memory map size */
#ifndef DEFAULT_MMAP_SIZE
#define DEFAULT_MMAP_SIZE (1024 * 1024 * 100)  /* 100 MB */
#endif

/** Default JDBX initial size */
#define DEFAULT_JDBX_SIZE (1024 * 1024 * 100)  /* 100 MB */

/*==============================================================================
 * Performance Defaults
 *============================================================================*/

/** Default cache enabled flag */
#define DEFAULT_CACHE_ENABLED 1

/** Default cache size */
#define DEFAULT_CACHE_SIZE (1024 * 1024 * 10)  /* 10 MB */

/** Default cache entry TTL in seconds */
#define DEFAULT_CACHE_TTL 300  /* 5 minutes */

/*==============================================================================
 * Thread Pool Defaults
 *============================================================================*/

/** Default minimum threads */
#define DEFAULT_THREAD_POOL_MIN 4

/** Default maximum threads */
#define DEFAULT_THREAD_POOL_MAX 16

/** Default thread pool queue size */
#define DEFAULT_THREAD_POOL_QUEUE_SIZE 1024

/** Default thread idle timeout in seconds */
#define DEFAULT_THREAD_POOL_IDLE_TIMEOUT 60

/*==============================================================================
 * Metrics Defaults
 *============================================================================*/

/** Default metrics enabled flag */
#define DEFAULT_METRICS_ENABLED 1

/** Default metrics retention (number of data points) */
#define DEFAULT_METRICS_RETENTION 15

/*==============================================================================
 * Adaptive Indexing Defaults
 *============================================================================*/

/** Default query threshold for indexing (number of queries) */
#define DEFAULT_INDEX_QUERY_THRESHOLD 10

/** Default time threshold for indexing (milliseconds) */
#define DEFAULT_INDEX_TIME_THRESHOLD 50

/** Default query threshold for system collections */
#define DEFAULT_INDEX_QUERY_THRESHOLD_SYSTEM 5

/** Default time threshold for system collections (milliseconds) */
#define DEFAULT_INDEX_TIME_THRESHOLD_SYSTEM 10

/** Default indexing startup delay (seconds) */
#define DEFAULT_INDEX_STARTUP_DELAY 30

/** Default indexing check interval (seconds) */
#define DEFAULT_INDEX_CHECK_INTERVAL 60

/*==============================================================================
 * Component-specific Defaults
 *============================================================================*/

/* Backup API defaults - will be dynamically constructed from base path */
#define DEFAULT_BACKUP_DIR_RELATIVE DEFAULT_BACKUP_SUBDIR
#define DEFAULT_BACKUP_RETENTION 10
#define DEFAULT_AUTO_BACKUP_INTERVAL_HOURS 24
#define MAX_FILENAME_LEN 256
#define MAX_TIMESTAMP_LEN 32
/* Increase path length to accommodate paths that can be up to PATH_MAX + filename */
#define MAX_BACKUP_PATH_LEN (PATH_MAX + MAX_FILENAME_LEN)

/* Database defaults */
#define DEFAULT_INDEX_BUCKETS 256
#define DEFAULT_COLLECTION_CAPACITY 64
#define DEFAULT_DOCUMENTS_CAPACITY 1024
#define DEFAULT_INDEX_CAPACITY 16
#define DEFAULT_LRU_CACHE_SIZE 1000

/* Transaction defaults */
#define DEFAULT_TRANSACTION_TIMEOUT 30  /* 30 seconds */
#define DEFAULT_MAX_RETRIES 3
#define DEFAULT_RETRY_DELAY 100  /* 100 milliseconds */

/* CORS defaults */
#define DEFAULT_CORS_ENABLED 0
#define DEFAULT_CORS_ALLOW_CREDENTIALS 0
#define DEFAULT_CORS_MAX_AGE 86400  /* 24 hours */

#endif /* CONFIG_DEFAULTS_H */