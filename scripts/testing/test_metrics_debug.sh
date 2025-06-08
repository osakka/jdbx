#!/bin/bash

echo "=== Metrics Debug Test ==="

# Get initial state
echo -e "\n1. Initial state:"
INITIAL=$(curl -s http://localhost:5000/api/health)
echo "$INITIAL" | jq '.metrics.performance.active_connections'

# Check if connection_duration metric exists
echo -e "\n2. Checking Prometheus metrics for connection_duration:"
curl -s http://localhost:5000/api/metrics | grep -i "connection_duration" | head -3

# Make a request and look at logs
echo -e "\n3. Making a test request with Connection: close..."
curl -s http://localhost:5000/api/health -H "Connection: close" -o /dev/null

# Check the latest logs
echo -e "\n4. Latest connection-related logs:"
tail -1000 /opt/jsondb/build/var/jsondb.log | grep -i "connection.*timer\|Started.*timer\|Stopped.*timer\|connection.*duration.*metric" | tail -10

# Check if metrics module was initialized
echo -e "\n5. Checking metrics initialization in logs:"
grep -i "init_metrics\|metrics.*init" /opt/jsondb/build/var/jsondb.log | tail -5

echo -e "\n6. Current metrics state:"
curl -s http://localhost:5000/api/health | jq '.metrics | {
  threads: .threads.total,
  active_connections: .performance.active_connections,
  has_connection_metrics: (.performance.avg_connection_lifetime_ms != null)
}'