#ifndef CONFIG_DEFAULTS_H
#define CONFIG_DEFAULTS_H

/**
 * @file config_defaults.h
 * @brief Centralized default configuration values for JSONdb
 * 
 * This file contains all default configuration values used throughout
 * the JSONdb server. It serves as a single source of truth for defaults,
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
 * Path Defaults (relative to install directory unless absolute)
 * 
 * These paths can be overridden using environment variables:
 * - JSONDB_BASE_PATH: Base installation directory (auto-detected by default)
 * - JSONDB_DOC_PATH: Documentation directory
 * - JSONDB_VAR_PATH: Variable data directory
 * - Individual path overrides (e.g., JSONDB_DB_DIR, JSONDB_LOG_FILE, etc.)
 *============================================================================*/

/** Default database file path */
#define DEFAULT_DB_PATH "var/data/jsondb/db.jdb"

/** Default RBAC config file path */
#define DEFAULT_RBAC_PATH "var/data/jsondb/rbac.json"

/** Default PID file path */
#define DEFAULT_PID_FILE "var/run/jsondb/jsondb_server.pid"

/** Default log file path */
#define DEFAULT_LOG_FILE "var/log/jsondb/server.log"

/** Default admin web interface root directory */
#define DEFAULT_WEB_ROOT "share/htdocs"

/** Admin files directory (maintained for compatibility) */
#define DEFAULT_ADMIN_FILES_DIR DEFAULT_WEB_ROOT

/** Default validators directory */
#define DEFAULT_VALIDATORS_DIR "var/validators"

/** Default transformers directory */
#define DEFAULT_TRANSFORMS_DIR "var/transforms"

/** Default metrics directory */
#define DEFAULT_METRICS_DIR "var/metrics"

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

/** Default database directory */
#define DEFAULT_DATABASE_DIR "/opt/jsondb/data"

/** Default storage backend (mmap or jdbx) */
#define DEFAULT_STORAGE_BACKEND "mmap"

/** Default memory map size */
#define DEFAULT_MMAP_SIZE (1024 * 1024 * 100)  /* 100 MB */

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

/* Backup API defaults */
#define DEFAULT_BACKUP_DIR "var/backups"
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