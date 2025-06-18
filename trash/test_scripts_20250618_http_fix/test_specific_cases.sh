#!/bin/bash

BASE_URL="https://localhost:5000"

# Get auth token
TOKEN=$(curl -s -k "$BASE_URL/api/auth/login" \
    -H "Content-Type: application/json" \
    -d '{"username":"admin","password":"secure123456789"}' | jq -r '.token')

echo "Testing specific cases that fail in discovery test..."

# Test 1: 5KB document (same as discovery test)
echo -e "\nTest 1: 5KB document"
MEDIUM_CONTENT=$(head -c 5000 < /dev/zero | tr '\0' 'A')
echo "Content length: ${#MEDIUM_CONTENT}"
JSON="{\"title\":\"Medium Document\",\"content\":\"$MEDIUM_CONTENT\"}"
echo "JSON length: ${#JSON}"

RESPONSE=$(curl -v -k -X POST "$BASE_URL/api/documents" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d "$JSON" 2>&1)

echo "$RESPONSE" | grep -E "(< HTTP|Content-Length:|400 Bad Request)"

# Check server
if ps -p $(cat /opt/jdbx/build/var/jdbxd.pid 2>/dev/null) > /dev/null 2>&1; then
    echo "✅ Server still running"
else
    echo "❌ Server crashed!"
    exit 1
fi

# Test 2: Check the actual bytes sent vs Content-Length
echo -e "\nTest 2: Checking byte count mismatch"
echo "$JSON" > /tmp/test_payload.json
ACTUAL_SIZE=$(stat -c%s /tmp/test_payload.json)
echo "Actual file size: $ACTUAL_SIZE bytes"

# Use curl with file to ensure exact bytes
curl -v -k -X POST "$BASE_URL/api/documents" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d @/tmp/test_payload.json 2>&1 | grep -E "(Content-Length:|< HTTP|400 Bad)"