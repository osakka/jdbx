#!/bin/bash
# Test Skiplist Brain Surgery Performance Impact

SERVER_URL="https://localhost:5000"

echo "🧠⚙️ SKIPLIST BRAIN SURGERY PERFORMANCE TEST"
echo "============================================="

# Get fresh token
TOKEN=$(curl -k -s -X POST "$SERVER_URL/api/auth/login" \
    -H "Content-Type: application/json" \
    -d '{"username":"admin","password":"admin"}' | \
    grep -o '"token":"[^"]*"' | cut -d'"' -f4)

if [ -z "$TOKEN" ]; then
    echo "❌ Authentication failed"
    exit 1
fi

echo "✅ Authentication successful"

# Function to measure operation time in microseconds
measure_time_us() {
    local operation=$1
    local command=$2
    echo -n "⚡ $operation: "
    
    start_time=$(date +%s%N)
    eval "$command" >/dev/null 2>&1
    end_time=$(date +%s%N)
    
    duration_us=$(( (end_time - start_time) / 1000 ))
    echo "${duration_us}μs"
    
    return $duration_us
}

echo ""
echo "📊 SURGICAL OPTIMIZATION IMPACT ANALYSIS"
echo "========================================"

# Test 1: Single Operations (should benefit from both optimizations)
echo ""
echo "🎯 1. SINGLE OPERATION PERFORMANCE"
echo "================================="

measure_time_us "Optimized Create" "curl -k -s -X POST \"$SERVER_URL/api/libraries/default/collections/skiplist_test/documents\" \
    -H \"Authorization: Bearer $TOKEN\" \
    -H \"Content-Type: application/json\" \
    -d '{\"key\":\"optimized_test_1\",\"data\":{\"value\":123,\"description\":\"skiplist optimization test\"}}'"

measure_time_us "Optimized Read" "curl -k -s -X GET \"$SERVER_URL/api/libraries/default/collections/skiplist_test/documents\" \
    -H \"Authorization: Bearer $TOKEN\""

# Test 2: Batch Operations (should show significant improvement)
echo ""
echo "🎯 2. BATCH OPERATIONS (Cache Locality Benefits)"
echo "=============================================="

measure_time_us "10 Sequential Creates" "
for i in {1..10}; do
    curl -k -s -X POST \"$SERVER_URL/api/libraries/default/collections/skiplist_batch/documents\" \
        -H \"Authorization: Bearer $TOKEN\" \
        -H \"Content-Type: application/json\" \
        -d \"{\\\"key\\\":\\\"batch_\$i\\\",\\\"data\\\":{\\\"index\\\":\$i,\\\"batch\\\":true}}\"
done"

measure_time_us "50 Sequential Creates" "
for i in {1..50}; do
    curl -k -s -X POST \"$SERVER_URL/api/libraries/default/collections/skiplist_large/documents\" \
        -H \"Authorization: Bearer $TOKEN\" \
        -H \"Content-Type: application/json\" \
        -d \"{\\\"key\\\":\\\"large_\$i\\\",\\\"data\\\":{\\\"index\\\":\$i,\\\"text\\\":\\\"performance test data\\\"}}\"
done"

# Test 3: Query Performance (should benefit from better tree balance)
echo ""
echo "🎯 3. QUERY PERFORMANCE (Better Tree Balance)"
echo "==========================================="

measure_time_us "Simple Query" "curl -k -s -X GET \"$SERVER_URL/api/libraries/default/collections/skiplist_large/documents\" \
    -H \"Authorization: Bearer $TOKEN\""

measure_time_us "Filtered Query" "curl -k -s -X GET \"$SERVER_URL/api/libraries/default/collections/skiplist_large/documents?filter={\\\"data.batch\\\":true}\" \
    -H \"Authorization: Bearer $TOKEN\""

# Test 4: Memory Efficiency
echo ""
echo "🎯 4. MEMORY EFFICIENCY ANALYSIS"
echo "==============================="

JDBX_PID=$(pgrep -f jdbxd | head -1)
if [ -n "$JDBX_PID" ]; then
    start_mem=$(grep "VmRSS" /proc/$JDBX_PID/status | awk '{print $2}')
    echo "Memory before stress test: $((start_mem / 1024)) MB"
    
    # Stress test with 100 operations
    echo "Performing 100 skiplist operations..."
    for i in {1..100}; do
        curl -k -s -X POST "$SERVER_URL/api/libraries/default/collections/stress_test/documents" \
            -H "Authorization: Bearer $TOKEN" \
            -H "Content-Type: application/json" \
            -d "{\"key\":\"stress_$i\",\"data\":{\"index\":$i,\"text\":\"stress test $i\"}}" >/dev/null
    done
    
    end_mem=$(grep "VmRSS" /proc/$JDBX_PID/status | awk '{print $2}')
    echo "Memory after stress test: $((end_mem / 1024)) MB"
    echo "Memory growth: $(((end_mem - start_mem) / 1024)) MB"
    echo "Memory per operation: $(((end_mem - start_mem) / 100)) KB"
else
    echo "❌ JDBX process not found"
fi

echo ""
echo "🎉 SKIPLIST BRAIN SURGERY ANALYSIS COMPLETE"
echo "=========================================="
echo ""
echo "🧠 OPTIMIZATIONS APPLIED:"
echo "1. ⚡ Fast Random Level Generation (P=0.25, xorshift32)"
echo "2. 🎯 Cache-Friendly Node Layout (64-byte aligned, embedded data)"
echo ""
echo "Expected improvements:"
echo "- Single operations: 10-20% faster"
echo "- Batch operations: 20-40% faster (cache locality)" 
echo "- Memory usage: 15-30% reduction (single allocations)"
echo "- Tree balance: Better search performance"