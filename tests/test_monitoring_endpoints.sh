#!/bin/bash

echo "Testing JSONdb Monitoring Endpoints"
echo "==================================="

# Start the server
echo "Starting JSONdb server..."
cd /opt/jsondb
build/jsondb_runtime.sh start

# Wait for server to start
sleep 3

echo -e "\n1. Testing /api/health endpoint:"
curl -s http://localhost:5000/api/health | jq .

echo -e "\n2. Testing /api/metrics endpoint:"
curl -s http://localhost:5000/api/metrics | jq . | head -20

echo -e "\n3. Testing /api/metrics/available endpoint:"
curl -s http://localhost:5000/api/metrics/available | jq .

echo -e "\n4. Creating some test data for metrics..."
curl -s -X POST http://localhost:5000/api/collections/test_metrics -H "Content-Type: application/json" -d '{"name":"test_metrics"}'

# Insert some documents
for i in {1..10}; do
    curl -s -X POST http://localhost:5000/api/collections/test_metrics/documents \
        -H "Content-Type: application/json" \
        -d "{\"id\":$i,\"value\":$((i*10))}" > /dev/null
done

echo -e "\n5. Testing /api/metrics with activity:"
curl -s http://localhost:5000/api/metrics | jq .

# Stop the server
echo -e "\nStopping JSONdb server..."
build/jsondb_runtime.sh stop