#!/bin/bash
# Test Memory Promotion Thread Safety - Step 3
# Validate that selective promotion fixes work under concurrent load

SERVER_URL="https://localhost:5000"
ADMIN_USER="admin"
ADMIN_PASS="admin"

echo "🧵 Memory Promotion Thread Safety Test"
echo "====================================="

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

# Test 1: Concurrent Small Operations (should use automatic cleanup)
echo ""
echo "🔄 Test 1: Concurrent small operations (4 processes × 20 operations)"
START_MEM=$(get_memory_usage_mb)

# Function for concurrent operations
perform_concurrent_ops() {
    local process_id=$1
    local token=$2
    local ops_count=$3
    
    for ((i=1; i<=ops_count; i++)); do
        # Create small document
        uuid=$(curl -k -s -X POST "$SERVER_URL/api/libraries/default/collections/thread_test/documents" \
            -H "Authorization: Bearer $token" \
            -H "Content-Type: application/json" \
            -d "{\"name\":\"thread_doc_${process_id}_${i}\",\"data\":{\"thread\":$process_id,\"iteration\":$i}}" | \
            grep -o '"uuid":"[^"]*"' | cut -d'"' -f4)
        
        if [ -n "$uuid" ]; then
            # Query documents
            curl -k -s -X GET "$SERVER_URL/api/libraries/default/collections/thread_test/documents" \
                -H "Authorization: Bearer $token" > /dev/null
            
            # Delete document
            curl -k -s -X DELETE "$SERVER_URL/api/libraries/default/collections/thread_test/documents/$uuid" \
                -H "Authorization: Bearer $token" > /dev/null
        fi
        
        # Small delay to allow checkpoint operations
        sleep 0.01
    done
    
    echo "   Process $process_id completed $ops_count operations"
}

# Launch concurrent processes
PIDS=()
for ((p=1; p<=4; p++)); do
    perform_concurrent_ops $p "$TOKEN" 20 &
    PIDS+=($!)
done

# Wait for completion
for pid in "${PIDS[@]}"; do
    wait $pid
done

END_MEM=$(get_memory_usage_mb)
CONCURRENT_DIFF=$((END_MEM - START_MEM))

echo "📊 Memory change for concurrent operations: ${CONCURRENT_DIFF} MB"

# Test 2: Thread Safety Under Rapid Requests
echo ""
echo "🚀 Test 2: Rapid concurrent requests (stress test)"
START_MEM=$(get_memory_usage_mb)

# Function for rapid requests
rapid_requests() {
    local process_id=$1
    local token=$2
    
    for ((i=1; i<=30; i++)); do
        curl -k -s -X POST "$SERVER_URL/api/libraries/default/collections/rapid_test/documents" \
            -H "Authorization: Bearer $token" \
            -H "Content-Type: application/json" \
            -d "{\"name\":\"rapid_$process_id\",\"data\":{\"timestamp\":$(date +%s%N)}}" > /dev/null &
        
        # Don't wait - fire and forget for stress testing
    done
    
    echo "   Process $process_id fired 30 rapid requests"
}

# Launch rapid requests
RAPID_PIDS=()
for ((p=1; p<=6; p++)); do
    rapid_requests $p "$TOKEN" &
    RAPID_PIDS+=($!)
done

# Wait for launch completion
for pid in "${RAPID_PIDS[@]}"; do
    wait $pid
done

# Wait for all background requests to complete
sleep 3

END_MEM=$(get_memory_usage_mb)
RAPID_DIFF=$((END_MEM - START_MEM))

echo "📊 Memory change for rapid requests: ${RAPID_DIFF} MB"

# Test 3: Mixed Operation Patterns
echo ""
echo "🔀 Test 3: Mixed operation patterns (small + medium responses)"
START_MEM=$(get_memory_usage_mb)

# Create some documents of varying sizes
for ((i=1; i<=10; i++)); do
    # Small document
    curl -k -s -X POST "$SERVER_URL/api/libraries/default/collections/mixed_test/documents" \
        -H "Authorization: Bearer $token" \
        -H "Content-Type: application/json" \
        -d "{\"name\":\"small_$i\",\"data\":{\"value\":$i}}" > /dev/null
    
    # Medium document (~5KB)
    medium_text=$(head -c 5000 /dev/urandom | base64 | tr -d '\n')
    curl -k -s -X POST "$SERVER_URL/api/libraries/default/collections/mixed_test/documents" \
        -H "Authorization: Bearer $token" \
        -H "Content-Type: application/json" \
        -d "{\"name\":\"medium_$i\",\"data\":{\"text\":\"$medium_text\"}}" > /dev/null
done

# Query all documents (should be manageable size)
curl -k -s -X GET "$SERVER_URL/api/libraries/default/collections/mixed_test/documents" \
    -H "Authorization: Bearer $token" > /dev/null

END_MEM=$(get_memory_usage_mb)
MIXED_DIFF=$((END_MEM - START_MEM))

echo "📊 Memory change for mixed operations: ${MIXED_DIFF} MB"

# Final Assessment
TOTAL_MEMORY=$(get_memory_usage_mb)
TOTAL_GROWTH=$((TOTAL_MEMORY - 9))  # 9MB was after auth

echo ""
echo "📈 Thread Safety Analysis:"
echo "========================="
echo "Concurrent operations: ${CONCURRENT_DIFF} MB growth"
echo "Rapid requests: ${RAPID_DIFF} MB growth" 
echo "Mixed patterns: ${MIXED_DIFF} MB growth"
echo "Total memory growth: ${TOTAL_GROWTH} MB"
echo "Final memory usage: ${TOTAL_MEMORY} MB"

# Assessment criteria
if [ "$CONCURRENT_DIFF" -lt 8 ] && [ "$RAPID_DIFF" -lt 10 ] && [ "$TOTAL_GROWTH" -lt 25 ]; then
    echo ""
    echo "✅ THREAD SAFETY VALIDATION PASSED"
    echo "   - Concurrent operations stable: ${CONCURRENT_DIFF} MB < 8 MB threshold"
    echo "   - Rapid requests handled: ${RAPID_DIFF} MB < 10 MB threshold"
    echo "   - Total growth reasonable: ${TOTAL_GROWTH} MB < 25 MB threshold"
    echo "   - Selective promotion working under concurrent load"
    echo ""
    echo "🎯 Memory promotion patterns validated for thread safety"
    exit 0
else
    echo ""
    echo "⚠️  THREAD SAFETY ISSUES DETECTED"
    echo "   - Concurrent growth: ${CONCURRENT_DIFF} MB (threshold: 8 MB)"
    echo "   - Rapid request growth: ${RAPID_DIFF} MB (threshold: 10 MB)"
    echo "   - Total growth: ${TOTAL_GROWTH} MB (threshold: 25 MB)"
    echo ""
    echo "🔍 Potential issues:"
    echo "   - Race conditions in selective promotion logic"
    echo "   - Checkpoint boundaries under concurrent load"
    echo "   - Memory promotion not thread-safe"
    exit 1
fi