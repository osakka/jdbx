#include "init.h"
#include "utils/metrics.h"
#include <stdlib.h>

/* External global metrics registry */
extern struct metrics_registry* g_metrics_registry;

/* Standard metric pointers for easy access */
static metric_t* m_server_requests_total = NULL;
static metric_t* m_server_request_duration = NULL;
static metric_t* m_db_operations_total = NULL;
static metric_t* m_db_operation_duration = NULL;
static metric_t* m_db_read_operations_total = NULL;
static metric_t* m_db_write_operations_total = NULL;
static metric_t* m_active_connections = NULL;
static metric_t* m_api_errors_total = NULL;
static metric_t* m_cache_hits_total = NULL;
static metric_t* m_cache_misses_total = NULL;
static metric_t* m_cache_evictions_total = NULL;
static metric_t* m_cache_size_bytes = NULL;
static metric_t* m_system_memory_bytes = NULL;

/* Initialize metrics registry and standard metrics */
init_status_t init_metrics(server_config_t* config) {
  INIT_LOG_PROGRESS("METRICS", "Initializing metrics system");
  
  /* Create metrics registry */
  g_metrics_registry = metrics_registry_create();
  if (!g_metrics_registry) {
    INIT_LOG_FAILURE("METRICS", "Failed to create metrics registry");
    return INIT_ERROR;
  }
  
  /* Create standard server metrics */
  m_server_requests_total = metrics_create_counter(
    g_metrics_registry,
    "server_requests_total",
    "Total number of API requests received"
  );
  
  m_server_request_duration = metrics_create_timer(
    g_metrics_registry,
    "server_request_duration_seconds",
    "Time taken to process API requests"
  );
  
  /* Create database metrics */
  m_db_operations_total = metrics_create_counter(
    g_metrics_registry,
    "db_operations_total",
    "Total number of database operations"
  );
  
  m_db_operation_duration = metrics_create_timer(
    g_metrics_registry,
    "db_operation_duration_seconds",
    "Time taken to execute database operations"
  );
  
  m_db_read_operations_total = metrics_create_counter(
    g_metrics_registry,
    "db_read_operations_total",
    "Total number of database read operations"
  );
  
  m_db_write_operations_total = metrics_create_counter(
    g_metrics_registry,
    "db_write_operations_total",
    "Total number of database write operations"
  );
  
  /* Create connection metrics */
  m_active_connections = metrics_create_gauge(
    g_metrics_registry,
    "active_connections",
    "Current number of active connections"
  );
  
  /* Create error metrics */
  m_api_errors_total = metrics_create_counter(
    g_metrics_registry,
    "api_errors_total",
    "Number of API errors"
  );
  
  /* Create cache metrics */
  m_cache_hits_total = metrics_create_counter(
    g_metrics_registry,
    "cache_hits_total",
    "Number of cache hits"
  );
  
  m_cache_misses_total = metrics_create_counter(
    g_metrics_registry,
    "cache_misses_total",
    "Number of cache misses"
  );
  
  m_cache_evictions_total = metrics_create_counter(
    g_metrics_registry,
    "cache_evictions_total",
    "Number of cache evictions"
  );
  
  m_cache_size_bytes = metrics_create_gauge(
    g_metrics_registry,
    "cache_size_bytes",
    "Size of internal caches in bytes"
  );
  
  /* Create system metrics */
  m_system_memory_bytes = metrics_create_gauge(
    g_metrics_registry,
    "system_memory_bytes",
    "Memory usage by the server in bytes"
  );
  
  /* Enable auto-export if configured */
  if (config->metrics_export_path) {
    metrics_registry_enable_auto_export(
      g_metrics_registry,
      config->metrics_export_path,
      config->metrics_export_interval > 0 ? config->metrics_export_interval : 60
    );
    INIT_LOG_DEBUG("METRICS", "Enabled auto-export to %s every %d seconds",
           config->metrics_export_path, 
           config->metrics_export_interval > 0 ? config->metrics_export_interval : 60);
  }
  
  INIT_LOG_SUCCESS("METRICS", "Metrics system initialized");
  return INIT_OK;
}

/* Cleanup metrics registry */
void cleanup_metrics(void) {
  if (g_metrics_registry) {
    INIT_LOG_PROGRESS("METRICS", "Shutting down metrics system");
    metrics_registry_free(g_metrics_registry);
    g_metrics_registry = NULL;
  }
}

/* Get metric pointers for use in other modules */
metric_t* get_server_requests_metric(void) { return m_server_requests_total; }
metric_t* get_server_request_duration_metric(void) { return m_server_request_duration; }
metric_t* get_db_operations_metric(void) { return m_db_operations_total; }
metric_t* get_db_operation_duration_metric(void) { return m_db_operation_duration; }
metric_t* get_db_read_operations_metric(void) { return m_db_read_operations_total; }
metric_t* get_db_write_operations_metric(void) { return m_db_write_operations_total; }
metric_t* get_active_connections_metric(void) { return m_active_connections; }
metric_t* get_api_errors_metric(void) { return m_api_errors_total; }
metric_t* get_cache_hits_metric(void) { return m_cache_hits_total; }
metric_t* get_cache_misses_metric(void) { return m_cache_misses_total; }
metric_t* get_cache_evictions_metric(void) { return m_cache_evictions_total; }
metric_t* get_cache_size_metric(void) { return m_cache_size_bytes; }
metric_t* get_system_memory_metric(void) { return m_system_memory_bytes; }