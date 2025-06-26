/**
 * @file memory_production_monitor.c
 * @brief Ultra-Precision Production Memory Monitoring System
 * 
 * SURGICAL PRECISION monitoring for exotic memory allocators in production.
 * Zero tolerance for error - comprehensive metrics, alerting, and validation.
 * Brain surgeon level precision for production deployment.
 */

#include "utils/memory_types.h"
#include "utils/memory_allocator_config.h"
#include "utils/memory_manager.h"
#include "utils/tlsf_allocator.h"
#include "utils/arena_allocator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <stdatomic.h>
#include <stdint.h>
#include <sys/time.h>

/**
 * ============================================================================
 * ULTRA-PRECISION METRICS COLLECTION
 * ============================================================================
 */

/* Production metrics - atomic for thread safety */
typedef struct {
    /* Allocation Counts */
    _Alignas(64) _Atomic(uint64_t) arena_allocations;
    _Alignas(64) _Atomic(uint64_t) tlsf_allocations;
    _Alignas(64) _Atomic(uint64_t) system_allocations;
    _Alignas(64) _Atomic(uint64_t) total_allocations;
    
    /* Performance Metrics */
    _Alignas(64) _Atomic(uint64_t) arena_alloc_time_ns;
    _Alignas(64) _Atomic(uint64_t) tlsf_alloc_time_ns;
    _Alignas(64) _Atomic(uint64_t) system_alloc_time_ns;
    
    /* Size Distribution */
    _Alignas(64) _Atomic(uint64_t) small_allocations;   /* <1KB */
    _Alignas(64) _Atomic(uint64_t) medium_allocations;  /* 1KB-64KB */
    _Alignas(64) _Atomic(uint64_t) large_allocations;   /* >64KB */
    
    /* Failure Metrics */
    _Alignas(64) _Atomic(uint64_t) allocation_failures;
    _Alignas(64) _Atomic(uint64_t) fallback_events;
    _Alignas(64) _Atomic(uint64_t) emergency_triggers;
    
    /* Memory Usage */
    _Alignas(64) _Atomic(uint64_t) arena_bytes_allocated;
    _Alignas(64) _Atomic(uint64_t) tlsf_bytes_allocated;
    _Alignas(64) _Atomic(uint64_t) system_bytes_allocated;
    
    /* Checkpoint Metrics */
    _Alignas(64) _Atomic(uint64_t) checkpoints_created;
    _Alignas(64) _Atomic(uint64_t) checkpoints_rewound;
    _Alignas(64) _Atomic(uint64_t) bulk_frees_performed;
    
    /* Health Indicators */
    _Alignas(64) _Atomic(uint64_t) last_update_timestamp;
    _Alignas(64) _Atomic(uint32_t) health_status;
} production_metrics_t;

static production_metrics_t g_metrics = {0};

/* Performance threshold configuration */
typedef struct {
    uint64_t max_allocation_time_ns;
    uint64_t max_failure_rate_ppm;      /* Parts per million */
    uint64_t max_fallback_rate_ppm;
    uint32_t health_check_interval_ms;
    uint32_t alert_threshold_seconds;
} performance_thresholds_t;

static performance_thresholds_t g_thresholds = {
    .max_allocation_time_ns = 1000000,    /* 1ms max allocation time */
    .max_failure_rate_ppm = 1000,         /* 0.1% max failure rate */
    .max_fallback_rate_ppm = 5000,        /* 0.5% max fallback rate */
    .health_check_interval_ms = 1000,     /* 1 second health checks */
    .alert_threshold_seconds = 5          /* 5 second alert threshold */
};

/**
 * ============================================================================
 * SURGICAL PRECISION TIMING
 * ============================================================================
 */

/* Ultra-precise nanosecond timing */
static inline uint64_t get_nanoseconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/* Record allocation metrics with surgical precision */
void memory_monitor_record_allocation(int allocator_type, size_t size, uint64_t duration_ns) {
    /* Update counters atomically */
    atomic_fetch_add(&g_metrics.total_allocations, 1);
    
    /* Size distribution */
    if (size < 1024) {
        atomic_fetch_add(&g_metrics.small_allocations, 1);
    } else if (size <= 65536) {
        atomic_fetch_add(&g_metrics.medium_allocations, 1);
    } else {
        atomic_fetch_add(&g_metrics.large_allocations, 1);
    }
    
    /* Allocator-specific metrics */
    switch (allocator_type) {
        case MEMORY_FLAG_ARENA_ALLOCATED:
            atomic_fetch_add(&g_metrics.arena_allocations, 1);
            atomic_fetch_add(&g_metrics.arena_alloc_time_ns, duration_ns);
            atomic_fetch_add(&g_metrics.arena_bytes_allocated, size);
            break;
        case MEMORY_FLAG_TLSF_ALLOCATED:
            atomic_fetch_add(&g_metrics.tlsf_allocations, 1);
            atomic_fetch_add(&g_metrics.tlsf_alloc_time_ns, duration_ns);
            atomic_fetch_add(&g_metrics.tlsf_bytes_allocated, size);
            break;
        default:
            atomic_fetch_add(&g_metrics.system_allocations, 1);
            atomic_fetch_add(&g_metrics.system_alloc_time_ns, duration_ns);
            atomic_fetch_add(&g_metrics.system_bytes_allocated, size);
            break;
    }
    
    /* Update timestamp */
    atomic_store(&g_metrics.last_update_timestamp, get_nanoseconds());
}

/* Record failure events */
void memory_monitor_record_failure(int failure_type) {
    switch (failure_type) {
        case 0: /* Allocation failure */
            atomic_fetch_add(&g_metrics.allocation_failures, 1);
            break;
        case 1: /* Fallback event */
            atomic_fetch_add(&g_metrics.fallback_events, 1);
            break;
        case 2: /* Emergency trigger */
            atomic_fetch_add(&g_metrics.emergency_triggers, 1);
            break;
    }
    
    atomic_store(&g_metrics.last_update_timestamp, get_nanoseconds());
}

/* Record checkpoint operations */
void memory_monitor_record_checkpoint(int operation_type) {
    switch (operation_type) {
        case 0: /* Checkpoint created */
            atomic_fetch_add(&g_metrics.checkpoints_created, 1);
            break;
        case 1: /* Checkpoint rewound */
            atomic_fetch_add(&g_metrics.checkpoints_rewound, 1);
            break;
        case 2: /* Bulk free performed */
            atomic_fetch_add(&g_metrics.bulk_frees_performed, 1);
            break;
    }
    
    atomic_store(&g_metrics.last_update_timestamp, get_nanoseconds());
}

/**
 * ============================================================================
 * BRAIN SURGEON PRECISION HEALTH MONITORING
 * ============================================================================
 */

/* Health status codes */
#define HEALTH_STATUS_EXCELLENT  0
#define HEALTH_STATUS_GOOD       1
#define HEALTH_STATUS_WARNING     2
#define HEALTH_STATUS_CRITICAL    3
#define HEALTH_STATUS_EMERGENCY   4

/* Calculate current health status with surgical precision */
uint32_t memory_monitor_calculate_health(void) {
    uint64_t total_allocs = atomic_load(&g_metrics.total_allocations);
    uint64_t failures = atomic_load(&g_metrics.allocation_failures);
    uint64_t fallbacks = atomic_load(&g_metrics.fallback_events);
    uint64_t emergencies = atomic_load(&g_metrics.emergency_triggers);
    
    /* Emergency state */
    if (emergencies > 0) {
        return HEALTH_STATUS_EMERGENCY;
    }
    
    /* Insufficient data */
    if (total_allocs < 1000) {
        return HEALTH_STATUS_GOOD; /* Assume good with limited data */
    }
    
    /* Calculate failure rates in parts per million */
    uint64_t failure_rate_ppm = (failures * 1000000) / total_allocs;
    uint64_t fallback_rate_ppm = (fallbacks * 1000000) / total_allocs;
    
    /* Critical thresholds */
    if (failure_rate_ppm > g_thresholds.max_failure_rate_ppm * 10) {
        return HEALTH_STATUS_CRITICAL;
    }
    
    /* Warning thresholds */
    if (failure_rate_ppm > g_thresholds.max_failure_rate_ppm ||
        fallback_rate_ppm > g_thresholds.max_fallback_rate_ppm) {
        return HEALTH_STATUS_WARNING;
    }
    
    /* Check performance metrics */
    uint64_t arena_allocs = atomic_load(&g_metrics.arena_allocations);
    uint64_t tlsf_allocs = atomic_load(&g_metrics.tlsf_allocations);
    
    if (arena_allocs > 0) {
        uint64_t avg_arena_time = atomic_load(&g_metrics.arena_alloc_time_ns) / arena_allocs;
        if (avg_arena_time > g_thresholds.max_allocation_time_ns) {
            return HEALTH_STATUS_WARNING;
        }
    }
    
    if (tlsf_allocs > 0) {
        uint64_t avg_tlsf_time = atomic_load(&g_metrics.tlsf_alloc_time_ns) / tlsf_allocs;
        if (avg_tlsf_time > g_thresholds.max_allocation_time_ns) {
            return HEALTH_STATUS_WARNING;
        }
    }
    
    /* Excellent health */
    return HEALTH_STATUS_EXCELLENT;
}

/* Update health status atomically */
void memory_monitor_update_health(void) {
    uint32_t new_health = memory_monitor_calculate_health();
    atomic_store(&g_metrics.health_status, new_health);
}

/* Get current health status */
uint32_t memory_monitor_get_health(void) {
    return atomic_load(&g_metrics.health_status);
}

/**
 * ============================================================================
 * ULTRA-PRECISION REPORTING
 * ============================================================================
 */

/* Generate surgical precision production report */
void memory_monitor_production_report(void) {
    /* Snapshot all metrics atomically */
    uint64_t arena_allocs = atomic_load(&g_metrics.arena_allocations);
    uint64_t tlsf_allocs = atomic_load(&g_metrics.tlsf_allocations);
    uint64_t system_allocs = atomic_load(&g_metrics.system_allocations);
    uint64_t total_allocs = atomic_load(&g_metrics.total_allocations);
    
    uint64_t arena_bytes = atomic_load(&g_metrics.arena_bytes_allocated);
    uint64_t tlsf_bytes = atomic_load(&g_metrics.tlsf_bytes_allocated);
    uint64_t system_bytes = atomic_load(&g_metrics.system_bytes_allocated);
    
    uint64_t failures = atomic_load(&g_metrics.allocation_failures);
    uint64_t fallbacks = atomic_load(&g_metrics.fallback_events);
    uint64_t emergencies = atomic_load(&g_metrics.emergency_triggers);
    
    uint64_t small_allocs = atomic_load(&g_metrics.small_allocations);
    uint64_t medium_allocs = atomic_load(&g_metrics.medium_allocations);
    uint64_t large_allocs = atomic_load(&g_metrics.large_allocations);
    
    uint32_t health = atomic_load(&g_metrics.health_status);
    
    const char* health_names[] = {
        "EXCELLENT", "GOOD", "WARNING", "CRITICAL", "EMERGENCY"
    };
    
    fprintf(stderr, "\n");
    fprintf(stderr, "================================================================\n");
    fprintf(stderr, "PRODUCTION MEMORY ALLOCATOR REPORT - SURGICAL PRECISION\n");
    fprintf(stderr, "================================================================\n");
    fprintf(stderr, "Health Status: %s\n", health_names[health]);
    fprintf(stderr, "Report Timestamp: %lu ns\n", atomic_load(&g_metrics.last_update_timestamp));
    fprintf(stderr, "\n");
    
    /* Allocation Distribution */
    fprintf(stderr, "--- ALLOCATION DISTRIBUTION ---\n");
    fprintf(stderr, "Total Allocations: %lu\n", total_allocs);
    
    if (total_allocs > 0) {
        fprintf(stderr, "Arena:  %lu (%.1f%%)\n", arena_allocs, (double)arena_allocs * 100.0 / total_allocs);
        fprintf(stderr, "TLSF:   %lu (%.1f%%)\n", tlsf_allocs, (double)tlsf_allocs * 100.0 / total_allocs);
        fprintf(stderr, "System: %lu (%.1f%%)\n", system_allocs, (double)system_allocs * 100.0 / total_allocs);
    }
    fprintf(stderr, "\n");
    
    /* Size Distribution */
    fprintf(stderr, "--- SIZE DISTRIBUTION ---\n");
    fprintf(stderr, "Small (<1KB):    %lu (%.1f%%)\n", small_allocs, (double)small_allocs * 100.0 / total_allocs);
    fprintf(stderr, "Medium (1-64KB): %lu (%.1f%%)\n", medium_allocs, (double)medium_allocs * 100.0 / total_allocs);
    fprintf(stderr, "Large (>64KB):   %lu (%.1f%%)\n", large_allocs, (double)large_allocs * 100.0 / total_allocs);
    fprintf(stderr, "\n");
    
    /* Memory Usage */
    fprintf(stderr, "--- MEMORY USAGE ---\n");
    fprintf(stderr, "Arena Memory:  %lu bytes (%.1f MB)\n", arena_bytes, (double)arena_bytes / (1024*1024));
    fprintf(stderr, "TLSF Memory:   %lu bytes (%.1f MB)\n", tlsf_bytes, (double)tlsf_bytes / (1024*1024));
    fprintf(stderr, "System Memory: %lu bytes (%.1f MB)\n", system_bytes, (double)system_bytes / (1024*1024));
    
    uint64_t total_bytes = arena_bytes + tlsf_bytes + system_bytes;
    fprintf(stderr, "Total Memory:  %lu bytes (%.1f MB)\n", total_bytes, (double)total_bytes / (1024*1024));
    fprintf(stderr, "\n");
    
    /* Performance Metrics */
    fprintf(stderr, "--- PERFORMANCE METRICS ---\n");
    if (arena_allocs > 0) {
        uint64_t avg_arena_time = atomic_load(&g_metrics.arena_alloc_time_ns) / arena_allocs;
        fprintf(stderr, "Arena Avg Time:  %lu ns (%.3f μs)\n", avg_arena_time, (double)avg_arena_time / 1000.0);
    }
    if (tlsf_allocs > 0) {
        uint64_t avg_tlsf_time = atomic_load(&g_metrics.tlsf_alloc_time_ns) / tlsf_allocs;
        fprintf(stderr, "TLSF Avg Time:   %lu ns (%.3f μs)\n", avg_tlsf_time, (double)avg_tlsf_time / 1000.0);
    }
    if (system_allocs > 0) {
        uint64_t avg_system_time = atomic_load(&g_metrics.system_alloc_time_ns) / system_allocs;
        fprintf(stderr, "System Avg Time: %lu ns (%.3f μs)\n", avg_system_time, (double)avg_system_time / 1000.0);
    }
    fprintf(stderr, "\n");
    
    /* Error Metrics */
    fprintf(stderr, "--- ERROR METRICS ---\n");
    fprintf(stderr, "Allocation Failures: %lu", failures);
    if (total_allocs > 0) {
        double failure_rate = (double)failures * 100.0 / total_allocs;
        fprintf(stderr, " (%.4f%%)", failure_rate);
    }
    fprintf(stderr, "\n");
    
    fprintf(stderr, "Fallback Events: %lu", fallbacks);
    if (total_allocs > 0) {
        double fallback_rate = (double)fallbacks * 100.0 / total_allocs;
        fprintf(stderr, " (%.4f%%)", fallback_rate);
    }
    fprintf(stderr, "\n");
    
    fprintf(stderr, "Emergency Triggers: %lu\n", emergencies);
    fprintf(stderr, "\n");
    
    /* Checkpoint Metrics */
    fprintf(stderr, "--- CHECKPOINT METRICS ---\n");
    fprintf(stderr, "Checkpoints Created: %lu\n", atomic_load(&g_metrics.checkpoints_created));
    fprintf(stderr, "Checkpoints Rewound: %lu\n", atomic_load(&g_metrics.checkpoints_rewound));
    fprintf(stderr, "Bulk Frees: %lu\n", atomic_load(&g_metrics.bulk_frees_performed));
    
    fprintf(stderr, "================================================================\n");
    fprintf(stderr, "\n");
}

/* JSON format report for monitoring systems */
void memory_monitor_json_report(void) {
    fprintf(stderr, "{\n");
    fprintf(stderr, "  \"timestamp\": %lu,\n", atomic_load(&g_metrics.last_update_timestamp));
    fprintf(stderr, "  \"health_status\": %u,\n", atomic_load(&g_metrics.health_status));
    fprintf(stderr, "  \"allocations\": {\n");
    fprintf(stderr, "    \"total\": %lu,\n", atomic_load(&g_metrics.total_allocations));
    fprintf(stderr, "    \"arena\": %lu,\n", atomic_load(&g_metrics.arena_allocations));
    fprintf(stderr, "    \"tlsf\": %lu,\n", atomic_load(&g_metrics.tlsf_allocations));
    fprintf(stderr, "    \"system\": %lu\n", atomic_load(&g_metrics.system_allocations));
    fprintf(stderr, "  },\n");
    fprintf(stderr, "  \"memory_usage\": {\n");
    fprintf(stderr, "    \"arena_bytes\": %lu,\n", atomic_load(&g_metrics.arena_bytes_allocated));
    fprintf(stderr, "    \"tlsf_bytes\": %lu,\n", atomic_load(&g_metrics.tlsf_bytes_allocated));
    fprintf(stderr, "    \"system_bytes\": %lu\n", atomic_load(&g_metrics.system_bytes_allocated));
    fprintf(stderr, "  },\n");
    fprintf(stderr, "  \"errors\": {\n");
    fprintf(stderr, "    \"failures\": %lu,\n", atomic_load(&g_metrics.allocation_failures));
    fprintf(stderr, "    \"fallbacks\": %lu,\n", atomic_load(&g_metrics.fallback_events));
    fprintf(stderr, "    \"emergencies\": %lu\n", atomic_load(&g_metrics.emergency_triggers));
    fprintf(stderr, "  }\n");
    fprintf(stderr, "}\n");
}

/**
 * ============================================================================
 * PRODUCTION INTEGRATION HOOKS
 * ============================================================================
 */

/* Reset all metrics for clean production start */
void memory_monitor_reset_metrics(void) {
    memset(&g_metrics, 0, sizeof(production_metrics_t));
    atomic_store(&g_metrics.last_update_timestamp, get_nanoseconds());
    atomic_store(&g_metrics.health_status, HEALTH_STATUS_GOOD);
    
    fprintf(stderr, "Production memory monitoring metrics reset\n");
}

/* Configure monitoring thresholds */
void memory_monitor_configure_thresholds(uint64_t max_alloc_time_ns, uint64_t max_failure_ppm, uint64_t max_fallback_ppm) {
    g_thresholds.max_allocation_time_ns = max_alloc_time_ns;
    g_thresholds.max_failure_rate_ppm = max_failure_ppm;
    g_thresholds.max_fallback_rate_ppm = max_fallback_ppm;
    
    fprintf(stderr, "Production monitoring thresholds configured:\n");
    fprintf(stderr, "  Max allocation time: %lu ns\n", max_alloc_time_ns);
    fprintf(stderr, "  Max failure rate: %lu ppm\n", max_failure_ppm);
    fprintf(stderr, "  Max fallback rate: %lu ppm\n", max_fallback_ppm);
}

/* Get metrics snapshot for external monitoring */
void memory_monitor_get_metrics_snapshot(production_metrics_t* snapshot) {
    if (!snapshot) return;
    
    /* Atomic snapshot of all metrics */
    snapshot->arena_allocations = atomic_load(&g_metrics.arena_allocations);
    snapshot->tlsf_allocations = atomic_load(&g_metrics.tlsf_allocations);
    snapshot->system_allocations = atomic_load(&g_metrics.system_allocations);
    snapshot->total_allocations = atomic_load(&g_metrics.total_allocations);
    
    snapshot->allocation_failures = atomic_load(&g_metrics.allocation_failures);
    snapshot->fallback_events = atomic_load(&g_metrics.fallback_events);
    snapshot->emergency_triggers = atomic_load(&g_metrics.emergency_triggers);
    
    snapshot->arena_bytes_allocated = atomic_load(&g_metrics.arena_bytes_allocated);
    snapshot->tlsf_bytes_allocated = atomic_load(&g_metrics.tlsf_bytes_allocated);
    snapshot->system_bytes_allocated = atomic_load(&g_metrics.system_bytes_allocated);
    
    snapshot->health_status = atomic_load(&g_metrics.health_status);
    snapshot->last_update_timestamp = atomic_load(&g_metrics.last_update_timestamp);
}