/**
 * Enhanced Metrics Collection System for JDBX
 * Integrates with QuickJS engine for advanced analytics and configurable collection
 */

#ifndef ENHANCED_METRICS_H
#define ENHANCED_METRICS_H

#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include "utils/json.h"
#include "database/database.h"

/* Enhanced metrics initialization */
int enhanced_metrics_init(uint32_t update_interval);
void enhanced_metrics_cleanup(void);

/* HTTP/API metrics recording */
void enhanced_metrics_record_http_status(int status_code);
void enhanced_metrics_record_response_time(double response_time_ms);

/* Database operation metrics */
void enhanced_metrics_record_db_operation(const char* operation, double duration_ms, int success, int index_used);

/* System resource updates */
void enhanced_metrics_update_system_resources(void);

/* JavaScript-powered analytics */
json_value_t* enhanced_metrics_calculate_percentiles(void);
json_value_t* enhanced_metrics_get_report(int include_percentiles);

/* Database persistence */
int enhanced_metrics_persist_to_database(database_t* db);

/* Metrics helper macros for easy integration */
#define METRICS_RECORD_HTTP(status) enhanced_metrics_record_http_status(status)
#define METRICS_RECORD_RESPONSE_TIME(ms) enhanced_metrics_record_response_time(ms)
#define METRICS_RECORD_DB_OP(op, duration, success, indexed) \
    enhanced_metrics_record_db_operation(op, duration, success, indexed)

/* Timer helper for measuring operations */
typedef struct {
    struct timespec start_time;
    const char* operation_name;
} enhanced_metrics_timer_t;

static inline enhanced_metrics_timer_t enhanced_metrics_start_timer(const char* operation) {
    enhanced_metrics_timer_t timer;
    timer.operation_name = operation;
    clock_gettime(CLOCK_MONOTONIC, &timer.start_time);
    return timer;
}

static inline double enhanced_metrics_stop_timer(enhanced_metrics_timer_t* timer) {
    struct timespec end_time;
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    
    double elapsed = (end_time.tv_sec - timer->start_time.tv_sec);
    elapsed += (end_time.tv_nsec - timer->start_time.tv_nsec) / 1.0e9;
    return elapsed * 1000.0; // Convert to milliseconds
}

#define METRICS_TIME_OPERATION(operation_name, code_block) do { \
    enhanced_metrics_timer_t timer = enhanced_metrics_start_timer(operation_name); \
    code_block \
    double duration = enhanced_metrics_stop_timer(&timer); \
    enhanced_metrics_record_response_time(duration); \
} while(0)

#endif /* ENHANCED_METRICS_H */