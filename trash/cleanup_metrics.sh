#!/bin/bash

TOKEN=$(curl -s -X POST http://localhost:5000/api/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"admin"}' | jq -r '.token')

echo "Getting all metric IDs..."
METRIC_IDS=$(curl -s "http://localhost:5000/api/collections/_system_metrics" \
  -H "Authorization: Bearer $TOKEN" | jq -r '.documents[]._id')

COUNT=$(echo "$METRIC_IDS" | wc -l)
echo "Found $COUNT metrics to delete"

# Keep only the last 10 metrics
KEEP_COUNT=10
DELETE_IDS=$(echo "$METRIC_IDS" | head -n -$KEEP_COUNT)
DELETE_COUNT=$(echo "$DELETE_IDS" | wc -l)

echo "Deleting $DELETE_COUNT old metrics, keeping last $KEEP_COUNT..."

for ID in $DELETE_IDS; do
    curl -s -X DELETE "http://localhost:5000/api/collections/_system_metrics/$ID" \
      -H "Authorization: Bearer $TOKEN" >/dev/null
    echo -n "."
done

echo -e "\nDone! Checking performance..."
time curl -s http://localhost:5000/api/health | jq '.metrics.performance'