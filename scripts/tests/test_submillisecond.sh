#!/bin/bash

echo "Testing for sub-millisecond response times"
echo "=========================================="

# Test health endpoint (no auth required)
echo -e "\n1. Testing /api/health (no auth):"
for i in {1..10}; do
    TIME=$(curl -s -o /dev/null -w "%{time_total}" http://localhost:5000/api/health)
    TIME_MS=$(echo "$TIME * 1000" | bc)
    echo "Request $i: ${TIME_MS}ms"
done

# Get auth token
echo -e "\n2. Getting auth token..."
LOGIN_RESPONSE=$(curl -s -X POST http://localhost:5000/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username": "admin", "password": "admin"}')

TOKEN=$(echo "$LOGIN_RESPONSE" | grep -o '"token":"[^"]*' | cut -d'"' -f4)

# Test authenticated endpoint with JWT cache
echo -e "\n3. Testing /api/collections (with JWT cache):"
for i in {1..10}; do
    TIME=$(curl -s -o /dev/null -w "%{time_total}" \
        -H "Authorization: Bearer $TOKEN" \
        http://localhost:5000/api/collections)
    TIME_MS=$(echo "$TIME * 1000" | bc)
    echo "Request $i: ${TIME_MS}ms"
done

# Test a simple document query
echo -e "\n4. Testing document query:"
for i in {1..10}; do
    TIME=$(curl -s -o /dev/null -w "%{time_total}" \
        http://localhost:5000/api/collections/test_collection/documents)
    TIME_MS=$(echo "$TIME * 1000" | bc)
    echo "Request $i: ${TIME_MS}ms"
done

echo -e "\nSub-millisecond = < 1.0ms"
echo "Current performance: Most requests are 1-3ms"