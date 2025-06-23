#!/bin/bash
# Test HTTP response promotion fix specifically
# Check if selective promotion is working correctly

SERVER_URL="https://localhost:5000"
ADMIN_USER="admin"
ADMIN_PASS="admin"

echo "🔬 HTTP Response Promotion Test"
echo "=============================="

# Find JDBX process
JDBX_PID=$(pgrep -f jdbxd | head -1)
if [ -z "$JDBX_PID" ]; then
    echo "❌ JDBX server process not found"
    exit 1
fi

echo "📊 Monitoring JDBX process PID: $JDBX_PID"

# Function to get memory usage in MB
get_memory_usage_mb() {
    local kb=$(grep "VmRSS" /proc/$JDBX_PID/status | awk '{print $2}')
    echo $((kb / 1024))
}

echo "📊 Initial memory: $(get_memory_usage_mb) MB"

# Get authentication token
TOKEN=$(curl -k -s -X POST "$SERVER_URL/api/auth/login" \
    -H "Content-Type: application/json" \
    -d "{\"username\":\"$ADMIN_USER\",\"password\":\"$ADMIN_PASS\"}" | \
    grep -o '"token":"[^"]*"' | cut -d'"' -f4)

if [ -z "$TOKEN" ]; then
    echo "❌ Authentication failed"
    exit 1
fi

echo "✅ Authentication successful"
echo "📊 Memory after auth: $(get_memory_usage_mb) MB"

# Test 1: Small JSON responses (should NOT be promoted)
echo ""
echo "📝 Test 1: Small JSON responses (should use checkpoint cleanup)"
START_MEM=$(get_memory_usage_mb)

for i in {1..50}; do
    # Create small document (typical API response ~200 bytes)
    curl -k -s -X POST "$SERVER_URL/api/libraries/default/collections/test_docs/documents" \
        -H "Authorization: Bearer $TOKEN" \
        -H "Content-Type: application/json" \
        -d "{\"name\":\"small_doc_$i\",\"data\":{\"value\":$i}}" > /dev/null
    
    # Query (small JSON response)
    curl -k -s -X GET "$SERVER_URL/api/libraries/default/collections/test_docs/documents" \
        -H "Authorization: Bearer $TOKEN" > /dev/null
done

END_MEM=$(get_memory_usage_mb)
SMALL_DIFF=$((END_MEM - START_MEM))

echo "📊 Memory change for 100 small operations: ${SMALL_DIFF} MB"

# Test 2: Large responses (should be promoted)
echo ""
echo "📝 Test 2: Large responses (should be selectively promoted)"
START_MEM=$(get_memory_usage_mb)

# Create large document (>64KB response)
LARGE_TEXT=$(head -c 70000 /dev/urandom | base64 | tr -d '\n')
curl -k -s -X POST "$SERVER_URL/api/libraries/default/collections/test_docs/documents" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d "{\"name\":\"large_doc\",\"data\":{\"large_text\":\"$LARGE_TEXT\"}}" > /dev/null

END_MEM=$(get_memory_usage_mb)
LARGE_DIFF=$((END_MEM - START_MEM))

echo "📊 Memory change for 1 large operation: ${LARGE_DIFF} MB"

# Test 3: Static file request (should check content-type promotion)
echo ""
echo "📝 Test 3: Static file requests (content-type based promotion)"
START_MEM=$(get_memory_usage_mb)

for i in {1..10}; do
    curl -k -s -X GET "$SERVER_URL/" \
        -H "Authorization: Bearer $TOKEN" > /dev/null
done

END_MEM=$(get_memory_usage_mb)
STATIC_DIFF=$((END_MEM - START_MEM))

echo "📊 Memory change for 10 static file requests: ${STATIC_DIFF} MB"

# Analysis
echo ""
echo "📈 Response Promotion Analysis:"
echo "==============================="
echo "Small JSON responses: ${SMALL_DIFF} MB growth (should be minimal)"
echo "Large response: ${LARGE_DIFF} MB growth (expected for promoted content)"
echo "Static files: ${STATIC_DIFF} MB growth (depends on file size)"

TOTAL_MEMORY=$(get_memory_usage_mb)
echo "Total memory usage: ${TOTAL_MEMORY} MB"

# Assessment
if [ "$SMALL_DIFF" -lt 5 ]; then
    echo "✅ Small response promotion fix WORKING - minimal memory growth"
else
    echo "❌ Small response promotion fix FAILED - excessive memory growth: ${SMALL_DIFF} MB"
fi

if [ "$TOTAL_MEMORY" -lt 50 ]; then
    echo "✅ Overall memory usage reasonable: ${TOTAL_MEMORY} MB"
else
    echo "⚠️  Total memory usage concerning: ${TOTAL_MEMORY} MB"
fi

echo ""
echo "🔍 If memory leaks persist, other sources to investigate:"
echo "   - Database query promotion (every query promoted permanently)"
echo "   - JSON deep copy promotion in API handlers"
echo "   - JWT cache recursive promotion"
echo "   - SSL connection promotion"