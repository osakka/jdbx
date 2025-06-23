#!/bin/bash
# JDBX Minimal Memory Test - Debug memory issues
# Tests minimal operations to isolate memory leak source

SERVER_URL="https://localhost:5000"
ADMIN_USER="admin"
ADMIN_PASS="admin"

echo "🔍 JDBX Minimal Memory Test"
echo "=========================="

# Find JDBX process
JDBX_PID=$(pgrep -f jdbxd | head -1)
if [ -z "$JDBX_PID" ]; then
    echo "❌ JDBX server process not found"
    exit 1
fi

echo "📊 Monitoring JDBX process PID: $JDBX_PID"

# Function to get memory usage in KB
get_memory_usage() {
    if [ -f "/proc/$JDBX_PID/status" ]; then
        grep "VmRSS" /proc/$JDBX_PID/status | awk '{print $2}'
    else
        echo "0"
    fi
}

# Function to get memory usage in MB
get_memory_usage_mb() {
    local kb=$(get_memory_usage)
    echo $((kb / 1024))
}

echo "📊 Initial memory: $(get_memory_usage_mb) MB"

# Test 1: Just authentication
echo ""
echo "🔐 Test 1: Authentication only"
TOKEN=$(curl -k -s -X POST "$SERVER_URL/api/auth/login" \
    -H "Content-Type: application/json" \
    -d "{\"username\":\"$ADMIN_USER\",\"password\":\"$ADMIN_PASS\"}" | \
    grep -o '"token":"[^"]*"' | cut -d'"' -f4)

if [ -n "$TOKEN" ]; then
    echo "✅ Authentication successful"
    echo "📊 Memory after auth: $(get_memory_usage_mb) MB"
else
    echo "❌ Authentication failed"
    exit 1
fi

# Test 2: Single document creation
echo ""
echo "📝 Test 2: Single document creation"
DOC_RESPONSE=$(curl -k -s -X POST "$SERVER_URL/api/libraries/default/collections/test_docs/documents" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d '{"name":"test_doc_1","data":{"value":123,"text":"hello world"}}')

if echo "$DOC_RESPONSE" | grep -q '"uuid"'; then
    echo "✅ Document created successfully"
    UUID=$(echo "$DOC_RESPONSE" | grep -o '"uuid":"[^"]*"' | cut -d'"' -f4)
    echo "📊 Memory after creation: $(get_memory_usage_mb) MB"
else
    echo "❌ Document creation failed"
    echo "Response: $DOC_RESPONSE"
fi

# Test 3: Query documents
echo ""
echo "📋 Test 3: Query documents"
QUERY_RESPONSE=$(curl -k -s -X GET "$SERVER_URL/api/libraries/default/collections/test_docs/documents" \
    -H "Authorization: Bearer $TOKEN")

if echo "$QUERY_RESPONSE" | grep -q '"documents"'; then
    echo "✅ Query successful"
    echo "📊 Memory after query: $(get_memory_usage_mb) MB"
else
    echo "❌ Query failed"
    echo "Response: $QUERY_RESPONSE"
fi

# Test 4: Multiple operations loop
echo ""
echo "🔄 Test 4: 20 create/query/delete cycles"
START_MEMORY=$(get_memory_usage_mb)

for i in {1..20}; do
    # Create
    DOC_RESPONSE=$(curl -k -s -X POST "$SERVER_URL/api/libraries/default/collections/test_docs/documents" \
        -H "Authorization: Bearer $TOKEN" \
        -H "Content-Type: application/json" \
        -d "{\"name\":\"test_doc_$i\",\"data\":{\"value\":$i,\"text\":\"iteration $i\"}}")
    
    if echo "$DOC_RESPONSE" | grep -q '"uuid"'; then
        UUID=$(echo "$DOC_RESPONSE" | grep -o '"uuid":"[^"]*"' | cut -d'"' -f4)
        
        # Query
        curl -k -s -X GET "$SERVER_URL/api/libraries/default/collections/test_docs/documents" \
            -H "Authorization: Bearer $TOKEN" > /dev/null
        
        # Delete
        curl -k -s -X DELETE "$SERVER_URL/api/libraries/default/collections/test_docs/documents/$UUID" \
            -H "Authorization: Bearer $TOKEN" > /dev/null
        
        if [ $((i % 5)) -eq 0 ]; then
            echo "   Iteration $i/20: $(get_memory_usage_mb) MB"
        fi
    else
        echo "❌ Failed at iteration $i"
        break
    fi
done

END_MEMORY=$(get_memory_usage_mb)
MEMORY_DIFF=$((END_MEMORY - START_MEMORY))

echo ""
echo "📈 Memory Analysis:"
echo "=================="
echo "Start Memory: ${START_MEMORY} MB"
echo "End Memory: ${END_MEMORY} MB"
echo "Memory Change: ${MEMORY_DIFF} MB"

if [ "$MEMORY_DIFF" -gt 10 ]; then
    echo "⚠️  MEMORY LEAK DETECTED: ${MEMORY_DIFF} MB growth"
    echo ""
    echo "🔍 Investigating potential causes:"
    
    # Check if it's related to document storage
    echo "   - Document storage: checkpoint-based allocation issue?"
    echo "   - JSON processing: deep copy or parsing leaks?"
    echo "   - HTTP responses: response promotion issue?"
    echo "   - SSL connections: connection lifecycle issue?"
    
    echo ""
    echo "💡 Recommendations:"
    echo "   1. Review checkpoint memory promotion in document handlers"
    echo "   2. Check JSON object lifecycle in API responses"
    echo "   3. Verify HTTP response memory management"
    echo "   4. Validate SSL connection cleanup"
    
    exit 1
else
    echo "✅ MEMORY STABLE: No significant memory leaks detected"
    echo ""
    echo "🎯 Phase 1.1 Status: Memory management appears stable for minimal operations"
    echo "   The previous high concurrent load may have triggered a specific leak pattern"
    echo "   Recommend investigating concurrent memory promotion patterns"
    exit 0
fi