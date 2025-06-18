#!/bin/bash

BASE_URL="https://localhost:5000"

# Get auth token
TOKEN=$(curl -s -k "$BASE_URL/api/auth/login" \
    -H "Content-Type: application/json" \
    -d '{"username":"admin","password":"secure123456789"}' | jq -r '.token')

echo "Starting RAPID test (no delays)..."

# Test 1: Rapid fire 20 requests with no delay
echo "Test 1: 20 rapid requests, no delay"
for i in {1..20}; do
    curl -s -k -X POST "$BASE_URL/api/documents" \
        -H "Authorization: Bearer $TOKEN" \
        -H "Content-Type: application/json" \
        -d "{\"id\":$i,\"rapid\":true}" \
        -o /dev/null &
done

# Wait for all background jobs
wait

if ps -p $(cat /opt/jdbx/build/var/jdbxd.pid 2>/dev/null) > /dev/null 2>&1; then
    echo "✅ Server survived 20 concurrent requests"
else
    echo "❌ Server crashed after concurrent requests!"
    tail -20 /opt/jdbx/build/var/jdbxd.log
    exit 1
fi

# Test 2: Sequential rapid fire (like discovery test)
echo -e "\nTest 2: 50 sequential rapid requests"
for i in {1..50}; do
    echo -n "."
    curl -s -k -X POST "$BASE_URL/api/documents" \
        -H "Authorization: Bearer $TOKEN" \
        -H "Content-Type: application/json" \
        -d "{\"id\":$i,\"seq\":true}" \
        -w "" -o /dev/null || echo -n "x"
done
echo ""

if ps -p $(cat /opt/jdbx/build/var/jdbxd.pid 2>/dev/null) > /dev/null 2>&1; then
    echo "✅ Server survived sequential rapid requests"
else
    echo "❌ Server crashed during sequential requests!"
    tail -20 /opt/jdbx/build/var/jdbxd.log
fi