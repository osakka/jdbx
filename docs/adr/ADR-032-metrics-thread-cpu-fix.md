# ADR-032: Metrics Thread CPU Usage Fix

**Status**: Accepted  
**Date**: 2025-06-19  
**Authors**: JDBX Development Team  
**Reviewers**: Performance Committee  

## Context

The JDBX metrics persistence thread was consuming 100% of a CPU core due to a busy-wait anti-pattern in the background metrics saving process. This created severe performance issues:

- **100% CPU Usage**: Single thread consuming entire CPU core continuously
- **Resource Starvation**: Other server processes competing for CPU resources  
- **Performance Degradation**: Overall server performance impacted by metrics overhead
- **Production Unsuitability**: Deployment blocked by excessive resource consumption

## Problem

### Busy-Wait Anti-Pattern Analysis

```c
// PROBLEMATIC: Excessive wake-up frequency
while (metrics_thread_running) {
    usleep(100000);  // ❌ 100ms = 10 wake-ups per second
    
    if (should_save_metrics()) {
        save_metrics_to_database();  // Happens every 60 seconds
    }
}
```

### Root Cause Mathematics

- **Sleep Duration**: `usleep(100000)` = 100 milliseconds
- **Wake-ups Per Second**: 1000ms ÷ 100ms = 10 wake-ups/second
- **Wake-ups Per Minute**: 10 × 60 = 600 wake-ups/minute
- **Actual Work Frequency**: Save metrics every 60 seconds = 1 operation/minute
- **Waste Ratio**: 599 unnecessary wake-ups per actual operation

### Performance Impact

```
CPU Usage Pattern:
- Metrics thread: 100% of one CPU core
- Available for application: Reduced by 25-50% depending on core count
- Resource contention: High priority metrics thread starving other processes
```

## Decision

**Change metrics persistence thread sleep from 100ms to 5 seconds, reducing wake-up frequency by 50x.**

### Implementation

```c
// BEFORE: Busy-wait anti-pattern
usleep(100000);  // 100ms → 10 wake-ups/second → 36,000/hour

// AFTER: Efficient sleep pattern  
sleep(5);        // 5 seconds → 0.2 wake-ups/second → 720/hour
```

### Wake-up Frequency Optimization

| Pattern | Sleep Duration | Wake-ups/Second | Wake-ups/Hour | CPU Impact |
|---------|---------------|-----------------|---------------|------------|
| Before | 100ms | 10 | 36,000 | 100% CPU |
| After | 5s | 0.2 | 720 | ~0% CPU |
| **Improvement** | **50x** | **50x reduction** | **50x reduction** | **100% → 0%** |

## Rationale

### Why 5 Seconds is Optimal

1. **Functionality Preservation**: Metrics saved every 60 seconds
   - 5-second checks ensure maximum 5-second delay in save timing
   - 60-second target maintained with 5-second granularity acceptable

2. **Massive Efficiency Gain**: 
   - 599 unnecessary checks per minute → 11 unnecessary checks per minute
   - 98% reduction in unnecessary CPU wake-ups

3. **Resource Liberation**:
   - CPU freed for actual application work
   - Background thread becomes truly invisible during idle periods

### Mathematical Validation

```
Metrics Save Frequency: Every 60 seconds
Check Frequency (Before): Every 0.1 seconds = 600 checks per save
Check Frequency (After): Every 5 seconds = 12 checks per save
Efficiency Improvement: 600 ÷ 12 = 50x improvement
```

## Implementation

### Technical Change

**File**: `src/components/utils/metrics_persistence.c`

```c
// In metrics persistence thread main loop
while (thread_should_continue) {
    sleep(5);  // Changed from usleep(100000)
    
    if (time_since_last_save() >= METRICS_SAVE_INTERVAL) {
        persist_metrics_to_database();
        update_last_save_time();
    }
}
```

### Validation Approach

1. **Before Measurement**: `top` showing 100% CPU usage on metrics thread
2. **Code Change**: One-line modification to sleep duration
3. **After Measurement**: `top` showing 0.0% CPU usage during idle periods
4. **Functionality Test**: Metrics still saved every 60 seconds

## Consequences

### Positive

- ✅ **CPU Liberation**: 100% → 0% CPU usage during idle
- ✅ **Resource Availability**: Full CPU power available for application work
- ✅ **Performance Restoration**: Server resources no longer wasted on busy-waiting
- ✅ **Production Viability**: Background metrics truly runs in background

### Validation Results

```
CPU Usage Before: 100%+ constant on metrics thread
CPU Usage After: 0.0% during idle periods
Metrics Functionality: Preserved - saves every 60 seconds
Performance Impact: Eliminated - one-line fix
```

### Zero Functional Impact

- **Metrics Accuracy**: No change in metrics collection or storage
- **Save Frequency**: Still every 60 seconds as designed
- **Data Integrity**: No impact on metrics data quality
- **Timing Precision**: 5-second granularity acceptable for 60-second intervals

## Alternatives Considered

### Alternative 1: Adaptive Sleep
**Rejected**: Added complexity for minimal benefit over fixed 5-second sleep.

### Alternative 2: Event-Driven Metrics
**Rejected**: Major architectural change for what fixed with one line.

### Alternative 3: Different Thread Priority
**Rejected**: Doesn't solve the fundamental busy-wait inefficiency.

## Lessons Learned

### Performance Design Principles

1. **Match Sleep to Work Interval**: 
   - Work every 60 seconds ≠ check every 0.1 seconds
   - Sleep duration should be reasonable fraction of work interval

2. **Calculate Wake Frequency Impact**:
   - 100ms sleep = 36,000 wake-ups per hour
   - Always calculate actual vs necessary wake-up ratios

3. **Background Should Be Invisible**:
   - Background threads should have zero CPU impact when idle
   - "Background" means resource usage only during actual work

### Code Review Criteria

- ❌ **Reject**: Any sleep shorter than 1 second without justification
- ✅ **Accept**: Sleep duration reasonable for work frequency
- ✅ **Accept**: Background threads with near-zero idle CPU usage

## Conclusion

This one-line fix demonstrates the massive impact of addressing fundamental efficiency patterns. By eliminating the busy-wait anti-pattern:

- **Immediate Impact**: CPU usage reduced from 100% to 0%
- **Zero Complexity**: Single line change with zero architectural impact
- **Production Ready**: Background metrics truly runs in background
- **Performance Excellence**: Server resources available for actual work

The fix exemplifies JDBX's commitment to performance excellence through surgical precision improvements.