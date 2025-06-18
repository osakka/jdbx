#!/bin/bash

echo "🔍 Testing Large Document Issue"
echo "==============================="

# Get auth token
TOKEN=$(curl -s -k https://localhost:5000/api/auth/login \
    -H "Content-Type: application/json" \
    -d '{"username":"admin","password":"secure123456789"}' | jq -r '.token')

if [ "$TOKEN" = "null" ]; then
    echo "❌ Authentication failed"
    exit 1
fi

echo "✅ Authenticated successfully"

# Test 1: Small document (should work)
echo -e "\nTest 1: Small document (100 bytes)"
SMALL_DOC=$(python3 -c "import json; print(json.dumps({'data': 'x' * 50}))")
echo "Document size: ${#SMALL_DOC} bytes"

RESPONSE=$(curl -s -k -w "\nHTTP_CODE:%{http_code}" \
    -X POST https://localhost:5000/api/documents \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d "$SMALL_DOC")

HTTP_CODE=$(echo "$RESPONSE" | grep "HTTP_CODE:" | cut -d: -f2)
echo "Response: HTTP $HTTP_CODE"

# Test 2: Medium document (5KB) - Using different method
echo -e "\nTest 2: Medium document (5KB) - Using printf instead of curl -d"
MEDIUM_DOC=$(python3 -c "import json; print(json.dumps({'data': 'x' * 5000}))")
echo "Document size: ${#MEDIUM_DOC} bytes"

# Method A: Using curl -d (known to have issues)
echo "Method A: curl -d"
RESPONSE=$(curl -s -k -w "\nHTTP_CODE:%{http_code}" \
    -X POST https://localhost:5000/api/documents \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d "$MEDIUM_DOC" 2>&1)

HTTP_CODE=$(echo "$RESPONSE" | grep "HTTP_CODE:" | cut -d: -f2)
echo "Response: HTTP $HTTP_CODE"

# Method B: Using echo and pipe (avoids curl -d issue)
echo -e "\nMethod B: echo | curl"
RESPONSE=$(echo -n "$MEDIUM_DOC" | curl -s -k -w "\nHTTP_CODE:%{http_code}" \
    -X POST https://localhost:5000/api/documents \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -H "Content-Length: ${#MEDIUM_DOC}" \
    --data-binary @- 2>&1)

HTTP_CODE=$(echo "$RESPONSE" | grep "HTTP_CODE:" | cut -d: -f2)
echo "Response: HTTP $HTTP_CODE"

# Test 3: Check server logs
echo -e "\nRecent server errors:"
tail -5 /opt/jdbx/build/var/jdbxd.log | grep -E "(ERROR|INCOMPLETE)" || echo "No recent errors"