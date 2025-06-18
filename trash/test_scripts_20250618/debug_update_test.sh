#!/bin/bash
# Debug the document update issue

BASE_URL="https://localhost:5000"
ADMIN_USER="admin"
ADMIN_PASS="secure123456789"

echo "🔍 JDBX Document Update Debug Test"
echo "=================================="

# Get token
TOKEN=$(curl -s -k -X POST "$BASE_URL/api/auth/login" \
    -H "Content-Type: application/json" \
    -d "{\"username\":\"$ADMIN_USER\",\"password\":\"$ADMIN_PASS\"}" | \
    grep -o '"token":"[^"]*' | cut -d'"' -f4)

if [ -z "$TOKEN" ]; then
    echo "❌ Authentication failed"
    exit 1
fi
echo "✅ Authentication successful"

# Create a single document
echo "📝 Creating test document..."
CREATE_RESPONSE=$(curl -s -k -X POST "$BASE_URL/api/documents" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d '{"title":"Debug Test","content":"Original content","debug":true}')

echo "📋 Create response: $CREATE_RESPONSE"

UUID=$(echo "$CREATE_RESPONSE" | grep -o '"uuid":"[^"]*' | cut -d'"' -f4)
echo "🆔 Created UUID: $UUID"

if [ -z "$UUID" ]; then
    echo "❌ Failed to create document"
    exit 1
fi

# Try to get the document back to confirm it exists
echo ""
echo "🔍 Verifying document exists..."
GET_RESPONSE=$(curl -s -k -X GET "$BASE_URL/api/documents/$UUID" \
    -H "Authorization: Bearer $TOKEN" \
    -w "\nHTTP_CODE:%{http_code}")

GET_HTTP_CODE=$(echo "$GET_RESPONSE" | grep "HTTP_CODE:" | cut -d':' -f2)
GET_BODY=$(echo "$GET_RESPONSE" | sed '/HTTP_CODE:/,$d')

echo "📄 GET HTTP Code: $GET_HTTP_CODE"
echo "📄 GET Response: $GET_BODY"

if [ "$GET_HTTP_CODE" != "200" ]; then
    echo "❌ Document doesn't exist after creation!"
    echo "🔍 Trying collections API..."
    
    # Try querying via collections API
    QUERY_RESPONSE=$(curl -s -k -X GET "$BASE_URL/api/documents" \
        -H "Authorization: Bearer $TOKEN")
    echo "📋 All documents: $QUERY_RESPONSE"
    exit 1
fi

# Now try to update it
echo ""
echo "🔄 Attempting document update..."
UPDATE_RESPONSE=$(curl -s -k -X PUT "$BASE_URL/api/documents/$UUID" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d '{"title":"Updated Debug Test","content":"Updated content","debug":true,"updated":true}' \
    -w "\nHTTP_CODE:%{http_code}")

UPDATE_HTTP_CODE=$(echo "$UPDATE_RESPONSE" | grep "HTTP_CODE:" | cut -d':' -f2)
UPDATE_BODY=$(echo "$UPDATE_RESPONSE" | sed '/HTTP_CODE:/,$d')

echo "📄 UPDATE HTTP Code: $UPDATE_HTTP_CODE"
echo "📄 UPDATE Response: $UPDATE_BODY"

if [ "$UPDATE_HTTP_CODE" = "200" ]; then
    echo "✅ Update successful!"
    
    # Verify the update
    echo ""
    echo "🔍 Verifying update..."
    VERIFY_RESPONSE=$(curl -s -k -X GET "$BASE_URL/api/documents/$UUID" \
        -H "Authorization: Bearer $TOKEN")
    echo "📄 Updated document: $VERIFY_RESPONSE"
else
    echo "❌ Update failed!"
    
    # Check server logs
    echo ""
    echo "🔍 Recent server logs:"
    tail -10 /opt/jdbx/build/var/jdbxd.log | grep -E "(ERROR|virtual_update|storage_update|$UUID)"
fi

echo ""
echo "✅ Debug test completed!"