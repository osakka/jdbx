#!/bin/bash

# Simple test to isolate the JWT issue

echo "=== Simple JWT Authentication Test ==="
echo

# Start fresh
echo "Starting server..."
cd /opt/jdbx
./build/jdbx_runtime.sh stop 2>/dev/null
sleep 2
./build/jdbx_runtime.sh start
sleep 3

# Login
echo "1. Login to get token..."
LOGIN_RESPONSE=$(curl -s -k -X POST https://localhost:5000/api/auth/login \
    -H "Content-Type: application/json" \
    -d '{"username":"admin","password":"admin123456789"}')

TOKEN=$(echo "$LOGIN_RESPONSE" | grep -o '"token":"[^"]*' | cut -d'"' -f4)

if [ -z "$TOKEN" ]; then
    echo "ERROR: No token received"
    echo "Response: $LOGIN_RESPONSE"
    exit 1
fi

echo "Got token: ${TOKEN:0:30}..."
echo

# Test unauthenticated endpoint first
echo "2. Testing /api/health (no auth required)..."
curl -s -k https://localhost:5000/api/health | jq . || echo "Failed"

echo
echo "3. Testing /api/libraries with token..."
RESPONSE=$(curl -s -k -w "\nHTTP_STATUS:%{http_code}" \
    -H "Authorization: Bearer $TOKEN" \
    https://localhost:5000/api/libraries)

HTTP_STATUS=$(echo "$RESPONSE" | grep "HTTP_STATUS:" | cut -d: -f2)
BODY=$(echo "$RESPONSE" | grep -v "HTTP_STATUS:")

echo "HTTP Status: $HTTP_STATUS"
echo "Response body:"
echo "$BODY" | jq . 2>/dev/null || echo "$BODY"

# Check if server is still running
echo
echo "4. Checking server status..."
if pgrep -f jdbxd > /dev/null; then
    echo "✅ Server is still running"
else
    echo "❌ Server crashed!"
    echo
    echo "Last 50 lines of log:"
    tail -50 /opt/jdbx/build/var/jdbxd.log
fi