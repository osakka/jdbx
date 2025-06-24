# ADR-045: Metrics Deduplication and Adaptive Indexing Collection

Date: June 24, 2025

## Status

Accepted

## Context

The JDBX metrics persistence system was storing duplicate data points even when metric values hadn't changed. This was particularly problematic for stable metrics like cache hit rates (often 0) and adaptive indexing statistics (which change infrequently). The root cause was a JSON type mismatch - numbers stored as JSON_INTEGER were being compared against JSON_NUMBER types, causing `json_equals()` to return false for identical values.

Additionally, adaptive indexing metrics weren't being collected as part of the time-series metrics system, making it difficult to track indexing effectiveness over time.

## Decision

We implemented two key enhancements:

1. **Adaptive Indexing Metrics Collection**: Added adaptive indexing statistics to the metrics persistence system, collecting:
   - indexes_created: Number of indexes automatically created
   - indexes_skipped: Number of index creation attempts skipped
   - total_indexes: Total number of adaptive indexes
   - effective_indexes: Number of indexes meeting effectiveness criteria
   - effectiveness_rate: Percentage of effective indexes

2. **Intelligent JSON Number Comparison**: Enhanced `json_equals()` to intelligently compare numeric values across INTEGER and NUMBER types:
   ```c
   /* Special case: compare INTEGER and NUMBER types */
   if ((value1->type == JSON_INTEGER && value2->type == JSON_NUMBER) ||
       (value1->type == JSON_NUMBER && value2->type == JSON_INTEGER)) {
     /* Compare numeric values across types */
     double v1 = (value1->type == JSON_INTEGER) ? (double)value1->value.integer : value1->value.number;
     double v2 = (value2->type == JSON_INTEGER) ? (double)value2->value.integer : value2->value.number;
     return v1 == v2;
   }
   ```

3. **Metric Update Deduplication**: Modified the metric update logic to only save documents when values have actually changed, preventing redundant storage and network traffic.

## Consequences

### Positive

- **Storage Efficiency**: Metrics with stable values (cache, adaptive_indexing) only store data points when values change
- **Cleaner Visualization**: Graphs show actual change points rather than interpolated flat lines
- **Network Efficiency**: Reduced database writes for unchanged metrics
- **System-Wide Benefit**: Enhanced `json_equals()` benefits all JSON comparison operations
- **Adaptive Indexing Visibility**: Can now track index creation patterns and effectiveness over time

### Negative

- **Floating Point Comparison**: Comparing integers as doubles could theoretically have precision issues for very large integers (>2^53)
- **Backward Compatibility**: Existing metric documents with duplicate data points remain unchanged

### Neutral

- **Type Conversion Overhead**: Minimal performance impact from cross-type comparison
- **Memory Usage**: Same memory footprint, just fewer data points stored

## Implementation Details

The implementation involved:

1. Adding adaptive indexing collection in `metrics_persistence.c`
2. Enhancing `json_equals()` in `json.c` for cross-type numeric comparison
3. Modifying metric update logic to skip updates when values haven't changed
4. Ensuring consistent numeric type creation (integers for counts, doubles for rates)

## Validation

Confirmed through testing:
- Stable metrics (cache, adaptive_indexing) maintain single data points when values don't change
- Changing metrics (operations, memory) continue to store new data points
- All numeric comparisons work correctly across INTEGER/NUMBER type boundaries
- No regressions in existing functionality