#!/bin/bash
# Phase 2.1: Cache Brain Surgery Performance Analysis

SERVER_URL="https://localhost:5000"

echo "🧠💾 CACHE BRAIN SURGERY: Performance Analysis"
echo "==============================================="

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

# Function to measure cache performance
test_cache_efficiency() {
    local test_name=$1
    local doc_id=$2
    echo ""
    echo "🎯 $test_name"
    echo "================================"
    
    # First request (cache miss)
    echo -n "Cache MISS (first request): "
    start_time=$(date +%s%N)
    curl -k -s -X GET "$SERVER_URL/api/libraries/default/collections/cache_test/documents/$doc_id" \
        -H "Authorization: Bearer $TOKEN" >/dev/null
    end_time=$(date +%s%N)
    miss_time=$(( (end_time - start_time) / 1000 ))
    echo "${miss_time}μs"
    
    # Second request (cache hit)
    echo -n "Cache HIT (repeat request): "
    start_time=$(date +%s%N)
    curl -k -s -X GET "$SERVER_URL/api/libraries/default/collections/cache_test/documents/$doc_id" \
        -H "Authorization: Bearer $TOKEN" >/dev/null
    end_time=$(date +%s%N)
    hit_time=$(( (end_time - start_time) / 1000 ))
    echo "${hit_time}μs"
    
    # Calculate improvement
    if [ $miss_time -gt 0 ]; then
        improvement=$(( (miss_time - hit_time) * 100 / miss_time ))
        echo "Cache improvement: ${improvement}% faster"
    fi
}

echo ""
echo "📊 CACHE SETUP: Creating test documents"
echo "======================================"

# Create test documents for cache analysis
DOC_IDS=()
for i in {1..5}; do
    response=$(curl -k -s -X POST "$SERVER_URL/api/libraries/default/collections/cache_test/documents" \
        -H "Authorization: Bearer $TOKEN" \
        -H "Content-Type: application/json" \
        -d "{\"name\":\"cache_test_$i\",\"data\":{\"value\":$i,\"text\":\"cache performance test\"}}")
    
    doc_id=$(echo "$response" | grep -o '"uuid":"[^"]*"' | cut -d'"' -f4)
    if [ -n "$doc_id" ]; then
        DOC_IDS+=("$doc_id")
        echo "Created test document: $doc_id"
    fi
done

echo ""
echo "🧠 CACHE PERFORMANCE ANALYSIS"
echo "============================="

# Test individual document cache performance
if [ ${#DOC_IDS[@]} -gt 0 ]; then
    test_cache_efficiency "Document Cache Test #1" "${DOC_IDS[0]}"
    test_cache_efficiency "Document Cache Test #2" "${DOC_IDS[1]}"
    test_cache_efficiency "Document Cache Test #3" "${DOC_IDS[2]}"
fi

# Test query cache performance
echo ""
echo "🎯 Query Cache Performance"
echo "========================="

echo -n "Query MISS (first query): "
start_time=$(date +%s%N)
curl -k -s -X GET "$SERVER_URL/api/libraries/default/collections/cache_test/documents?filter={\"data.value\":1}" \
    -H "Authorization: Bearer $TOKEN" >/dev/null
end_time=$(date +%s%N)
query_miss_time=$(( (end_time - start_time) / 1000 ))
echo "${query_miss_time}μs"

echo -n "Query HIT (repeat query): "
start_time=$(date +%s%N)
curl -k -s -X GET "$SERVER_URL/api/libraries/default/collections/cache_test/documents?filter={\"data.value\":1}" \
    -H "Authorization: Bearer $TOKEN" >/dev/null
end_time=$(date +%s%N)
query_hit_time=$(( (end_time - start_time) / 1000 ))
echo "${query_hit_time}μs"

if [ $query_miss_time -gt 0 ]; then
    query_improvement=$(( (query_miss_time - query_hit_time) * 100 / query_miss_time ))
    echo "Query cache improvement: ${query_improvement}% faster"
fi

# Test cache saturation
echo ""
echo "🎯 Cache Saturation Test"
echo "======================="

echo "Creating 20 documents to test cache behavior..."
for i in {6..25}; do
    curl -k -s -X POST "$SERVER_URL/api/libraries/default/collections/cache_saturation/documents" \
        -H "Authorization: Bearer $TOKEN" \
        -H "Content-Type: application/json" \
        -d "{\"name\":\"saturation_test_$i\",\"data\":{\"index\":$i}}" >/dev/null
done

echo "Testing cache efficiency under load..."
avg_time=0
for i in {1..10}; do
    start_time=$(date +%s%N)
    curl -k -s -X GET "$SERVER_URL/api/libraries/default/collections/cache_saturation/documents" \
        -H "Authorization: Bearer $TOKEN" >/dev/null
    end_time=$(date +%s%N)
    operation_time=$(( (end_time - start_time) / 1000 ))
    avg_time=$(( (avg_time + operation_time) / 2 ))
done

echo "Average query time under cache load: ${avg_time}μs"

echo ""
echo "🧠 CACHE BRAIN SURGERY DIAGNOSIS"
echo "================================"
echo ""
echo "CURRENT CACHE ARCHITECTURE:"
echo "- Document cache: 10,000 entries (LRU)"
echo "- Query cache: 1,000 entries (LRU)" 
echo "- Implementation: Basic LRU with hash table"
echo ""
echo "OPTIMIZATION OPPORTUNITIES IDENTIFIED:"
echo "1. Cache sizing may not be optimal for workload"
echo "2. LRU may not be best eviction policy for JSON queries"
echo "3. Query cache key generation could be improved"
echo "4. No workload-adaptive cache management"