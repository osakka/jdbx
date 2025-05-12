# Metrics System

The JSON Database Server includes a robust metrics system for monitoring and performance analysis. This document explains how to use the metrics features.

## Overview

The metrics system allows you to:
- Track counters (incrementing values)
- Monitor gauges (values that can go up or down)
- Measure operation times with timers
- Collect value distributions with histograms
- Export metrics to files for analysis
- Set up automatic metrics collection and export

## Standard Metrics

The database server tracks several standard metrics:

| Metric Name | Type | Description |
|-------------|------|-------------|
| `server_requests_total` | Counter | Total number of API requests received |
| `server_request_duration_seconds` | Timer | Time taken to process API requests |
| `db_operations_total` | Counter | Total number of database operations |
| `db_operation_duration_seconds` | Timer | Time taken to execute database operations |
| `active_connections` | Gauge | Current number of active connections |
| `collection_documents_total` | Gauge | Number of documents in each collection |
| `document_size_bytes` | Histogram | Size distribution of documents |
| `api_errors_total` | Counter | Number of API errors by type |
| `system_memory_bytes` | Gauge | Memory usage by the server |
| `cache_size_bytes` | Gauge | Size of internal caches |
| `cache_hits_total` | Counter | Number of cache hits |
| `cache_misses_total` | Counter | Number of cache misses |

## Accessing Metrics

### Via HTTP API

Metrics are available through a dedicated API endpoint:

```
GET /api/metrics
```

This returns metrics in a plain text format:

```
# JSON Database Metrics - 2023-05-01 12:34:56
server_requests_total counter 15243
server_request_duration_seconds timer count=15243 min=0.000213 max=1.543210 sum=423.112000 avg=0.027758
active_connections gauge 12.000000
...
```

### Configuration

You can configure metrics through the configuration file:

```json
"metrics": {
  "enabled": true,
  "path": "metrics/",
  "interval": 60,
  "detailed": true
}
```

- `enabled`: Enable or disable metrics collection
- `path`: Directory to store metrics files (relative to binary location if not absolute)
- `interval`: Auto-export interval in seconds (0 to disable)
- `detailed`: Whether to include detailed metrics (histograms)

### Automatic Export

When auto-export is enabled, the server will periodically write metrics to:

```
<path>/metrics_YYYYMMDD_HHMMSS.txt
```

## Monitoring Tools

The metrics system is compatible with common monitoring tools:

### Grafana Dashboard

A sample Grafana dashboard is included in the `/examples/grafana/` directory. To use it:

1. Configure a data source to read the metrics files
2. Import the dashboard JSON file

### Prometheus Integration

For Prometheus monitoring, use the included metrics exporter:

```bash
./bin/jsondb_metrics_exporter --port=5001
```

This starts a server at http://localhost:5001/metrics that exposes metrics in Prometheus format. The exporter automatically locates and reads metrics files from the configured metrics directory relative to the binary location.

## Adding Custom Metrics

You can add custom metrics in your own code by using the metrics API:

```c
/* Create metrics */
metric_t* requests = metrics_create_counter(registry, "my_requests_total", "Total requests");
metric_t* duration = metrics_create_timer(registry, "my_operation_duration", "Operation time");

/* Use metrics */
metrics_counter_inc(requests, 1);

timer_context_t* timer = metrics_timer_start(duration);
// ... do work ...
metrics_timer_stop(timer);
```

## Performance Impact

The metrics system is designed to have minimal impact on performance:

- Thread-safe implementation with fine-grained locks
- Lock-free fast paths for common operations
- Efficient data structures for histograms
- Asynchronous export to avoid blocking operations

In typical usage, the overhead is less than 1% of total processing time.

## Troubleshooting

If metrics are not being collected or exported:

1. Check if metrics are enabled in the configuration
2. Ensure the metrics path directory exists and is writable
   - Remember that paths are now relative to the binary location
   - The server will attempt to create the directory if it doesn't exist
3. Look for errors in the server logs
4. Try manually exporting metrics:
   ```
   GET /api/metrics/export
   ```

## Metrics File Format

The metrics file format is designed to be both human-readable and machine-parsable:

```
# JSON Database Metrics - [timestamp]
# Format: name type value [additional_fields]

metric_name counter value
metric_name gauge value
metric_name timer count=N min=X max=Y sum=Z avg=A
metric_name histogram count=N min=X max=Y sum=Z avg=A
metric_name bucket le=B count=C
```

Each line contains a metric name, its type, and associated values.

## Analyzing Metrics

The `bin` directory includes utilities for analyzing metrics:

```bash
# Generate a summary report
./bin/jsondb_tools metrics-analyze --file=metrics.txt

# Compare two metrics files
./bin/jsondb_tools metrics-diff --before=metrics1.txt --after=metrics2.txt

# Generate a performance report
./bin/jsondb_tools metrics-report --dir=metrics/
```

Note that relative paths in the above commands are relative to the binary location. These tools help identify performance bottlenecks and track changes over time.