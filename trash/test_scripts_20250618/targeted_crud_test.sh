#!/bin/bash
# Targeted CRUD and Auth investigation test

BASE_URL="https://localhost:5000"
ADMIN_USER="admin"
ADMIN_PASS="secure123456789"

echo "🔍 JDBX Targeted CRUD & Auth Investigation"
echo "========================================"

# Get token
echo "🔐 Getting authentication token..."
TOKEN_RESPONSE=$(curl -s -k -X POST "$BASE_URL/api/auth/login" \
    -H "Content-Type: application/json" \
    -d "{\"username\":\"$ADMIN_USER\",\"password\":\"$ADMIN_PASS\"}")

echo "📋 Full login response: $TOKEN_RESPONSE"

TOKEN=$(echo "$TOKEN_RESPONSE" | grep -o '"token":"[^"]*' | cut -d'"' -f4)

if [ -z "$TOKEN" ]; then
    echo "❌ Authentication failed"
    echo "🔍 Checking server logs for auth issues..."
    tail -10 /opt/jdbx/build/var/jdbxd.log | grep -i "auth\|login\|error"
    exit 1
fi
echo "✅ Authentication successful, token: ${TOKEN:0:20}..."

# Test 1: Simple Document Creation
echo ""
echo "🧪 Test 1: Simple Document Creation"
echo "===================================="

for i in {1..5}; do
    echo "📝 Creating document $i..."
    
    RESPONSE=$(curl -s -k -X POST "$BASE_URL/api/documents" \
        -H "Authorization: Bearer $TOKEN" \
        -H "Content-Type: application/json" \
        -d "{\"title\":\"Test Doc $i\",\"content\":\"Simple content for doc $i\"}" \
        -w "\nHTTP_CODE:%{http_code}\nTIME:%{time_total}")
    
    HTTP_CODE=$(echo "$RESPONSE" | grep "HTTP_CODE:" | cut -d':' -f2)
    TIME=$(echo "$RESPONSE" | grep "TIME:" | cut -d':' -f2)
    BODY=$(echo "$RESPONSE" | sed '/HTTP_CODE:/,$d')
    
    if [ "$HTTP_CODE" = "201" ]; then
        echo "✅ Document $i created successfully (${TIME}s)"
        # Extract UUID for later operations
        UUID=$(echo "$BODY" | grep -o '"uuid":"[^"]*' | cut -d'"' -f4)
        echo "🆔 UUID: $UUID"
        
        # Store for later use
        echo "$UUID" >> /tmp/created_docs.txt
    else
        echo "❌ Document $i failed - HTTP $HTTP_CODE (${TIME}s)"
        echo "📄 Response body: $BODY"
        
        # Check server logs
        echo "🔍 Recent server logs:"
        tail -5 /opt/jdbx/build/var/jdbxd.log
        break
    fi
done

# Test 2: Document Query
echo ""
echo "🧪 Test 2: Document Query"
echo "========================="

QUERY_RESPONSE=$(curl -s -k -X GET "$BASE_URL/api/documents" \
    -H "Authorization: Bearer $TOKEN" \
    -w "\nHTTP_CODE:%{http_code}\nTIME:%{time_total}")

QUERY_HTTP_CODE=$(echo "$QUERY_RESPONSE" | grep "HTTP_CODE:" | cut -d':' -f2)
QUERY_TIME=$(echo "$QUERY_RESPONSE" | grep "TIME:" | cut -d':' -f2)
QUERY_BODY=$(echo "$QUERY_RESPONSE" | sed '/HTTP_CODE:/,$d')

if [ "$QUERY_HTTP_CODE" = "200" ]; then
    echo "✅ Query successful (${QUERY_TIME}s)"
    DOC_COUNT=$(echo "$QUERY_BODY" | grep -o '"count":[0-9]*' | cut -d':' -f2)
    echo "📊 Documents found: $DOC_COUNT"
else
    echo "❌ Query failed - HTTP $QUERY_HTTP_CODE (${QUERY_TIME}s)"
    echo "📄 Response: $QUERY_BODY"
fi

# Test 3: Document Update (if we have UUIDs)
echo ""
echo "🧪 Test 3: Document Update"
echo "=========================="

if [ -f /tmp/created_docs.txt ]; then
    FIRST_UUID=$(head -1 /tmp/created_docs.txt)
    if [ -n "$FIRST_UUID" ]; then
        echo "🔄 Updating document: $FIRST_UUID"
        
        UPDATE_RESPONSE=$(curl -s -k -X PUT "$BASE_URL/api/documents/$FIRST_UUID" \
            -H "Authorization: Bearer $TOKEN" \
            -H "Content-Type: application/json" \
            -d "{\"title\":\"Updated Document\",\"content\":\"Updated content\",\"updated\":true}" \
            -w "\nHTTP_CODE:%{http_code}\nTIME:%{time_total}")
        
        UPDATE_HTTP_CODE=$(echo "$UPDATE_RESPONSE" | grep "HTTP_CODE:" | cut -d':' -f2)
        UPDATE_TIME=$(echo "$UPDATE_RESPONSE" | grep "TIME:" | cut -d':' -f2)
        UPDATE_BODY=$(echo "$UPDATE_RESPONSE" | sed '/HTTP_CODE:/,$d')
        
        if [ "$UPDATE_HTTP_CODE" = "200" ]; then
            echo "✅ Update successful (${UPDATE_TIME}s)"
        else
            echo "❌ Update failed - HTTP $UPDATE_HTTP_CODE (${UPDATE_TIME}s)"
            echo "📄 Response: $UPDATE_BODY"
        fi
    else
        echo "⚠️ No UUID available for update test"
    fi
else
    echo "⚠️ No documents created for update test"
fi

# Test 4: Authentication Stress (smaller scale)
echo ""
echo "🧪 Test 4: Authentication Validation"
echo "===================================="

# Valid auth test
for i in {1..3}; do
    echo "🔑 Auth test $i..."
    AUTH_RESPONSE=$(curl -s -k -X POST "$BASE_URL/api/auth/login" \
        -H "Content-Type: application/json" \
        -d "{\"username\":\"$ADMIN_USER\",\"password\":\"$ADMIN_PASS\"}" \
        -w "\nHTTP_CODE:%{http_code}")
    
    AUTH_HTTP_CODE=$(echo "$AUTH_RESPONSE" | grep "HTTP_CODE:" | cut -d':' -f2)
    
    if [ "$AUTH_HTTP_CODE" = "200" ]; then
        echo "✅ Auth test $i successful"
    else
        echo "❌ Auth test $i failed - HTTP $AUTH_HTTP_CODE"
        echo "📄 Response: $(echo "$AUTH_RESPONSE" | sed '/HTTP_CODE:/,$d')"
        break
    fi
done

# Invalid auth test
echo "🚫 Testing invalid credentials..."
INVALID_RESPONSE=$(curl -s -k -X POST "$BASE_URL/api/auth/login" \
    -H "Content-Type: application/json" \
    -d '{"username":"invalid","password":"wrong"}' \
    -w "\nHTTP_CODE:%{http_code}")

INVALID_HTTP_CODE=$(echo "$INVALID_RESPONSE" | grep "HTTP_CODE:" | cut -d':' -f2)

if [ "$INVALID_HTTP_CODE" = "401" ]; then
    echo "✅ Invalid auth correctly rejected"
else
    echo "❌ Invalid auth test unexpected result - HTTP $INVALID_HTTP_CODE"
fi

# Final Status
echo ""
echo "🎯 Test Summary:"
echo "================"
echo "📝 Document Creation: Check individual results above"
echo "🔍 Document Query: HTTP $QUERY_HTTP_CODE"
echo "🔄 Document Update: HTTP $UPDATE_HTTP_CODE"
echo "🔑 Authentication: Check individual results above"

# Cleanup
rm -f /tmp/created_docs.txt

echo ""
echo "🔍 Recent Server Activity:"
echo "=========================="
tail -15 /opt/jdbx/build/var/jdbxd.log | grep -E "(ERROR|WARNING|INFO.*jwt|INFO.*document)"

echo ""
echo "✅ Targeted investigation completed!"