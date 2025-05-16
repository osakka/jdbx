#!/bin/bash
# Test script for optimized database operations

# Stop the server if it's running
echo "Stopping any running JSONDB server instances..."
pkill -f jsondb_server

# Clean and rebuild the server
echo "Building the server with optimized database operations..."
cd /opt/jsondb/src
make clean
make

# Start the server with debug logging
echo "Starting the server with debug logging..."
cd /opt/jsondb
./build/bin/jsondb_server -f -l debug > /tmp/jsondb_optimized_debug.log 2>&1 &
sleep 2
SERVER_PID=$!

echo "Server started with PID: $SERVER_PID"

# Wait for server to initialize
sleep 3

# Test authentication
echo "Testing authentication..."
curl -X POST "http://localhost:5000/api/auth/login" -H "Content-Type: application/json" -d '{"username": "admin", "password": "admin"}' > /tmp/token.json
TOKEN=$(jq -r '.token' /tmp/token.json)

if [ -z "$TOKEN" ]; then
    echo "Failed to get authentication token"
    exit 1
fi

echo "Successfully authenticated with token: ${TOKEN:0:20}..."

# Test collection listing
echo "Testing collection listing..."
time curl -X GET "http://localhost:5000/api/collections" -H "Authorization: Bearer $TOKEN" -H "Content-Type: application/json" -o /tmp/collections.json

# Create a collection
echo "Creating test collection..."
time curl -X POST "http://localhost:5000/api/collections" -H "Authorization: Bearer $TOKEN" -H "Content-Type: application/json" -d '{"name": "test_collection"}' -o /tmp/create_collection.json

# Create test documents
echo "Creating test documents..."
for i in {1..10}; do
    curl -X POST "http://localhost:5000/api/collections/test_collection/documents" -H "Authorization: Bearer $TOKEN" -H "Content-Type: application/json" -d "{\"name\": \"Test Document $i\", \"value\": $i, \"active\": true}" -o /tmp/doc_$i.json
done

# Query documents
echo "Querying documents..."
time curl -X GET "http://localhost:5000/api/collections/test_collection/documents" -H "Authorization: Bearer $TOKEN" -H "Content-Type: application/json" -o /tmp/query_documents.json

# Stop the server
echo "Stopping the server..."
kill $SERVER_PID

echo "Test completed."
echo "Log file available at: /tmp/jsondb_optimized_debug.log"