# JDBX Metrics System Reference

**Version**: 2.0.7  
**Last Updated**: January 2025

This comprehensive guide documents the JDBX metrics system, including collection, storage, and analysis capabilities.

## Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Metrics Types](#metrics-types)
4. [Storage Implementation](#storage-implementation)
5. [API Endpoints](#api-endpoints)
6. [Configuration](#configuration)
7. [Performance](#performance)
8. [Monitoring Integration](#monitoring-integration)

## Overview

The JDBX metrics system provides comprehensive monitoring and performance analysis capabilities. It uses a time-series approach with fixed documents to ensure scalability and high performance.

### Key Features

- **Time-Series Storage**: Rolling window of metrics data with configurable retention
- **Fixed Documents**: 5 metric types stored as fixed documents to prevent unbounded growth
- **High Performance**: 10x performance improvement over previous implementations
- **Real-time Updates**: Metrics updated every 60 seconds automatically
- **System Integration**: Seamlessly integrated with the database and RBAC systems

## Architecture

### Design Principles

1. **Fixed Document Model**: Uses 5 fixed documents instead of creating new documents
2. **Append-and-Trim**: Time-series data appends new values and trims old ones
3. **Atomic Updates**: Single atomic update per metric type
4. **Current Values**: Maintains current values separately for fast access
5. **System Collection**: Stored in `_system_metrics` collection

### Document Structure

Each metric document follows this structure:

```json
{
  "_id": "doc-<timestamp>-<random>",
  "name": "operations",  // Searchable name field
  "type": "operations",
  "retention_minutes": 15,
  "max_entries": 15,
  "data": [
    {
      "timestamp": "2025-05-30T10:28:51Z",
      "total": 790,
      "database": 1324,
      "read": 911,
      "write": 366
    }
    // ... more time-series entries
  ],
  "current": {
    "total": 790,
    "database": 1324,
    "read": 911,
    "write": 366
  },
  "created_at": "2025-05-30T09:21:48Z",
  "updated_at": "2025-05-30T10:28:51Z"
}
```

## Metrics Types

### 1. Operations Metrics

**Document Name**: `operations`

Tracks database operation counts:

| Field | Type | Description |
|-------|------|-------------|
| `total` | counter | Total API requests |
| `database` | counter | Database operations |
| `read` | counter | Read operations |
| `write` | counter | Write operations |

### 2. Performance Metrics

**Document Name**: `performance`

Monitors response times and performance:

| Field | Type | Description |
|-------|------|-------------|
| `avg_response_time_ms` | gauge | Average response time |
| `min_response_time_ms` | gauge | Minimum response time |
| `max_response_time_ms` | gauge | Maximum response time |
| `active_connections` | gauge | Current active connections |

### 3. Cache Metrics

**Document Name**: `cache`

Tracks cache performance:

| Field | Type | Description |
|-------|------|-------------|
| `hit_rate` | gauge | Cache hit rate percentage |
| `hits` | counter | Total cache hits |
| `misses` | counter | Total cache misses |
| `evictions` | counter | Cache evictions |
| `size_bytes` | gauge | Current cache size |

### 4. Memory Metrics

**Document Name**: `memory`

Monitors system memory usage:

| Field | Type | Description |
|-------|------|-------------|
| `total_kb` | gauge | Total system memory |
| `free_kb` | gauge | Free system memory |
| `used_kb` | gauge | Used system memory |
| `process_kb` | gauge | Process memory usage |

### 5. Connection Metrics

**Document Name**: `connections`

Tracks connection statistics:

| Field | Type | Description |
|-------|------|-------------|
| `active` | gauge | Currently active connections |
| `total` | counter | Total connections handled |

## Storage Implementation

### Implementation Details

The metrics system is implemented in:
- `/opt/jdbx/src/components/utils/metrics_persistence.c`
- `/opt/jdbx/src/components/utils/metrics.c`

### Key Functions

```c
// Update operations metrics
void update_operations_metrics(Database* db) {
    // Find or create the operations metrics document
    // Append new time-series data
    // Trim old entries beyond retention period
    // Update current values
}

// Persist all metrics
void persist_all_metrics(Database* db) {
    update_operations_metrics(db);
    update_performance_metrics(db);
    update_cache_metrics(db);
    update_memory_metrics(db);
    update_connections_metrics(db);
}
```

### Update Cycle

1. Metrics are collected continuously during operations
2. Every 60 seconds, the persistence thread:
   - Calculates current values
   - Appends to time-series data
   - Trims old entries (keeps last 15)
   - Updates the document atomically

## API Endpoints

### Get All Metrics

```http
GET /api/metrics
Authorization: Bearer <token>
```

**Response**:
```json
{
  "status": "success",
  "data": {
    "operations": { /* operations metrics */ },
    "performance": { /* performance metrics */ },
    "cache": { /* cache metrics */ },
    "memory": { /* memory metrics */ },
    "connections": { /* connections metrics */ }
  }
}
```

### Get Specific Metric Type

```http
GET /api/metrics/{type}
Authorization: Bearer <token>
```

**Parameters**:
- `type`: One of `operations`, `performance`, `cache`, `memory`, `connections`

**Response**:
```json
{
  "status": "success",
  "data": {
    "type": "operations",
    "current": {
      "total": 790,
      "database": 1324,
      "read": 911,
      "write": 366
    },
    "history": [
      /* time-series data */
    ]
  }
}
```

### Query Metrics History

```http
POST /api/collections/_system_metrics/query
Authorization: Bearer <token>
Content-Type: application/json

{
  "filter": {"name": "operations"},
  "projection": {"data": 1}
}
```

## Configuration

### Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `JDBX_METRICS_ENABLED` | Enable metrics collection | `true` |
| `JDBX_METRICS_DIR` | Metrics storage directory | `${JDBX_VAR_DIR}/metrics` |
| `JDBX_METRICS_INTERVAL` | Update interval (seconds) | `60` |
| `JDBX_METRICS_RETENTION` | Data retention (minutes) | `15` |

### Configuration File

```json
{
  "metrics": {
    "enabled": true,
    "path": "var/metrics/",
    "interval": 60,
    "retention": 15,
    "detailed": true
  }
}
```

## Performance

### Benchmarks

The current implementation achieves:
- **Average Response Time**: ~38ms (was ~400ms)
- **Performance Improvement**: 10x
- **Storage Overhead**: Fixed 5 documents
- **Update Frequency**: Every 60 seconds
- **Query Performance**: O(1) for current values

### Optimization Techniques

1. **Fixed Document IDs**: Eliminates document creation overhead
2. **Atomic Updates**: Single update per metric type
3. **In-Memory Aggregation**: Calculations done before persistence
4. **Indexed Queries**: Name field indexed for fast lookups
5. **Batch Updates**: All metrics updated in single transaction

## Monitoring Integration

### Grafana Integration

Configure Grafana to query the metrics API:

1. Add JDBX as a data source
2. Use the `/api/metrics` endpoint
3. Configure authentication headers
4. Import provided dashboards

### Prometheus Integration

Export metrics in Prometheus format:

```bash
# Using the metrics exporter
./bin/jdbx_metrics_exporter --port=9090

# Or via API transformation
curl -H "Authorization: Bearer $TOKEN" \
  http://localhost:5000/api/metrics | \
  ./scripts/metrics_to_prometheus.py
```

### Custom Monitoring

Query metrics directly from the database:

```javascript
// JavaScript function example
function getMetricsTrend(metricType, field) {
  const doc = db.query("_system_metrics", {
    filter: {name: metricType}
  })[0];
  
  return doc.data.map(d => ({
    time: d.timestamp,
    value: d[field]
  }));
}
```

### Alerting

Set up alerts based on metrics:

```json
{
  "alert": "HighResponseTime",
  "condition": "performance.current.avg_response_time_ms > 100",
  "action": "notify",
  "interval": 300
}
```

## Best Practices

1. **Monitor Key Metrics**: Focus on operations, performance, and memory
2. **Set Appropriate Retention**: Balance history needs with storage
3. **Use Aggregation**: For long-term trends, aggregate to hourly/daily
4. **Alert Thresholds**: Set realistic thresholds based on baseline
5. **Regular Analysis**: Review metrics weekly for trends

## Troubleshooting

### Metrics Not Updating

1. Check if metrics are enabled in configuration
2. Verify the persistence thread is running
3. Check database permissions for _system_metrics collection
4. Review logs for errors

### High Memory Usage

1. Check retention settings
2. Verify max_entries configuration
3. Monitor document sizes
4. Consider reducing update frequency

### Performance Issues

1. Ensure indexes exist on name field
2. Check for lock contention
3. Verify atomic updates are working
4. Monitor persistence thread timing

## References

- [Metrics Implementation](src/components/utils/metrics.c)
- [Metrics Persistence](src/components/utils/metrics_persistence.c)
- [API Documentation](docs/api/api-rest.md#metrics)
- [Configuration Guide](docs/reference/reference-configuration.md#metrics)