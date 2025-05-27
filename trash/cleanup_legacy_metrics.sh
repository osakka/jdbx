#!/bin/bash

echo "Cleaning up legacy metrics documents..."

TOKEN=$(curl -s -X POST http://localhost:5000/api/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"admin"}' | jq -r '.token')

# First, let's just delete the entire collection and recreate it
echo "Dropping _system_metrics collection..."

# Since we can't drop collections via API, we'll delete all documents except metrics_current
# But since querying is slow, let's restart the server after fixing the code to prevent new metrics

echo "Stopping metrics collection temporarily..."

# For now, let's just test with the current metrics document
echo "Checking if current metrics document exists..."
curl -s "http://localhost:5000/api/collections/_system_metrics/metrics_current" \
  -H "Authorization: Bearer $TOKEN" | jq '.'