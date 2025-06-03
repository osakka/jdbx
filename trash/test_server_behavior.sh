#!/bin/bash

# Test server behavior with different request types
echo "=== JSONdb Server Behavior Test Suite ==="
echo "Testing server at http://localhost:5000"
echo

# Function to check if server is still responding
check_server_health() {
    echo -n "Checking server health... "
    if curl -s http://localhost:5000/api/health > /dev/null 2>&1; then
        echo "OK"
        return 0
    else
        echo "FAILED"
        return 1
    fi
}

# 1. Test basic API endpoints
echo "1. Testing Basic API Endpoints"
echo "------------------------------"

echo -n "GET /api/collections: "
time curl -s -o /dev/null -w "%{http_code} - %{time_total}s\n" http://localhost:5000/api/collections

echo -n "GET /api/health: "
time curl -s -o /dev/null -w "%{http_code} - %{time_total}s\n" http://localhost:5000/api/health

echo -n "GET /api/metrics: "
time curl -s -o /dev/null -w "%{http_code} - %{time_total}s\n" http://localhost:5000/api/metrics

echo

# 2. Test static file serving
echo "2. Testing Static File Serving"
echo "------------------------------"

echo -n "GET / (index.html): "
time curl -s -o /dev/null -w "%{http_code} - %{time_total}s - %{size_download} bytes\n" http://localhost:5000/

echo -n "GET /css/unified-theme.css: "
time curl -s -o /dev/null -w "%{http_code} - %{time_total}s - %{size_download} bytes\n" http://localhost:5000/css/unified-theme.css

echo -n "GET /js/app.js: "
time curl -s -o /dev/null -w "%{http_code} - %{time_total}s - %{size_download} bytes\n" http://localhost:5000/js/app.js

echo

# 3. Test concurrent connections
echo "3. Testing Concurrent Connections"
echo "---------------------------------"

echo "Sending 10 concurrent requests..."
for i in {1..10}; do
    curl -s http://localhost:5000/api/collections > /dev/null 2>&1 &
done
wait
check_server_health

echo

# 4. Test rapid sequential requests
echo "4. Testing Rapid Sequential Requests"
echo "------------------------------------"

echo "Sending 20 rapid requests..."
start_time=$(date +%s.%N)
for i in {1..20}; do
    response=$(curl -s -w "%{http_code}" -o /dev/null http://localhost:5000/api/health)
    if [ "$response" != "200" ]; then
        echo "Request $i failed with status: $response"
    fi
done
end_time=$(date +%s.%N)
duration=$(echo "$end_time - $start_time" | bc)
echo "Completed 20 requests in ${duration}s"
check_server_health

echo

# 5. Test large payload handling
echo "5. Testing Large Payload Handling"
echo "---------------------------------"

# Create a test collection
echo -n "Creating test collection: "
curl -s -X POST http://localhost:5000/api/collections \
    -H "Content-Type: application/json" \
    -d '{"name": "test_collection"}' \
    -w "%{http_code}\n" -o /dev/null

# Create a large document (1MB of data)
echo -n "Creating large document (1MB): "
large_data=$(python3 -c "import json; print(json.dumps({'data': 'x' * 1000000}))")
curl -s -X POST http://localhost:5000/api/collections/test_collection/documents \
    -H "Content-Type: application/json" \
    -d "$large_data" \
    -w "%{http_code}\n" -o /dev/null

check_server_health

echo

# 6. Test error handling
echo "6. Testing Error Handling"
echo "------------------------"

echo -n "GET non-existent endpoint: "
curl -s -o /dev/null -w "%{http_code}\n" http://localhost:5000/api/nonexistent

echo -n "POST with invalid JSON: "
curl -s -X POST http://localhost:5000/api/collections \
    -H "Content-Type: application/json" \
    -d '{invalid json}' \
    -w "%{http_code}\n" -o /dev/null

echo -n "GET non-existent file: "
curl -s -o /dev/null -w "%{http_code}\n" http://localhost:5000/nonexistent.html

check_server_health

echo

# 7. Test OPTIONS requests (CORS)
echo "7. Testing CORS Preflight"
echo "------------------------"

echo -n "OPTIONS request: "
curl -s -X OPTIONS http://localhost:5000/api/collections \
    -H "Origin: http://example.com" \
    -H "Access-Control-Request-Method: POST" \
    -w "%{http_code}\n" -o /dev/null

echo

# 8. Test keep-alive connections
echo "8. Testing Keep-Alive Connections"
echo "---------------------------------"

echo "Sending 5 requests on same connection..."
curl -s -w "Request 1: %{http_code} - %{time_total}s\n" -o /dev/null http://localhost:5000/api/health \
     -s -w "Request 2: %{http_code} - %{time_total}s\n" -o /dev/null http://localhost:5000/api/collections \
     -s -w "Request 3: %{http_code} - %{time_total}s\n" -o /dev/null http://localhost:5000/api/metrics \
     -s -w "Request 4: %{http_code} - %{time_total}s\n" -o /dev/null http://localhost:5000/api/health \
     -s -w "Request 5: %{http_code} - %{time_total}s\n" -o /dev/null http://localhost:5000/api/collections

check_server_health

echo

# 9. Test mixed workload
echo "9. Testing Mixed Workload"
echo "------------------------"

echo "Sending mixed API and static file requests concurrently..."
(
    for i in {1..5}; do
        curl -s http://localhost:5000/api/collections > /dev/null 2>&1 &
        curl -s http://localhost:5000/ > /dev/null 2>&1 &
        curl -s http://localhost:5000/js/app.js > /dev/null 2>&1 &
        curl -s http://localhost:5000/api/health > /dev/null 2>&1 &
    done
    wait
)

check_server_health

echo

# 10. Test connection limits
echo "10. Testing Connection Stress"
echo "-----------------------------"

echo "Opening 50 concurrent connections..."
for i in {1..50}; do
    (curl -s http://localhost:5000/api/health > /dev/null 2>&1) &
done
wait

check_server_health

echo

# Final health check
echo "=== Final Server Status ==="
if check_server_health; then
    echo "Server is still healthy after all tests!"
    
    # Get detailed metrics
    echo
    echo "Server metrics:"
    curl -s http://localhost:5000/api/health | jq '.metrics'
else
    echo "Server is not responding!"
fi

# Cleanup
echo
echo "Cleaning up test collection..."
curl -s -X DELETE http://localhost:5000/api/collections/test_collection > /dev/null 2>&1

echo
echo "=== Test Suite Complete ==="