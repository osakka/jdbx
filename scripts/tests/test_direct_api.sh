#!/bin/bash

# Start the server first
echo "Starting server..."
build/jsondb_runtime.sh start

sleep 3

# Test only the working endpoints
echo ""
echo "Testing working endpoints:"
echo "========================"

# Health check
echo -n "Health check: "
curl -s http://localhost:5000/api/health | jq -r '.status' || echo "FAILED"

# OpenAPI spec
echo -n "OpenAPI spec: "
curl -s http://localhost:5000/api/openapi.json | jq -r '.openapi' || echo "FAILED"

# Try creating a collection directly (bypassing unified documents)
echo ""
echo "Testing direct collection access (bypassing unified documents):"
echo "=============================================================="

# Create a test collection
echo -n "Creating test collection: "
curl -s -X POST http://localhost:5000/api/collections/mylib/testcoll \
  -H "Content-Type: application/json" \
  -d '{"test": "data", "value": 123}' \
  --max-time 5 || echo "TIMEOUT/ERROR"

echo ""
echo "Checking server status..."
build/jsondb_runtime.sh status