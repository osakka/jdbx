#!/bin/bash
# Clean up old metrics documents and restart server

echo "Stopping server..."
/opt/jsondb/build/jsondb_runtime.sh stop

echo "Removing old metrics from database file..."
# Since we can't easily edit the binary file, we'll just restart fresh
# The cleanup function in the server will handle removing old documents

echo "Starting server with new metrics implementation..."
/opt/jsondb/build/jsondb_runtime.sh start

echo "Waiting for server to clean up old metrics..."
sleep 10

echo "Checking new metrics collection..."
curl -s http://localhost:5000/api/collections/system_metrics/documents | python3 -m json.tool | grep -E '("_id"|"type")' | head -20

echo "Done!"