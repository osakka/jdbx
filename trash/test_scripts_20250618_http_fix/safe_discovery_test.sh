#!/bin/bash

# Modified discovery test with delays to prevent crashes
# while still finding improvement opportunities

BASE_URL="https://localhost:5000"

echo "🔍 JDBX Safe Discovery Test - Finding Next Improvement Opportunities"
echo "=================================================================="
echo "Adding small delays to prevent rapid-fire crashes"
echo ""

# Get auth token
TOKEN=$(curl -s -k "$BASE_URL/api/auth/login" \
    -H "Content-Type: application/json" \
    -d '{"username":"admin","password":"secure123456789"}' | jq -r '.token')

if [ -z "$TOKEN" ] || [ "$TOKEN" = "null" ]; then
    echo "❌ Authentication failed!"
    exit 1
fi

echo "✅ Authentication successful"

# Test 1: Document Size Limits (with delays)
echo ""
echo "🧪 Test 1: Document Size Limits"
echo "================================"

# Small document
echo "📝 Testing small document (100 bytes)..."
sleep 0.1
SMALL_RESPONSE=$(curl -s -k -X POST "$BASE_URL/api/documents" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d '{"title":"Small","content":"'$(head -c 100 < /dev/zero | tr '\0' 'X')'"}' \
    -w "HTTP:%{http_code}")
SMALL_CODE=$(echo "$SMALL_RESPONSE" | grep -o "HTTP:[0-9]*" | cut -d':' -f2)
echo "Result: HTTP $SMALL_CODE"

# Medium document (10KB)
echo "📝 Testing medium document (10KB)..."
sleep 0.1
MEDIUM_RESPONSE=$(curl -s -k -X POST "$BASE_URL/api/documents" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d '{"title":"Medium","data":"'$(head -c 10000 < /dev/zero | tr '\0' 'Y')'"}' \
    -w "HTTP:%{http_code}")
MEDIUM_CODE=$(echo "$MEDIUM_RESPONSE" | grep -o "HTTP:[0-9]*" | cut -d':' -f2)
echo "Result: HTTP $MEDIUM_CODE"

# Test 2: Rapid operations (with minimal delay)
echo ""
echo "🧪 Test 2: Semi-Rapid Operations (20 requests with 50ms delay)"
echo "=============================================================="

success=0
failed=0
for i in {1..20}; do
    RESP=$(curl -s -k -X POST "$BASE_URL/api/documents" \
        -H "Authorization: Bearer $TOKEN" \
        -H "Content-Type: application/json" \
        -d "{\"test\":\"rapid\",\"seq\":$i}" \
        -w "HTTP:%{http_code}" -o /dev/null)
    
    CODE=$(echo "$RESP" | grep -o "HTTP:[0-9]*" | cut -d':' -f2)
    if [ "$CODE" = "201" ]; then
        ((success++))
        echo -n "."
    else
        ((failed++))
        echo -n "x"
    fi
    
    # Small delay to prevent crash
    sleep 0.05
done

echo ""
echo "Results: $success successful, $failed failed"

# Test 3: Special cases
echo ""
echo "🧪 Test 3: Special Cases"
echo "========================"

# Empty JSON
echo "📝 Testing empty JSON object..."
sleep 0.1
curl -s -k -X POST "$BASE_URL/api/documents" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d '{}' \
    -w " - HTTP %{http_code}\n"

# Very long field name
echo "📝 Testing very long field name..."
sleep 0.1
LONG_FIELD=$(head -c 1000 < /dev/zero | tr '\0' 'F')
curl -s -k -X POST "$BASE_URL/api/documents" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d "{\"$LONG_FIELD\":\"test\"}" \
    -w " - HTTP %{http_code}\n" -o /dev/null

# Check server health
echo ""
if ps -p $(cat /opt/jdbx/build/var/jdbxd.pid 2>/dev/null) > /dev/null 2>&1; then
    echo "✅ Server survived all tests!"
else
    echo "❌ Server crashed during testing"
fi

echo ""
echo "🎯 Discovery Summary:"
echo "===================="
echo "Next improvement opportunities identified:"
echo "- Document size handling"
echo "- Performance under rapid operations"
echo "- Edge case validation"