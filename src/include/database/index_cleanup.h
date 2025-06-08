#ifndef INDEX_CLEANUP_H
#define INDEX_CLEANUP_H

#include <pthread.h>
#include <time.h>
#include "database/database.h"
#include "database/index_metrics.h"

/* Cleanup thresholds */
#define INDEX_CLEANUP_MIN_AGE_HOURS 24           /* Index must exist for 24 hours */
#define INDEX_CLEANUP_MIN_QUERIES 100            /* Minimum queries before evaluation */
#define INDEX_CLEANUP_ROI_THRESHOLD -0.5         /* Remove if ROI < -50% */
#define INDEX_CLEANUP_EFFECTIVENESS_THRESHOLD 0.1 /* Remove if < 10% effective */
#define INDEX_CLEANUP_CHECK_INTERVAL 3600        /* Check every hour */
#define INDEX_CLEANUP_STARTUP_DELAY 300          /* 5 minute startup delay */

/* Index cleanup reasons */
typedef enum {
    CLEANUP_REASON_LOW_ROI,
    CLEANUP_REASON_LOW_EFFECTIVENESS,
    CLEANUP_REASON_DUPLICATE,
    CLEANUP_REASON_UNUSED,
    CLEANUP_REASON_MANUAL
} cleanup_reason_t;

/* Index cleanup statistics */
typedef struct {
    uint64_t checks_performed;
    uint64_t indexes_removed;
    uint64_t indexes_optimized;
    uint64_t storage_reclaimed;
    time_t last_cleanup;
} cleanup_stats_t;

/* Forward declaration */
struct index_metrics_collector;
typedef struct index_metrics_collector index_metrics_t;

/* Index cleanup system */
typedef struct {
    database_t* database;
    index_metrics_t* metrics;
    
    /* Cleanup thread */
    pthread_t cleanup_thread;
    pthread_mutex_t lock;
    int should_stop;
    
    /* Configuration */
    double roi_threshold;
    double effectiveness_threshold;
    int min_age_hours;
    int min_queries;
    int check_interval;
    
    /* Statistics */
    cleanup_stats_t stats;
} index_cleanup_t;

/* Cleanup decision information */
typedef struct {
    char collection_name[256];
    char index_name[256];
    cleanup_reason_t reason;
    double roi;
    double effectiveness;
    uint64_t storage_bytes;
    uint64_t query_count;
    time_t created_at;
    int should_remove;
} cleanup_decision_t;

/* Initialize index cleanup system */
index_cleanup_t* index_cleanup_init(database_t* db, index_metrics_t* metrics);

/* Start cleanup thread */
int index_cleanup_start(index_cleanup_t* cleanup);

/* Stop cleanup thread */
void index_cleanup_stop(index_cleanup_t* cleanup);

/* Cleanup system resources */
void index_cleanup_destroy(index_cleanup_t* cleanup);

/* Evaluate single index for removal */
cleanup_decision_t* index_cleanup_evaluate(index_cleanup_t* cleanup,
                                          const char* collection_name,
                                          const char* index_name);

/* Perform cleanup check on all indexes */
int index_cleanup_check_all(index_cleanup_t* cleanup);

/* Remove underperforming index */
int index_cleanup_remove_index(index_cleanup_t* cleanup,
                              const char* collection_name,
                              const char* index_name,
                              cleanup_reason_t reason);

/* Optimize index storage */
int index_cleanup_optimize_index(index_cleanup_t* cleanup,
                                const char* collection_name,
                                const char* index_name);

/* Get cleanup statistics */
cleanup_stats_t index_cleanup_get_stats(index_cleanup_t* cleanup);

/* Configure cleanup thresholds */
void index_cleanup_configure(index_cleanup_t* cleanup,
                           double roi_threshold,
                           double effectiveness_threshold,
                           int min_age_hours,
                           int min_queries);

/* Force immediate cleanup check */
int index_cleanup_force_check(index_cleanup_t* cleanup);

/* Export cleanup history */
json_value_t* index_cleanup_export_history(index_cleanup_t* cleanup);

#endif /* INDEX_CLEANUP_H */