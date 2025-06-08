#!/bin/bash

echo "Testing single connection with enhanced logging..."

# Clear previous logs
echo "=== NEW TEST RUN ===" >> /opt/jsondb/build/var/jsondb.log

# Check initial state
initial=$(curl -s http://localhost:5000/api/health | jq -r '.metrics.performance.active_connections')
echo "Initial active connections: $initial"

# Make single request with keep-alive
echo -e "\nMaking single request with keep-alive..."
curl -s -H "Connection: keep-alive" http://localhost:5000/api/health > /dev/null

# Check after request
sleep 1
after=$(curl -s http://localhost:5000/api/health | jq -r '.metrics.performance.active_connections')
echo "After request: $after"

# Wait for timeout
echo -e "\nWaiting 6 seconds for keep-alive timeout..."
sleep 6

# Check final
final=$(curl -s http://localhost:5000/api/health | jq -r '.metrics.performance.active_connections')
echo "Final active connections: $final"

echo -e "\n=== RELEVANT LOGS ==="
grep -E "(TRACE_METRICS|TRACE_HANDLER_START|TRACE_HANDLER_EXIT|connection_closing)" /opt/jsondb/build/var/jsondb.log | tail -20