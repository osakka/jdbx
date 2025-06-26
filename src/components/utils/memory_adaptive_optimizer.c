/**
 * @file memory_adaptive_optimizer.c
 * @brief Adaptive Threshold Optimization System - Phase 5 Component
 * 
 * SURGICAL PRECISION: Self-tuning memory allocator thresholds based on real-world
 * performance metrics. Continuously optimizes Arena/TLSF decision boundaries to
 * maximize performance for actual workload patterns.
 * 
 * Brain surgeon level precision for production optimization.
 */

#include "utils/memory_allocator_config.h"
#include "utils/memory_manager.h"

/* Forward declarations to avoid circular includes */
typedef struct {
    uint64_t arena_allocations;
    uint64_t tlsf_allocations;
    uint64_t system_allocations;
    uint64_t total_allocations;
    uint64_t arena_alloc_time_ns;
    uint64_t tlsf_alloc_time_ns;
    uint64_t system_alloc_time_ns;
    uint64_t small_allocations;
    uint64_t medium_allocations;
    uint64_t large_allocations;
    uint64_t allocation_failures;
    uint64_t fallback_events;
    uint64_t emergency_triggers;
    uint64_t arena_bytes_allocated;
    uint64_t tlsf_bytes_allocated;
    uint64_t system_bytes_allocated;
    uint64_t checkpoints_created;
    uint64_t checkpoints_rewound;
    uint64_t bulk_frees_performed;
    uint64_t last_update_timestamp;
    uint32_t health_status;
} production_metrics_t;

void memory_monitor_get_metrics_snapshot(production_metrics_t* snapshot);
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <stdatomic.h>
#include <stdint.h>
#include <math.h>

/**
 * ============================================================================
 * ADAPTIVE THRESHOLD CONFIGURATION 
 * ============================================================================
 */

/* Optimization thresholds - dynamically adjusted */
typedef struct {
    /* Current allocation size thresholds */
    _Atomic(uint64_t) arena_max_size;           /* Max size for Arena allocator */
    _Atomic(uint64_t) tlsf_min_size;            /* Min size for TLSF allocator */
    _Atomic(uint64_t) tlsf_max_size;            /* Max size for TLSF allocator */
    
    /* Performance targets (nanoseconds) */
    _Atomic(uint64_t) target_arena_time_ns;     /* Target Arena allocation time */
    _Atomic(uint64_t) target_tlsf_time_ns;      /* Target TLSF allocation time */
    _Atomic(uint64_t) target_system_time_ns;    /* Target system allocation time */
    
    /* Optimization control */
    _Atomic(uint64_t) last_optimization_time;   /* Last optimization timestamp */
    _Atomic(uint32_t) optimization_interval_s;  /* Seconds between optimizations */
    _Atomic(uint32_t) optimization_enabled;     /* 1 if enabled, 0 if disabled */
    
    /* Learning parameters */
    _Atomic(uint32_t) learning_rate_ppm;        /* Learning rate in parts per million */
    _Atomic(uint32_t) adaptation_sensitivity;   /* Sensitivity to performance changes */
    
} adaptive_thresholds_t;

/* Global adaptive configuration */
static adaptive_thresholds_t g_adaptive_config = {
    .arena_max_size = ATOMIC_VAR_INIT(64 * 1024),    /* 64KB default */
    .tlsf_min_size = ATOMIC_VAR_INIT(32),             /* 32B default */
    .tlsf_max_size = ATOMIC_VAR_INIT(32 * 1024 * 1024), /* 32MB default */
    
    .target_arena_time_ns = ATOMIC_VAR_INIT(500),     /* 500ns target */
    .target_tlsf_time_ns = ATOMIC_VAR_INIT(800),      /* 800ns target */
    .target_system_time_ns = ATOMIC_VAR_INIT(1000),   /* 1000ns target */
    
    .last_optimization_time = ATOMIC_VAR_INIT(0),
    .optimization_interval_s = ATOMIC_VAR_INIT(60),   /* 1 minute intervals */
    .optimization_enabled = ATOMIC_VAR_INIT(1),       /* Enabled by default */
    
    .learning_rate_ppm = ATOMIC_VAR_INIT(50000),      /* 5% learning rate */
    .adaptation_sensitivity = ATOMIC_VAR_INIT(3),     /* Moderate sensitivity */
};

/**
 * ============================================================================
 * PERFORMANCE ANALYSIS ENGINE
 * ============================================================================
 */

/* Performance analysis results */
typedef struct {
    /* Allocator performance ratios (vs targets) */
    double arena_performance_ratio;     /* Actual/target performance */
    double tlsf_performance_ratio;      /* Actual/target performance */
    double system_performance_ratio;    /* Actual/target performance */
    
    /* Allocation distribution analysis */
    double arena_utilization;           /* % of allocations using Arena */
    double tlsf_utilization;            /* % of allocations using TLSF */
    double system_utilization;          /* % of allocations using system */
    
    /* Threshold optimization recommendations */
    uint64_t recommended_arena_max;     /* Recommended Arena max size */
    uint64_t recommended_tlsf_min;      /* Recommended TLSF min size */
    uint64_t recommended_tlsf_max;      /* Recommended TLSF max size */
    
    /* Confidence metrics */
    double optimization_confidence;     /* Confidence in recommendations (0-1) */
    uint64_t sample_size;              /* Number of allocations analyzed */
    
} performance_analysis_t;

/* Analyze current allocator performance */
static performance_analysis_t analyze_allocator_performance(void) {
    performance_analysis_t analysis = {0};
    production_metrics_t metrics;
    
    /* Get current metrics snapshot */
    memory_monitor_get_metrics_snapshot(&metrics);
    
    uint64_t total_allocs = metrics.total_allocations;
    if (total_allocs < 1000) {
        /* Insufficient data for analysis */
        analysis.optimization_confidence = 0.0;
        analysis.sample_size = total_allocs;
        return analysis;
    }
    
    analysis.sample_size = total_allocs;
    
    /* Calculate utilization ratios */
    analysis.arena_utilization = (double)metrics.arena_allocations / total_allocs;
    analysis.tlsf_utilization = (double)metrics.tlsf_allocations / total_allocs;
    analysis.system_utilization = (double)metrics.system_allocations / total_allocs;
    
    /* Calculate performance ratios (actual vs target) */
    uint64_t target_arena = atomic_load(&g_adaptive_config.target_arena_time_ns);
    uint64_t target_tlsf = atomic_load(&g_adaptive_config.target_tlsf_time_ns);
    uint64_t target_system = atomic_load(&g_adaptive_config.target_system_time_ns);
    
    if (metrics.arena_allocations > 0) {
        uint64_t avg_arena_time = metrics.arena_alloc_time_ns / metrics.arena_allocations;
        analysis.arena_performance_ratio = (double)avg_arena_time / target_arena;
    } else {
        analysis.arena_performance_ratio = 1.0; /* Neutral */
    }
    
    if (metrics.tlsf_allocations > 0) {
        uint64_t avg_tlsf_time = metrics.tlsf_alloc_time_ns / metrics.tlsf_allocations;
        analysis.tlsf_performance_ratio = (double)avg_tlsf_time / target_tlsf;
    } else {
        analysis.tlsf_performance_ratio = 1.0; /* Neutral */
    }
    
    if (metrics.system_allocations > 0) {
        uint64_t avg_system_time = metrics.system_alloc_time_ns / metrics.system_allocations;
        analysis.system_performance_ratio = (double)avg_system_time / target_system;
    } else {
        analysis.system_performance_ratio = 1.0; /* Neutral */
    }
    
    /* Calculate optimization confidence based on sample size and variance */
    if (total_allocs >= 10000) {
        analysis.optimization_confidence = 0.95; /* High confidence */
    } else if (total_allocs >= 5000) {
        analysis.optimization_confidence = 0.80; /* Good confidence */
    } else if (total_allocs >= 1000) {
        analysis.optimization_confidence = 0.60; /* Moderate confidence */
    } else {
        analysis.optimization_confidence = 0.30; /* Low confidence */
    }
    
    /* Generate threshold recommendations */
    uint64_t current_arena_max = atomic_load(&g_adaptive_config.arena_max_size);
    uint64_t current_tlsf_min = atomic_load(&g_adaptive_config.tlsf_min_size);
    uint64_t current_tlsf_max = atomic_load(&g_adaptive_config.tlsf_max_size);
    
    /* Arena threshold optimization */
    if (analysis.arena_performance_ratio > 1.5) {
        /* Arena is slow - reduce threshold */
        analysis.recommended_arena_max = current_arena_max * 0.8;
    } else if (analysis.arena_performance_ratio < 0.8 && analysis.arena_utilization > 0.3) {
        /* Arena is fast and well-utilized - increase threshold */
        analysis.recommended_arena_max = current_arena_max * 1.2;
    } else {
        analysis.recommended_arena_max = current_arena_max;
    }
    
    /* TLSF threshold optimization */
    if (analysis.tlsf_performance_ratio > 1.5) {
        /* TLSF is slow - increase minimum size */
        analysis.recommended_tlsf_min = current_tlsf_min * 1.5;
    } else if (analysis.tlsf_performance_ratio < 0.8 && analysis.tlsf_utilization > 0.2) {
        /* TLSF is fast and utilized - decrease minimum size */
        analysis.recommended_tlsf_min = current_tlsf_min * 0.8;
    } else {
        analysis.recommended_tlsf_min = current_tlsf_min;
    }
    
    analysis.recommended_tlsf_max = current_tlsf_max; /* Usually stable */
    
    return analysis;
}

/**
 * ============================================================================
 * ADAPTIVE OPTIMIZATION ENGINE
 * ============================================================================
 */

/* Apply threshold optimizations with learning rate */
static int apply_threshold_optimizations(const performance_analysis_t* analysis) {
    if (!analysis || analysis->optimization_confidence < 0.5) {
        return -1; /* Insufficient confidence for optimization */
    }
    
    uint32_t learning_rate = atomic_load(&g_adaptive_config.learning_rate_ppm);
    double learning_factor = (double)learning_rate / 1000000.0; /* Convert from PPM */
    
    /* Apply Arena threshold optimization */
    uint64_t current_arena_max = atomic_load(&g_adaptive_config.arena_max_size);
    uint64_t new_arena_max = current_arena_max + 
                            (uint64_t)((double)(analysis->recommended_arena_max - current_arena_max) * learning_factor);
    
    /* Bounds checking for Arena threshold */
    if (new_arena_max < 1024) new_arena_max = 1024;         /* Min 1KB */
    if (new_arena_max > 1024 * 1024) new_arena_max = 1024 * 1024; /* Max 1MB */
    
    /* Apply TLSF threshold optimization */
    uint64_t current_tlsf_min = atomic_load(&g_adaptive_config.tlsf_min_size);
    uint64_t new_tlsf_min = current_tlsf_min + 
                           (uint64_t)((double)(analysis->recommended_tlsf_min - current_tlsf_min) * learning_factor);
    
    /* Bounds checking for TLSF threshold */
    if (new_tlsf_min < 16) new_tlsf_min = 16;               /* Min 16B */
    if (new_tlsf_min > 8192) new_tlsf_min = 8192;          /* Max 8KB */
    
    /* Atomic updates */
    atomic_store(&g_adaptive_config.arena_max_size, new_arena_max);
    atomic_store(&g_adaptive_config.tlsf_min_size, new_tlsf_min);
    
    /* Log optimization if debug enabled */
    if (getenv("JDBX_MEM_DEBUG")) {
        fprintf(stderr, "Adaptive optimization applied:\n");
        fprintf(stderr, "  Arena max: %lu -> %lu bytes\n", current_arena_max, new_arena_max);
        fprintf(stderr, "  TLSF min:  %lu -> %lu bytes\n", current_tlsf_min, new_tlsf_min);
        fprintf(stderr, "  Confidence: %.2f, Sample size: %lu\n", 
                analysis->optimization_confidence, analysis->sample_size);
    }
    
    return 0;
}

/* Main optimization routine */
static int perform_adaptive_optimization(void) {
    /* Check if optimization is enabled */
    if (!atomic_load(&g_adaptive_config.optimization_enabled)) {
        return 0;
    }
    
    /* Check optimization interval */
    uint64_t current_time = time(NULL);
    uint64_t last_optimization = atomic_load(&g_adaptive_config.last_optimization_time);
    uint32_t interval = atomic_load(&g_adaptive_config.optimization_interval_s);
    
    if (current_time - last_optimization < interval) {
        return 0; /* Too soon for optimization */
    }
    
    /* Perform performance analysis */
    performance_analysis_t analysis = analyze_allocator_performance();
    
    /* Apply optimizations if confident enough */
    int result = apply_threshold_optimizations(&analysis);
    
    /* Update last optimization time */
    atomic_store(&g_adaptive_config.last_optimization_time, current_time);
    
    return result;
}

/**
 * ============================================================================
 * PUBLIC API
 * ============================================================================
 */

/* Initialize adaptive optimization system */
void memory_adaptive_optimizer_init(void) {
    /* Initialize from environment variables */
    const char* interval_str = getenv("JDBX_ADAPTIVE_OPTIMIZATION_INTERVAL");
    if (interval_str) {
        uint32_t interval = (uint32_t)atoi(interval_str);
        if (interval > 0 && interval <= 3600) { /* Max 1 hour */
            atomic_store(&g_adaptive_config.optimization_interval_s, interval);
        }
    }
    
    const char* learning_rate_str = getenv("JDBX_ADAPTIVE_LEARNING_RATE");
    if (learning_rate_str) {
        uint32_t rate = (uint32_t)atoi(learning_rate_str);
        if (rate > 0 && rate <= 200000) { /* Max 20% */
            atomic_store(&g_adaptive_config.learning_rate_ppm, rate);
        }
    }
    
    const char* enabled_str = getenv("JDBX_ADAPTIVE_OPTIMIZATION_ENABLED");
    if (enabled_str) {
        uint32_t enabled = (strcmp(enabled_str, "true") == 0 || strcmp(enabled_str, "1") == 0) ? 1 : 0;
        atomic_store(&g_adaptive_config.optimization_enabled, enabled);
    }
    
    /* Initialize optimization timestamp */
    atomic_store(&g_adaptive_config.last_optimization_time, time(NULL));
    
    if (getenv("JDBX_MEM_DEBUG")) {
        fprintf(stderr, "Adaptive optimization initialized:\n");
        fprintf(stderr, "  Enabled: %s\n", atomic_load(&g_adaptive_config.optimization_enabled) ? "true" : "false");
        fprintf(stderr, "  Interval: %u seconds\n", atomic_load(&g_adaptive_config.optimization_interval_s));
        fprintf(stderr, "  Learning rate: %u ppm\n", atomic_load(&g_adaptive_config.learning_rate_ppm));
    }
}

/* Trigger adaptive optimization cycle */
int memory_adaptive_optimizer_optimize(void) {
    return perform_adaptive_optimization();
}

/* Get current adaptive thresholds */
void memory_adaptive_optimizer_get_thresholds(uint64_t* arena_max, uint64_t* tlsf_min, uint64_t* tlsf_max) {
    if (arena_max) *arena_max = atomic_load(&g_adaptive_config.arena_max_size);
    if (tlsf_min) *tlsf_min = atomic_load(&g_adaptive_config.tlsf_min_size);
    if (tlsf_max) *tlsf_max = atomic_load(&g_adaptive_config.tlsf_max_size);
}

/* Set optimization parameters */
void memory_adaptive_optimizer_configure(uint32_t interval_s, uint32_t learning_rate_ppm, uint32_t enabled) {
    if (interval_s > 0 && interval_s <= 3600) {
        atomic_store(&g_adaptive_config.optimization_interval_s, interval_s);
    }
    if (learning_rate_ppm > 0 && learning_rate_ppm <= 200000) {
        atomic_store(&g_adaptive_config.learning_rate_ppm, learning_rate_ppm);
    }
    atomic_store(&g_adaptive_config.optimization_enabled, enabled ? 1 : 0);
}

/* Generate optimization report */
void memory_adaptive_optimizer_report(void) {
    performance_analysis_t analysis = analyze_allocator_performance();
    
    fprintf(stderr, "\n");
    fprintf(stderr, "================================================================\n");
    fprintf(stderr, "ADAPTIVE OPTIMIZATION REPORT - SURGICAL PRECISION\n");
    fprintf(stderr, "================================================================\n");
    fprintf(stderr, "Optimization Status: %s\n", 
            atomic_load(&g_adaptive_config.optimization_enabled) ? "ENABLED" : "DISABLED");
    fprintf(stderr, "Sample Size: %lu allocations\n", analysis.sample_size);
    fprintf(stderr, "Optimization Confidence: %.1f%%\n", analysis.optimization_confidence * 100.0);
    fprintf(stderr, "\n");
    
    /* Current thresholds */
    fprintf(stderr, "--- CURRENT THRESHOLDS ---\n");
    fprintf(stderr, "Arena Max Size:  %lu bytes (%.1f KB)\n", 
            atomic_load(&g_adaptive_config.arena_max_size),
            (double)atomic_load(&g_adaptive_config.arena_max_size) / 1024.0);
    fprintf(stderr, "TLSF Min Size:   %lu bytes\n", atomic_load(&g_adaptive_config.tlsf_min_size));
    fprintf(stderr, "TLSF Max Size:   %lu bytes (%.1f MB)\n",
            atomic_load(&g_adaptive_config.tlsf_max_size),
            (double)atomic_load(&g_adaptive_config.tlsf_max_size) / (1024.0 * 1024.0));
    fprintf(stderr, "\n");
    
    /* Performance analysis */
    fprintf(stderr, "--- PERFORMANCE ANALYSIS ---\n");
    fprintf(stderr, "Arena Performance:  %.2fx target (%.1f%% utilization)\n", 
            analysis.arena_performance_ratio, analysis.arena_utilization * 100.0);
    fprintf(stderr, "TLSF Performance:   %.2fx target (%.1f%% utilization)\n",
            analysis.tlsf_performance_ratio, analysis.tlsf_utilization * 100.0);
    fprintf(stderr, "System Performance: %.2fx target (%.1f%% utilization)\n",
            analysis.system_performance_ratio, analysis.system_utilization * 100.0);
    fprintf(stderr, "\n");
    
    /* Optimization configuration */
    fprintf(stderr, "--- OPTIMIZATION CONFIGURATION ---\n");
    fprintf(stderr, "Learning Rate:       %u ppm (%.1f%%)\n",
            atomic_load(&g_adaptive_config.learning_rate_ppm),
            (double)atomic_load(&g_adaptive_config.learning_rate_ppm) / 10000.0);
    fprintf(stderr, "Optimization Interval: %u seconds\n", atomic_load(&g_adaptive_config.optimization_interval_s));
    fprintf(stderr, "Last Optimization:     %lu seconds ago\n",
            time(NULL) - atomic_load(&g_adaptive_config.last_optimization_time));
    fprintf(stderr, "================================================================\n");
    fprintf(stderr, "\n");
}

/* Reset adaptive optimizer to defaults */
void memory_adaptive_optimizer_reset(void) {
    atomic_store(&g_adaptive_config.arena_max_size, 64 * 1024);
    atomic_store(&g_adaptive_config.tlsf_min_size, 32);
    atomic_store(&g_adaptive_config.tlsf_max_size, 32 * 1024 * 1024);
    atomic_store(&g_adaptive_config.last_optimization_time, time(NULL));
    
    if (getenv("JDBX_MEM_DEBUG")) {
        fprintf(stderr, "Adaptive optimizer reset to defaults\n");
    }
}