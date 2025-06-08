#ifndef METRICS_H
#define METRICS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sys/time.h>
#include <stdint.h>

/* Metric types */
typedef enum {
    METRIC_TYPE_COUNTER,  /* Incremental counter */
    METRIC_TYPE_GAUGE,    /* Value that can go up or down */
    METRIC_TYPE_TIMER,    /* Measure time taken */
    METRIC_TYPE_HISTOGRAM /* Frequency distribution of values */
} metric_type_t;

/* Forward declarations */
struct metrics_registry;
struct metric;

/* Metric value */
typedef union {
    int64_t counter;      /* For counters */
    double gauge;         /* For gauges */
    struct {              /* For timers */
        uint64_t count;
        double min;
        double max;
        double sum;
        double sum_squares;
    } timer;
    struct {              /* For histograms */
        uint64_t count;
        double min;
        double max;
        double sum;
        uint64_t* buckets;
        size_t num_buckets;
        double* bucket_bounds;
    } histogram;
} metric_value_t;

/* Metric data structure */
typedef struct metric {
    char* name;                   /* Metric name */
    char* description;            /* Metric description */
    metric_type_t type;           /* Metric type */
    metric_value_t value;         /* Metric value */
    pthread_mutex_t mutex;        /* Mutex for thread safety */
    struct metric* next;          /* Next metric in linked list */
} metric_t;

/* Timer context for timing operations */
typedef struct {
    struct timespec start_time;   /* Operation start time */
    metric_t* metric;             /* Associated metric */
} timer_context_t;

/* Metrics registry */
typedef struct metrics_registry {
    metric_t* metrics;            /* Linked list of metrics */
    pthread_mutex_t mutex;        /* Mutex for thread safety */
    int auto_export;              /* Whether to auto-export metrics */
    char* export_path;            /* Path for auto-exported metrics */
    int export_interval;          /* Auto-export interval in seconds */
    pthread_t export_thread;      /* Thread for auto-export */
    int export_thread_running;    /* Flag to control export thread */
} metrics_registry_t;

/* Metrics registry functions */
metrics_registry_t* metrics_registry_create();
void metrics_registry_free(metrics_registry_t* registry);
void metrics_registry_enable_auto_export(metrics_registry_t* registry, const char* export_path, int interval);
void metrics_registry_disable_auto_export(metrics_registry_t* registry);
int metrics_registry_export(metrics_registry_t* registry, const char* export_path);

/* Metric creation functions */
metric_t* metrics_create_counter(metrics_registry_t* registry, const char* name, const char* description);
metric_t* metrics_create_gauge(metrics_registry_t* registry, const char* name, const char* description);
metric_t* metrics_create_timer(metrics_registry_t* registry, const char* name, const char* description);
metric_t* metrics_create_histogram(metrics_registry_t* registry, const char* name, const char* description, 
                                  double* bucket_bounds, size_t num_buckets);

/* Counter operations */
void metrics_counter_inc(metric_t* counter, int64_t value);

/* Gauge operations */
void metrics_gauge_set(metric_t* gauge, double value);
void metrics_gauge_inc(metric_t* gauge, double value);
void metrics_gauge_dec(metric_t* gauge, double value);

/* Timer operations */
timer_context_t* metrics_timer_start(metric_t* timer);
void metrics_timer_stop(timer_context_t* context);
void metrics_timer_observe(metric_t* timer, double seconds);

/* Histogram operations */
void metrics_histogram_observe(metric_t* histogram, double value);

/* Helper functions */
double metrics_get_timestamp();
double metrics_elapsed_time(struct timespec start);
char* metrics_get_json(metrics_registry_t* registry);

/* Standard metrics */
#define METRIC_SERVER_REQUESTS "server_requests_total"
#define METRIC_SERVER_REQUEST_DURATION "server_request_duration_seconds"
#define METRIC_DB_OPERATIONS "db_operations_total"
#define METRIC_DB_OPERATION_DURATION "db_operation_duration_seconds"
#define METRIC_DB_READ_OPERATIONS "db_read_operations_total"
#define METRIC_DB_WRITE_OPERATIONS "db_write_operations_total"
#define METRIC_ACTIVE_CONNECTIONS "active_connections"
#define METRIC_COLLECTION_SIZE "collection_documents_total"
#define METRIC_DOCUMENT_SIZE "document_size_bytes"
#define METRIC_API_ERRORS "api_errors_total"
#define METRIC_SYSTEM_MEMORY "system_memory_bytes"
#define METRIC_CACHE_SIZE "cache_size_bytes"
#define METRIC_CACHE_HITS "cache_hits_total"
#define METRIC_CACHE_MISSES "cache_misses_total"
#define METRIC_CACHE_EVICTIONS "cache_evictions_total"

/* Global metrics registry */
extern struct metrics_registry* g_metrics_registry;

/* Metric accessor functions for use across modules */
metric_t* get_server_requests_metric(void);
metric_t* get_server_request_duration_metric(void);
metric_t* get_db_operations_metric(void);
metric_t* get_db_operation_duration_metric(void);
metric_t* get_db_read_operations_metric(void);
metric_t* get_db_write_operations_metric(void);
metric_t* get_active_connections_metric(void);
metric_t* get_api_errors_metric(void);
metric_t* get_cache_hits_metric(void);
metric_t* get_cache_misses_metric(void);
metric_t* get_cache_evictions_metric(void);
metric_t* get_cache_size_metric(void);
metric_t* get_system_memory_metric(void);

#endif /* METRICS_H */