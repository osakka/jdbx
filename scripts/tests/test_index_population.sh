#!/bin/bash

echo "=== Testing Index Population with Existing Documents ==="
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
COLLECTION="test_indexed_docs"

# Step 1: Create collection
echo "Step 1: Creating collection '$COLLECTION'..."
curl -s -X POST "http://localhost:5000/api/collections" \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d "{\"name\":\"$COLLECTION\"}" | jq .

echo

# Step 2: Insert documents WITHOUT index
echo "Step 2: Inserting 100 documents (no index yet)..."
for i in $(seq 1 100); do
    age=$((20 + $i % 60))
    curl -s -X POST "http://localhost:5000/api/collections/$COLLECTION/documents" \
      -H "Authorization: Bearer $TOKEN" \
      -H "Content-Type: application/json" \
      -d "{\"name\":\"User $i\",\"age\":$age,\"email\":\"user$i@example.com\",\"active\":true}" > /dev/null
    
    if [ $((i % 10)) -eq 0 ]; then
        echo -n "."
    fi
done
echo " Done!"
echo

# Step 3: Create index on age field
echo "Step 3: Creating index on 'age' field (should populate with existing docs)..."
INDEX_RESULT=$(curl -s -X POST "http://localhost:5000/api/collections/$COLLECTION/indexes" \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"field":"age","type":"btree"}')

echo "$INDEX_RESULT" | jq .
echo

# Step 4: Test range query using the index
echo "Step 4: Testing range query (age between 30 and 35)..."
START_TIME=$(date +%s%N)

QUERY_RESULT=$(curl -s -X POST "http://localhost:5000/api/collections/$COLLECTION/query" \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"age":{"$gte":30,"$lte":35}}')

END_TIME=$(date +%s%N)
QUERY_TIME=$(( ($END_TIME - $START_TIME) / 1000000 ))

echo "Query completed in ${QUERY_TIME}ms"
echo

# Count results
RESULT_COUNT=$(echo "$QUERY_RESULT" | jq '.documents | length')
echo "Found $RESULT_COUNT documents with age between 30 and 35"
echo

# Show a few results
echo "Sample results:"
echo "$QUERY_RESULT" | jq '.documents[:3]'
echo

# Step 5: List indexes
echo "Step 5: Listing indexes on collection..."
curl -s -X GET "http://localhost:5000/api/collections/$COLLECTION/indexes" \
  -H "Authorization: Bearer $TOKEN" | jq .

echo
echo "=== Test Complete ==="
echo
echo "The index should have been populated with all 100 existing documents."
echo "Query performance should be sub-millisecond with the B+tree index."