#!/bin/bash

echo "Testing JSONdb Optimizations"
echo "============================"

# Test 1: Basic health check
echo -e "\n1. Testing basic connectivity..."
curl -s http://localhost:5000/api/health | jq .

# Test 2: Login and get token
echo -e "\n2. Testing authentication..."
TOKEN=$(curl -s -X POST http://localhost:5000/api/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"admin"}' | jq -r .token)

if [ -z "$TOKEN" ]; then
    echo "Failed to get authentication token"
    exit 1
fi

echo "Got token: ${TOKEN:0:20}..."

# Test 3: Test deep copy optimization (create documents)
echo -e "\n3. Testing document operations (deep copy optimization)..."
curl -s -X POST http://localhost:5000/api/collections/test_collection/documents \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "name": "Test Document",
    "data": {
      "nested": {
        "array": [1, 2, 3, 4, 5],
        "object": {
          "key1": "value1",
          "key2": "value2"
        }
      }
    }
  }' | jq .

# Test 4: Test buffer pool (multiple small requests)
echo -e "\n4. Testing buffer pool with multiple requests..."
for i in {1..10}; do
    curl -s http://localhost:5000/api/collections \
      -H "Authorization: Bearer $TOKEN" > /dev/null
    echo -n "."
done
echo " Done"

# Test 5: Test static file serving (if available)
echo -e "\n5. Testing static file serving..."
curl -s -o /dev/null -w "Static file response time: %{time_total}s\n" \
  http://localhost:5000/index.html

# Test 6: Check memory usage
echo -e "\n6. Checking server memory usage..."
PID=$(cat /opt/jsondb/build/var/jsondb.pid)
ps aux | grep "^[^ ]*[ ]*$PID" | awk '{print "Memory usage: " $6 " KB (" $4 "% of system)"}'

echo -e "\nOptimization tests completed!"