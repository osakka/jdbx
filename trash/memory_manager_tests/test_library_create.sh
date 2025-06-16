#!/bin/bash

echo "=== Testing Library Creation ==="
echo

# Start fresh
cd /opt/jdbx
./build/jdbx_runtime.sh stop 2>/dev/null
sleep 2
./build/jdbx_runtime.sh start
sleep 3

# Login to get token
echo "1. Getting admin token..."
TOKEN=$(curl -s -k -X POST https://localhost:5000/api/auth/login \
    -H "Content-Type: application/json" \
    -d '{"username":"admin","password":"admin123456789"}' \
    | grep -o '"token":"[^"]*' | cut -d'"' -f4)

echo "Token: ${TOKEN:0:30}..."

# Try to create a library
echo
echo "2. Creating test library..."
RESPONSE=$(curl -s -k -X POST https://localhost:5000/api/libraries \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d '{"name":"test_library","description":"Test library"}' \
    -w "\nHTTP_STATUS:%{http_code}")

HTTP_STATUS=$(echo "$RESPONSE" | grep "HTTP_STATUS:" | cut -d: -f2)
BODY=$(echo "$RESPONSE" | grep -v "HTTP_STATUS:")

echo "HTTP Status: $HTTP_STATUS"
echo "Response:"
echo "$BODY" | jq . 2>/dev/null || echo "$BODY"

# Check server status
echo
echo "3. Checking server status..."
if pgrep -f jdbxd > /dev/null; then
    echo "✅ Server is still running"
else
    echo "❌ Server crashed!"
    echo
    echo "Last 30 lines of log:"
    tail -30 /opt/jdbx/build/var/jdbxd.log
fi