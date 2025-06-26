#ifndef JDBX_MEMORY_PRODUCTION_MONITOR_H
#define JDBX_MEMORY_PRODUCTION_MONITOR_H

#include <stdint.h>
#include <stdatomic.h>

/* Production metrics structure - exposed for external monitoring */
typedef struct {
    /* Allocation Counts */
    _Atomic(uint64_t) arena_allocations;
    _Atomic(uint64_t) tlsf_allocations;
    _Atomic(uint64_t) system_allocations;
    _Atomic(uint64_t) total_allocations;
    
    /* Performance Metrics */
    _Atomic(uint64_t) arena_alloc_time_ns;
    _Atomic(uint64_t) tlsf_alloc_time_ns;
    _Atomic(uint64_t) system_alloc_time_ns;
    
    /* Size Distribution */
    _Atomic(uint64_t) small_allocations;   /* <1KB */
    _Atomic(uint64_t) medium_allocations;  /* 1KB-64KB */
    _Atomic(uint64_t) large_allocations;   /* >64KB */
    
    /* Failure Metrics */
    _Atomic(uint64_t) allocation_failures;
    _Atomic(uint64_t) fallback_events;
    _Atomic(uint64_t) emergency_triggers;
    
    /* Memory Usage */
    _Atomic(uint64_t) arena_bytes_allocated;
    _Atomic(uint64_t) tlsf_bytes_allocated;
    _Atomic(uint64_t) system_bytes_allocated;
    
    /* Checkpoint Metrics */
    _Atomic(uint64_t) checkpoints_created;
    _Atomic(uint64_t) checkpoints_rewound;
    _Atomic(uint64_t) bulk_frees_performed;
    
    /* Health Indicators */
    _Atomic(uint64_t) last_update_timestamp;
    _Atomic(uint32_t) health_status;
} production_metrics_t;

/* Health status codes */
#define HEALTH_STATUS_EXCELLENT  0
#define HEALTH_STATUS_GOOD       1
#define HEALTH_STATUS_WARNING     2
#define HEALTH_STATUS_CRITICAL    3
#define HEALTH_STATUS_EMERGENCY   4

/* Metrics recording functions */
void memory_monitor_record_allocation(int allocator_type, size_t size, uint64_t duration_ns);
void memory_monitor_record_failure(int failure_type);
void memory_monitor_record_checkpoint(int operation_type);

/* Health monitoring */
uint32_t memory_monitor_calculate_health(void);
void memory_monitor_update_health(void);
uint32_t memory_monitor_get_health(void);

/* Reporting functions */
void memory_monitor_production_report(void);
void memory_monitor_json_report(void);

/* Configuration and management */
void memory_monitor_reset_metrics(void);
void memory_monitor_configure_thresholds(uint64_t max_alloc_time_ns, uint64_t max_failure_ppm, uint64_t max_fallback_ppm);
void memory_monitor_get_metrics_snapshot(production_metrics_t* snapshot);

/* Timing utilities */
static inline uint64_t memory_monitor_get_nanoseconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

#endif /* JDBX_MEMORY_PRODUCTION_MONITOR_H */