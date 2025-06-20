# ADR-011: Adaptive Indexing System

**Date**: June 8, 2025  
**Status**: Accepted  
**Version**: 3.1.0  
**Impact**: Performance  

## Context

Manual index creation led to:
- Missed optimization opportunities
- Over-indexing wasting space
- No query pattern awareness
- Static performance characteristics

## Decision

Implement automatic adaptive indexing:
- Query pattern tracking
- Automatic index creation
- Performance-based decisions
- Index lifecycle management

## Rationale

### Performance Goals
- Self-optimizing database
- Minimal manual tuning
- Dynamic workload adaptation
- Optimal resource usage

### Implementation Strategy
- Track all query patterns
- Analyze performance metrics
- Create beneficial indexes
- Remove underperforming indexes

## Implementation

### Query Pattern Tracking
```c
typedef struct query_pattern {
    char* collection;
    char* field_path;
    uint64_t query_count;
    double total_time_ms;
    double avg_time_ms;
    time_t first_seen;
    time_t last_seen;
} query_pattern_t;
```

### Index Creation Decision
```c
bool should_create_index(query_pattern_t* pattern) {
    // Thresholds
    const int MIN_QUERIES = 10;
    const double MIN_AVG_TIME_MS = 50.0;
    
    // System collections have lower thresholds
    if (is_system_collection(pattern->collection)) {
        return pattern->query_count >= 5 && 
               pattern->avg_time_ms >= 10.0;
    }
    
    return pattern->query_count >= MIN_QUERIES &&
           pattern->avg_time_ms >= MIN_AVG_TIME_MS;
}
```

### Background Indexer Thread
```c
void* adaptive_indexer_thread(void* arg) {
    while (running) {
        sleep(60); // Check every minute
        
        // Analyze patterns
        query_pattern_t* patterns = get_query_patterns();
        
        for (int i = 0; i < pattern_count; i++) {
            if (should_create_index(&patterns[i])) {
                create_index(patterns[i].collection,
                           patterns[i].field_path);
            }
        }
        
        // Cleanup underperforming indexes
        cleanup_ineffective_indexes();
    }
}
```

## Consequences

### Positive
- **Self-Optimizing**: No manual tuning
- **Performance**: Automatic improvements
- **Adaptive**: Handles workload changes
- **Efficient**: Removes bad indexes

### Negative
- **Overhead**: Pattern tracking cost
- **Delay**: Not immediate optimization
- **Complexity**: Background processing

### Mitigations
- Efficient pattern storage
- Configurable thresholds
- Manual override options
- Comprehensive metrics

## Technical Details

### Files Created
- `src/components/database/query_tracker.c` - Pattern tracking
- `src/components/database/adaptive_indexer.c` - Index decisions
- `src/components/database/index_metrics.c` - Performance tracking

### Metrics Collected
- Query count per pattern
- Average query time
- Index hit rate
- Index size overhead
- ROI calculation

### Configuration
- `adaptive_indexing_enabled`: true/false
- `index_creation_threshold`: query count
- `index_cleanup_threshold`: effectiveness %
- `indexer_check_interval`: seconds

## Validation

- ✅ Pattern tracking accurate
- ✅ Indexes created automatically
- ✅ Performance improvements measured
- ✅ Bad indexes removed
- ✅ System collections prioritized

## References

- Related: ADR-002 (Billion-Document Scale)
- Related: ADR-012 (Field-Level Operations)