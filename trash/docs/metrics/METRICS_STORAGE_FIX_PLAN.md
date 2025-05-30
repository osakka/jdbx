# Metrics Storage Fix Plan

## Current Problem
- Creating new document every 60 seconds
- 1,500+ documents after 1 day
- All documents serialized on every write
- Causes 400-500ms response times

## Proper Design

### 1. Single Document for Current Metrics
```json
{
  "_id": "metrics_current",
  "type": "current",
  "counters": {
    "server_requests_total": 12345,
    "db_operations_total": 5678,
    "db_read_operations_total": 4567,
    "db_write_operations_total": 1111,
    "api_errors_total": 23,
    "cache_hits_total": 0,
    "cache_misses_total": 0
  },
  "gauges": {
    "active_connections": 5,
    "cache_size_bytes": 0,
    "system_memory_bytes": 1234567
  },
  "histograms": {
    "request_duration_ms": {
      "count": 12345,
      "sum": 61725.5,
      "min": 0.1,
      "max": 100.5,
      "buckets": {
        "1": 1000,
        "5": 3000,
        "10": 2000,
        "50": 1000,
        "100": 500,
        "+Inf": 345
      }
    }
  },
  "last_updated": "2025-05-27T11:45:00Z"
}
```

### 2. Hourly Aggregates (Optional)
```json
{
  "_id": "metrics_hourly_2025-05-27-11",
  "type": "hourly",
  "hour": "2025-05-27T11:00:00Z",
  "counters": {
    "server_requests_total": 3600,
    "db_operations_total": 1800
  },
  "averages": {
    "active_connections": 5.2,
    "response_time_ms": 4.5
  }
}
```

### 3. Daily Aggregates (Optional)
```json
{
  "_id": "metrics_daily_2025-05-27",
  "type": "daily", 
  "date": "2025-05-27",
  "counters": {
    "server_requests_total": 86400
  }
}
```

## Implementation Steps

1. **Update metrics_persistence.c**:
   - Change save_metrics_snapshot to update existing document
   - Use atomic operations for counters
   - Keep only current values in memory

2. **Create migration script**:
   - Delete old per-minute documents
   - Create new current metrics document

3. **Update health API**:
   - Calculate rates on-the-fly
   - Use current metrics document

4. **Optional: Add aggregation**:
   - Hourly aggregation task
   - Daily rollup task
   - Automatic cleanup of old aggregates

## Benefits
- Only 1-3 documents instead of 1,500+
- Instant writes (no massive serialization)
- Sub-millisecond response times
- Still persistent (not memory-only)
- Scalable to billions of operations