#ifndef QUERY_TRACKER_H
#define QUERY_TRACKER_H

#include "utils/json.h"
#include <time.h>
#include <stdint.h>
#include <pthread.h>

/**
 * @file query_tracker.h
 * @brief Adaptive query pattern tracking for automatic index optimization
 * 
 * This system tracks query patterns to automatically create and maintain
 * indexes for optimal performance without manual configuration.
 */

/* Query pattern tracking configuration */
#define QUERY_TRACKER_MAX_PATTERNS 10000
#define QUERY_TRACKER_INDEX_THRESHOLD_COUNT_MIN 10      /* Minimum queries before considering */
#define QUERY_TRACKER_INDEX_THRESHOLD_COUNT_DEFAULT 50  /* Default threshold */
#define QUERY_TRACKER_INDEX_THRESHOLD_AVG_MS_MIN 0.5    /* Index anything over 0.5ms */
#define QUERY_TRACKER_INDEX_THRESHOLD_AVG_MS_DEFAULT 2.0 /* Default avg time threshold */
#define QUERY_TRACKER_CLEANUP_INTERVAL 3600  /* 1 hour */

/* Dynamic thresholds based on collection size */
#define QUERY_TRACKER_DYNAMIC_THRESHOLD_SMALL 100    /* Collections under 100 docs */
#define QUERY_TRACKER_DYNAMIC_THRESHOLD_MEDIUM 1000  /* Collections under 1000 docs */

/* Query pattern entry */
typedef struct query_pattern {
    char* collection_name;       /* Collection being queried */
    char* field_path;           /* JSON path of queried field (e.g., "token", "user.email") */
    uint64_t query_count;       /* Number of times this field was queried */
    double total_time_ms;       /* Total time spent on queries for this field */
    double avg_time_ms;         /* Average query time for this field */
    double max_time_ms;         /* Maximum query time for this field */
    time_t first_seen;          /* When this pattern was first observed */
    time_t last_seen;           /* When this pattern was last observed */
    int has_index;              /* Whether an index exists for this field */
    struct query_pattern* next; /* Hash table collision handling */
} query_pattern_t;

/* Query tracker structure */
typedef struct query_tracker {
    query_pattern_t** patterns;     /* Hash table of query patterns */
    size_t num_buckets;            /* Number of hash buckets */
    size_t pattern_count;          /* Total number of tracked patterns */
    pthread_rwlock_t lock;         /* Thread safety */
    time_t last_cleanup;           /* Last cleanup timestamp */
} query_tracker_t;

/* Query timing context */
typedef struct query_timing {
    struct timespec start_time;
    const char* collection_name;
    json_value_t* query_json;
} query_timing_t;

/* === Query Tracker Functions === */

/**
 * Initialize the global query tracker
 */
int query_tracker_init(void);

/**
 * Cleanup the global query tracker
 */
void query_tracker_cleanup(void);

/**
 * Start timing a query (call before query execution)
 */
query_timing_t* query_tracker_start_timing(const char* collection_name, json_value_t* query_json);

/**
 * End timing a query and record the pattern (call after query execution)
 */
void query_tracker_end_timing(query_timing_t* timing);

/**
 * Get query patterns that should be indexed
 * Returns array of patterns that meet indexing thresholds
 */
query_pattern_t** query_tracker_get_index_candidates(size_t* count);

/**
 * Mark a field as having an index
 */
void query_tracker_mark_indexed(const char* collection_name, const char* field_path);

/**
 * Get query statistics for monitoring
 */
json_value_t* query_tracker_get_stats(void);

/**
 * Force cleanup of old/unused patterns
 */
void query_tracker_force_cleanup(void);

/* === Internal Helper Functions === */

/**
 * Extract field paths from a query JSON object
 * Returns array of field paths that are being queried
 */
char** extract_query_field_paths(json_value_t* query_json, size_t* count);

/**
 * Free array of field paths
 */
void free_field_paths(char** paths, size_t count);

/**
 * Hash function for pattern lookup
 */
uint32_t query_pattern_hash(const char* collection_name, const char* field_path);

/**
 * Calculate time difference in milliseconds
 */
double timespec_diff_ms(const struct timespec* start, const struct timespec* end);

#endif /* QUERY_TRACKER_H */