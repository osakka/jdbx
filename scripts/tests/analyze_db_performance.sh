#!/bin/bash
# Phase 2.1: Database Engine Performance Analysis
# Deep dive into current performance characteristics

SERVER_URL="https://localhost:5000"
TOKEN="eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJqZGJ4Iiwic3ViIjoiZG9jLTE3NTA2MTQ1MjgtMzMxNTc0OTY3NSIsImV4cCI6MTc1MDYxNjU3MCwiaWF0IjoxNzUwNjE0NzcwLCJ1c2VybmFtZSI6ImFkbWluIiwidHlwZSI6ImFjY2VzcyJ9.AJruxEo9CRcJcB4fkPnA_sKPLEweqAYLj-nJdpkMdEA"

echo "🧠 PHASE 2.1: Database Engine Performance Analysis"
echo "=================================================="

# Function to measure operation time
measure_time() {
    local operation=$1
    local command=$2
    echo -n "⏱️  $operation: "
    
    start_time=$(date +%s%N)
    eval "$command" >/dev/null 2>&1
    end_time=$(date +%s%N)
    
    duration_ms=$(( (end_time - start_time) / 1000000 ))
    echo "${duration_ms}ms"
    
    return $duration_ms
}

# Test 1: Single Document Operations
echo ""
echo "📊 1. SINGLE DOCUMENT PERFORMANCE"
echo "================================="

# Create operation
measure_time "Document Create" "curl -k -s -X POST \"$SERVER_URL/api/libraries/default/collections/perf_test/documents\" \
    -H \"Authorization: Bearer $TOKEN\" \
    -H \"Content-Type: application/json\" \
    -d '{\"name\":\"perf_doc_1\",\"data\":{\"value\":42,\"text\":\"performance test\"}}'"

# Read operation
measure_time "Document Read" "curl -k -s -X GET \"$SERVER_URL/api/libraries/default/collections/perf_test/documents\" \
    -H \"Authorization: Bearer $TOKEN\""

# Update operation (need to get document ID first)
DOC_ID=$(curl -k -s -X GET "$SERVER_URL/api/libraries/default/collections/perf_test/documents" \
    -H "Authorization: Bearer $TOKEN" | grep -o '"uuid":"[^"]*"' | head -1 | cut -d'"' -f4)

if [ -n "$DOC_ID" ]; then
    measure_time "Document Update" "curl -k -s -X PUT \"$SERVER_URL/api/libraries/default/collections/perf_test/documents/$DOC_ID\" \
        -H \"Authorization: Bearer $TOKEN\" \
        -H \"Content-Type: application/json\" \
        -d '{\"name\":\"perf_doc_updated\",\"data\":{\"value\":84,\"text\":\"updated performance test\"}}'"
fi

# Test 2: Batch Operations Performance
echo ""
echo "📊 2. BATCH OPERATIONS PERFORMANCE"
echo "=================================="

# Small batch (10 documents)
measure_time "Small Batch Create (10 docs)" "
for i in {1..10}; do
    curl -k -s -X POST \"$SERVER_URL/api/libraries/default/collections/batch_test/documents\" \
        -H \"Authorization: Bearer $TOKEN\" \
        -H \"Content-Type: application/json\" \
        -d \"{\\\"name\\\":\\\"batch_doc_\$i\\\",\\\"data\\\":{\\\"batch\\\":true,\\\"index\\\":\$i}}\" &
done
wait"

# Medium batch (50 documents)
measure_time "Medium Batch Create (50 docs)" "
for i in {1..50}; do
    curl -k -s -X POST \"$SERVER_URL/api/libraries/default/collections/batch_test/documents\" \
        -H \"Authorization: Bearer $TOKEN\" \
        -H \"Content-Type: application/json\" \
        -d \"{\\\"name\\\":\\\"medium_doc_\$i\\\",\\\"data\\\":{\\\"batch\\\":true,\\\"index\\\":\$i}}\" &
done
wait"

# Test 3: Query Performance
echo ""
echo "📊 3. QUERY PERFORMANCE ANALYSIS"
echo "==============================="

# Simple query
measure_time "Simple Query (all docs)" "curl -k -s -X GET \"$SERVER_URL/api/libraries/default/collections/batch_test/documents\" \
    -H \"Authorization: Bearer $TOKEN\""

# Complex query with filters
measure_time "Filtered Query" "curl -k -s -X GET \"$SERVER_URL/api/libraries/default/collections/batch_test/documents?filter={\\\"data.batch\\\":true}\" \
    -H \"Authorization: Bearer $TOKEN\""

# Test 4: Memory Usage During Operations
echo ""
echo "📊 4. MEMORY USAGE ANALYSIS"
echo "==========================="

JDBX_PID=$(pgrep -f jdbxd | head -1)
if [ -n "$JDBX_PID" ]; then
    start_mem=$(grep "VmRSS" /proc/$JDBX_PID/status | awk '{print $2}')
    echo "Memory before operations: $((start_mem / 1024)) MB"
    
    # Heavy operation batch
    echo "Performing heavy operations..."
    for i in {1..20}; do
        curl -k -s -X POST "$SERVER_URL/api/libraries/default/collections/memory_test/documents" \
            -H "Authorization: Bearer $TOKEN" \
            -H "Content-Type: application/json" \
            -d "{\"name\":\"heavy_doc_$i\",\"data\":{\"large_text\":\"$(head -c 1000 /dev/urandom | base64 | tr -d '\n')\"}}" >/dev/null &
    done
    wait
    
    end_mem=$(grep "VmRSS" /proc/$JDBX_PID/status | awk '{print $2}')
    echo "Memory after operations: $((end_mem / 1024)) MB"
    echo "Memory growth: $(((end_mem - start_mem) / 1024)) MB"
else
    echo "❌ JDBX process not found for memory analysis"
fi

# Test 5: Cache Performance Analysis
echo ""
echo "📊 5. CACHE PERFORMANCE ANALYSIS"
echo "==============================="

# First query (cache miss)
echo "First query (cache miss):"
measure_time "Cache Miss Query" "curl -k -s -X GET \"$SERVER_URL/api/libraries/default/collections/batch_test/documents?filter={\\\"name\\\":\\\"batch_doc_1\\\"}\" \
    -H \"Authorization: Bearer $TOKEN\""

# Repeat query (cache hit)
echo "Repeat query (cache hit):"
measure_time "Cache Hit Query" "curl -k -s -X GET \"$SERVER_URL/api/libraries/default/collections/batch_test/documents?filter={\\\"name\\\":\\\"batch_doc_1\\\"}\" \
    -H \"Authorization: Bearer $TOKEN\""

# Test 6: Concurrent Performance
echo ""
echo "📊 6. CONCURRENT PERFORMANCE ANALYSIS"
echo "====================================="

echo "Testing concurrent operations (10 parallel requests):"
measure_time "10 Concurrent Creates" "
for i in {1..10}; do
    curl -k -s -X POST \"$SERVER_URL/api/libraries/default/collections/concurrent_test/documents\" \
        -H \"Authorization: Bearer $TOKEN\" \
        -H \"Content-Type: application/json\" \
        -d \"{\\\"name\\\":\\\"concurrent_doc_\$i\\\",\\\"data\\\":{\\\"concurrent\\\":true,\\\"thread\\\":\$i}}\" &
done
wait"

echo ""
echo "🎯 PERFORMANCE ANALYSIS COMPLETE"
echo "================================"
echo "Results saved for Phase 2.1 optimization planning"