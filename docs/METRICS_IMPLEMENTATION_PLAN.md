# JSONdb Metrics Implementation Plan

## Overview
JSONdb already has a complete metrics infrastructure in place but it's not being utilized. This plan outlines how to activate and expose all metrics via API.

## Current State
- ✅ Complete metrics system in `src/components/utils/metrics.c`
- ✅ API endpoints already exist (`/api/metrics`, `/api/metrics/stats`, etc.)
- ❌ Metrics registry not initialized
- ❌ No metrics being collected
- ❌ UI shows N/A because no data is available

## Implementation Plan

### Phase 1: Initialize Metrics Registry
1. Add metrics registry initialization in server startup
2. Create global metrics registry instance
3. Register all standard metrics at startup

### Phase 2: Implement Metric Collection Points

#### Server Metrics
- **server_requests_total**: Increment in `handle_client()` for each request
- **server_request_duration_seconds**: Time each request in `handle_request()`
- **active_connections**: Update in connection accept/close
- **api_errors_total**: Track in error response handlers

#### Database Metrics
- **db_operations_total**: Track in:
  - `db_insert_document()`
  - `db_get_document()`
  - `db_update_document()`
  - `db_delete_document()`
  - `db_query_collection()`
- **db_operation_duration_seconds**: Time each DB operation
- **collection_documents_total**: Update on insert/delete
- **document_size_bytes**: Track document sizes on insert/update

#### Cache Metrics
- **cache_hits_total**: Increment in `cache_get()` on hit
- **cache_misses_total**: Increment in `cache_get()` on miss
- **cache_size_bytes**: Update on cache insert/evict

#### System Metrics
- **system_memory_bytes**: Update periodically from health checks

### Phase 3: Enhanced API Response
Update `/api/health` to include:
```json
{
  "metrics": {
    "operations": {
      "total": 15243,
      "read": 10234,
      "write": 5009,
      "delete": 0
    },
    "performance": {
      "avg_response_time_ms": 27.8,
      "operations_per_second": 125.4
    },
    "cache": {
      "hit_rate": 0.89,
      "size_mb": 24.5
    }
  }
}
```

### Phase 4: Real-time Metrics Updates
1. Add operation counters that persist across restarts
2. Calculate rates and averages on the fly
3. Maintain rolling windows for time-based metrics

## Files to Modify

1. **src/components/main.c**
   - Initialize metrics registry in main()
   
2. **src/components/core/server.c**
   - Add request counting and timing
   - Track active connections
   
3. **src/components/database/simplified_db.c**
   - Add operation counters and timers
   - Track document counts and sizes
   
4. **src/components/utils/cache.c**
   - Add hit/miss counters
   - Track cache size
   
5. **src/components/api/health_api.c**
   - Enhance health endpoint with metrics summary
   
## Expected Results
Once implemented, the UI will show:
- Real operation counts and rates
- Actual response times
- Live performance graphs
- Cache hit rates
- Memory usage trends

## Testing
1. Verify metrics increment correctly
2. Check API endpoints return data
3. Confirm UI displays real values
4. Test metric persistence across restarts