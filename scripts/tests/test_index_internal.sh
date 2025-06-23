#!/bin/bash

echo "=== Testing Index Population (Internal) ==="
echo

# Ensure server is running
if ! pgrep -f jsondb_server > /dev/null; then
    echo "Starting JSONdb server..."
    cd /opt/jsondb && build/jsondb_runtime.sh start
    sleep 2
fi

# Get JWT token
echo "Getting authentication token..."
TOKEN=$(curl -s -X POST http://localhost:5000/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"admin"}' | jq -r '.token')

if [ -z "$TOKEN" ] || [ "$TOKEN" = "null" ]; then
    echo "Failed to get authentication token"
    exit 1
fi

echo "Token obtained successfully"
echo

# Test collection name
COLLECTION="perf_test_docs"

# Step 1: Create collection
echo "Step 1: Creating collection '$COLLECTION'..."
curl -s -X POST "http://localhost:5000/api/collections" \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d "{\"name\":\"$COLLECTION\"}" | jq .

echo

# Step 2: Insert documents
echo "Step 2: Inserting 1000 documents..."
for i in $(seq 1 1000); do
    age=$((20 + $i % 60))
    score=$((50 + $i % 50))
    curl -s -X POST "http://localhost:5000/api/collections/$COLLECTION/documents" \
      -H "Authorization: Bearer $TOKEN" \
      -H "Content-Type: application/json" \
      -d "{\"name\":\"User $i\",\"age\":$age,\"score\":$score,\"email\":\"user$i@example.com\",\"active\":true}" > /dev/null
    
    if [ $((i % 100)) -eq 0 ]; then
        echo -n "."
    fi
done
echo " Done!"
echo

# Step 3: Test range query performance
echo "Step 3: Testing range query performance (age between 30 and 35)..."

# Run query 5 times and measure time
total_time=0
for run in {1..5}; do
    START_TIME=$(date +%s%N)
    
    QUERY_RESULT=$(curl -s -X POST "http://localhost:5000/api/collections/$COLLECTION/query" \
      -H "Authorization: Bearer $TOKEN" \
      -H "Content-Type: application/json" \
      -d '{"age":{"$gte":30,"$lte":35}}')
    
    END_TIME=$(date +%s%N)
    QUERY_TIME=$(( ($END_TIME - $START_TIME) / 1000000 ))
    
    if [ $run -eq 1 ]; then
        # Count results on first run
        RESULT_COUNT=$(echo "$QUERY_RESULT" | jq '.documents | length')
        echo "Found $RESULT_COUNT documents"
    fi
    
    echo "Run $run: ${QUERY_TIME}ms"
    total_time=$((total_time + QUERY_TIME))
done

avg_time=$((total_time / 5))
echo "Average query time: ${avg_time}ms"
echo

# Step 4: Test exact match query
echo "Step 4: Testing exact match query (score = 75)..."
START_TIME=$(date +%s%N)

QUERY_RESULT=$(curl -s -X POST "http://localhost:5000/api/collections/$COLLECTION/query" \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"score":75}')

END_TIME=$(date +%s%N)
QUERY_TIME=$(( ($END_TIME - $START_TIME) / 1000000 ))

RESULT_COUNT=$(echo "$QUERY_RESULT" | jq '.documents | length')
echo "Found $RESULT_COUNT documents in ${QUERY_TIME}ms"
echo

# Step 5: Test full collection scan
echo "Step 5: Testing full collection scan (all documents)..."
START_TIME=$(date +%s%N)

QUERY_RESULT=$(curl -s -X GET "http://localhost:5000/api/collections/$COLLECTION/documents" \
  -H "Authorization: Bearer $TOKEN")

END_TIME=$(date +%s%N)
QUERY_TIME=$(( ($END_TIME - $START_TIME) / 1000000 ))

RESULT_COUNT=$(echo "$QUERY_RESULT" | jq '.documents | length')
echo "Retrieved $RESULT_COUNT documents in ${QUERY_TIME}ms"
echo

echo "=== Test Complete ==="
echo
echo "Note: Without explicit index creation via API, we're testing the"
echo "query optimizer's ability to use indexes if they exist internally."