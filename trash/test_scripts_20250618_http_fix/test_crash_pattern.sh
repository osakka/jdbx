#!/bin/bash
set -e

BASE_URL="https://localhost:5000"

# Get auth token
echo "Authenticating..."
TOKEN=$(curl -s -k "$BASE_URL/api/auth/login" \
    -H "Content-Type: application/json" \
    -d '{"username":"admin","password":"secure123456789"}' | jq -r '.token')

echo "Token: ${TOKEN:0:20}..."

# Test 1: Single medium document
echo -e "\nTest 1: Single 5KB document"
JSON=$(python3 -c "import json; print(json.dumps({'key': 'x' * 5000}))")
echo "JSON length: ${#JSON} bytes"

curl -v -k -X POST "$BASE_URL/api/documents" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d "$JSON" 2>&1 | grep -E "(Content-Length:|HTTP/|< )"

# Check if server is still running
sleep 1
if ps -p $(cat /opt/jdbx/build/var/jdbxd.pid) > /dev/null; then
    echo "✅ Server still running after 5KB test"
else
    echo "❌ Server crashed after 5KB test"
    exit 1
fi

# Test 2: Rapid small requests
echo -e "\nTest 2: 10 rapid small requests"
for i in {1..10}; do
    echo -n "Request $i: "
    curl -s -k -X POST "$BASE_URL/api/documents" \
        -H "Authorization: Bearer $TOKEN" \
        -H "Content-Type: application/json" \
        -d '{"test":"rapid","seq":'$i'}' \
        -w "HTTP %{http_code}\n" -o /dev/null || echo "FAILED"
    
    # Check server after each request
    if ! ps -p $(cat /opt/jdbx/build/var/jdbxd.pid) > /dev/null; then
        echo "❌ Server crashed after request $i"
        exit 1
    fi
done

echo "✅ Server survived all tests"