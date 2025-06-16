#!/bin/bash

# Test script to verify JWT authentication fix

echo "=== JWT Authentication Fix Test ==="
echo

# Start the server
echo "Starting JDBX server..."
cd /opt/jdbx
./build/jdbx_runtime.sh stop 2>/dev/null
sleep 2
./build/jdbx_runtime.sh start
sleep 3

# Check if server started
if ! pgrep -f jdbxd > /dev/null; then
    echo "ERROR: Server failed to start"
    exit 1
fi

echo "Server started successfully"
echo

# Login to get JWT token
echo "1. Testing login to get JWT token..."
LOGIN_RESPONSE=$(curl -s -k -X POST https://localhost:5000/api/auth/login \
    -H "Content-Type: application/json" \
    -d '{"username":"admin","password":"admin123"}')

if [ $? -ne 0 ]; then
    echo "ERROR: Login request failed"
    ./build/jdbx_runtime.sh stop
    exit 1
fi

# Extract token
TOKEN=$(echo "$LOGIN_RESPONSE" | grep -o '"token":"[^"]*' | cut -d'"' -f4)

if [ -z "$TOKEN" ]; then
    echo "ERROR: No token received from login"
    echo "Response: $LOGIN_RESPONSE"
    ./build/jdbx_runtime.sh stop
    exit 1
fi

echo "Successfully received JWT token: ${TOKEN:0:20}..."
echo

# Test authenticated request to /api/libraries multiple times
echo "2. Testing authenticated requests to /api/libraries (10 sequential requests)..."
FAILURES=0

for i in {1..10}; do
    echo -n "Request $i: "
    RESPONSE=$(curl -s -k -w "\nHTTP_STATUS:%{http_code}" \
        -H "Authorization: Bearer $TOKEN" \
        https://localhost:5000/api/libraries)
    
    HTTP_STATUS=$(echo "$RESPONSE" | grep "HTTP_STATUS:" | cut -d: -f2)
    BODY=$(echo "$RESPONSE" | grep -v "HTTP_STATUS:")
    
    if [ "$HTTP_STATUS" = "200" ]; then
        echo "SUCCESS (HTTP 200)"
    else
        echo "FAILED (HTTP $HTTP_STATUS)"
        echo "Response body: $BODY"
        ((FAILURES++))
    fi
    
    # Check if server is still running
    if ! pgrep -f jdbxd > /dev/null; then
        echo "ERROR: Server crashed after request $i!"
        exit 1
    fi
done

echo
echo "3. Testing concurrent requests (5 parallel requests)..."
for i in {1..5}; do
    curl -s -k -H "Authorization: Bearer $TOKEN" https://localhost:5000/api/libraries > /tmp/jwt_test_$i.out 2>&1 &
done

# Wait for all background jobs
wait

# Check results
CONCURRENT_FAILURES=0
for i in {1..5}; do
    if grep -q "libraries" /tmp/jwt_test_$i.out; then
        echo "Concurrent request $i: SUCCESS"
    else
        echo "Concurrent request $i: FAILED"
        cat /tmp/jwt_test_$i.out
        ((CONCURRENT_FAILURES++))
    fi
    rm -f /tmp/jwt_test_$i.out
done

# Check if server is still running
if ! pgrep -f jdbxd > /dev/null; then
    echo
    echo "ERROR: Server crashed during concurrent requests!"
    exit 1
fi

# Stop the server
echo
echo "Stopping server..."
./build/jdbx_runtime.sh stop

# Summary
echo
echo "=== Test Summary ==="
echo "Sequential request failures: $FAILURES/10"
echo "Concurrent request failures: $CONCURRENT_FAILURES/5"

if [ $FAILURES -eq 0 ] && [ $CONCURRENT_FAILURES -eq 0 ]; then
    echo "✅ ALL TESTS PASSED - JWT authentication fix verified!"
    exit 0
else
    echo "❌ Some tests failed"
    exit 1
fi