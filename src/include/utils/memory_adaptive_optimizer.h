#ifndef JDBX_MEMORY_ADAPTIVE_OPTIMIZER_H
#define JDBX_MEMORY_ADAPTIVE_OPTIMIZER_H

#include <stdint.h>

/**
 * @file memory_adaptive_optimizer.h
 * @brief Adaptive Threshold Optimization System - Phase 5 Component
 * 
 * Self-tuning memory allocator thresholds based on real-world performance metrics.
 * Continuously optimizes Arena/TLSF decision boundaries to maximize performance
 * for actual workload patterns with brain surgeon precision.
 */

/* Initialization and lifecycle */
void memory_adaptive_optimizer_init(void);

/* Optimization control */
int memory_adaptive_optimizer_optimize(void);

/* Configuration management */
void memory_adaptive_optimizer_get_thresholds(uint64_t* arena_max, uint64_t* tlsf_min, uint64_t* tlsf_max);
void memory_adaptive_optimizer_configure(uint32_t interval_s, uint32_t learning_rate_ppm, uint32_t enabled);

/* Reporting and monitoring */
void memory_adaptive_optimizer_report(void);
void memory_adaptive_optimizer_reset(void);

/* Environment variable configuration */
#define JDBX_ADAPTIVE_OPTIMIZATION_ENABLED     "JDBX_ADAPTIVE_OPTIMIZATION_ENABLED"
#define JDBX_ADAPTIVE_OPTIMIZATION_INTERVAL    "JDBX_ADAPTIVE_OPTIMIZATION_INTERVAL"
#define JDBX_ADAPTIVE_LEARNING_RATE            "JDBX_ADAPTIVE_LEARNING_RATE"

#endif /* JDBX_MEMORY_ADAPTIVE_OPTIMIZER_H */