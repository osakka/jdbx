#!/bin/bash
# Next Discovery Test - Find the next improvement opportunity

BASE_URL="https://localhost:5000"
ADMIN_USER="admin"
ADMIN_PASS="secure123456789"

echo "🔍 JDBX Next Discovery Test - Finding Next Improvement Opportunities"
echo "=================================================================="
echo "Current Achievement: 100% SSL reliability, 100% basic operations"
echo "Searching for: Next bottleneck or edge case to improve"
echo ""

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

# Test 1: Extreme Document Sizes
echo ""
echo "🧪 Test 1: Extreme Document Size Limits"
echo "======================================="

# Very small document
echo "📝 Testing very small document..."
TINY_RESPONSE=$(curl -s -k -X POST "$BASE_URL/api/documents" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d '{"x":"1"}' \
    -w "HTTP:%{http_code}")

TINY_CODE=$(echo "$TINY_RESPONSE" | grep -o "HTTP:[0-9]*" | cut -d':' -f2)
if [ "$TINY_CODE" = "201" ]; then
    echo "✅ Tiny document (7 bytes): SUCCESS"
else
    echo "❌ Tiny document failed: HTTP $TINY_CODE"
fi

# Medium document  
echo "📝 Testing medium document (5KB)..."
MEDIUM_CONTENT=$(head -c 5000 < /dev/zero | tr '\0' 'A')
MEDIUM_RESPONSE=$(curl -s -k -X POST "$BASE_URL/api/documents" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d "{\"title\":\"Medium Document\",\"content\":\"$MEDIUM_CONTENT\"}" \
    -w "HTTP:%{http_code}")

MEDIUM_CODE=$(echo "$MEDIUM_RESPONSE" | grep -o "HTTP:[0-9]*" | cut -d':' -f2)
if [ "$MEDIUM_CODE" = "201" ]; then
    echo "✅ Medium document (5KB): SUCCESS"
else
    echo "❌ Medium document failed: HTTP $MEDIUM_CODE"
fi

# Large document
echo "📝 Testing large document (50KB)..."
LARGE_CONTENT=$(head -c 50000 < /dev/zero | tr '\0' 'B')
LARGE_RESPONSE=$(curl -s -k -X POST "$BASE_URL/api/documents" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d "{\"title\":\"Large Document\",\"content\":\"$LARGE_CONTENT\"}" \
    -w "HTTP:%{http_code}")

LARGE_CODE=$(echo "$LARGE_RESPONSE" | grep -o "HTTP:[0-9]*" | cut -d':' -f2)
if [ "$LARGE_CODE" = "201" ]; then
    echo "✅ Large document (50KB): SUCCESS"
else
    echo "❌ Large document failed: HTTP $LARGE_CODE"
    echo "🔍 This could be our next improvement opportunity!"
fi

# Test 2: Complex JSON Structures
echo ""
echo "🧪 Test 2: Complex JSON Structures"
echo "==================================="

# Deep nesting
echo "📝 Testing deeply nested JSON..."
DEEP_JSON='{"level1":{"level2":{"level3":{"level4":{"level5":{"level6":{"level7":{"level8":{"level9":{"level10":"deep data"}}}}}}}}}}'
DEEP_RESPONSE=$(curl -s -k -X POST "$BASE_URL/api/documents" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d "$DEEP_JSON" \
    -w "HTTP:%{http_code}")

DEEP_CODE=$(echo "$DEEP_RESPONSE" | grep -o "HTTP:[0-9]*" | cut -d':' -f2)
if [ "$DEEP_CODE" = "201" ]; then
    echo "✅ Deep nested JSON (10 levels): SUCCESS"
else
    echo "❌ Deep nested JSON failed: HTTP $DEEP_CODE"
    echo "🔍 JSON parsing depth could be our next improvement!"
fi

# Large array
echo "📝 Testing large array (1000 elements)..."
ARRAY_JSON='{"items":['
for i in $(seq 1 1000); do
    ARRAY_JSON+="{\"id\":$i,\"value\":\"item$i\"}"
    if [ $i -lt 1000 ]; then
        ARRAY_JSON+=","
    fi
done
ARRAY_JSON+=']}'

ARRAY_RESPONSE=$(curl -s -k -X POST "$BASE_URL/api/documents" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d "$ARRAY_JSON" \
    -w "HTTP:%{http_code}")

ARRAY_CODE=$(echo "$ARRAY_RESPONSE" | grep -o "HTTP:[0-9]*" | cut -d':' -f2)
if [ "$ARRAY_CODE" = "201" ]; then
    echo "✅ Large array (1000 items): SUCCESS"
else
    echo "❌ Large array failed: HTTP $ARRAY_CODE"
    echo "🔍 Array processing could be our next improvement!"
fi

# Test 3: Special Characters and Unicode
echo ""
echo "🧪 Test 3: Special Characters & Unicode"
echo "======================================="

# Unicode test
echo "📝 Testing Unicode content..."
UNICODE_RESPONSE=$(curl -s -k -X POST "$BASE_URL/api/documents" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json; charset=utf-8" \
    -d '{"title":"Unicode Test","content":"Hello 世界! 🚀 Ñoño café naïve résumé"}' \
    -w "HTTP:%{http_code}")

UNICODE_CODE=$(echo "$UNICODE_RESPONSE" | grep -o "HTTP:[0-9]*" | cut -d':' -f2)
if [ "$UNICODE_CODE" = "201" ]; then
    echo "✅ Unicode content: SUCCESS"
else
    echo "❌ Unicode content failed: HTTP $UNICODE_CODE"
    echo "🔍 Unicode handling could be our next improvement!"
fi

# Special characters test
echo "📝 Testing special characters..."
SPECIAL_RESPONSE=$(curl -s -k -X POST "$BASE_URL/api/documents" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d '{"title":"Special Chars","content":"Quotes: \"hello\" '\''world'\'' Backslashes: \\ Newlines: \n Tabs: \t"}' \
    -w "HTTP:%{http_code}")

SPECIAL_CODE=$(echo "$SPECIAL_RESPONSE" | grep -o "HTTP:[0-9]*" | cut -d':' -f2)
if [ "$SPECIAL_CODE" = "201" ]; then
    echo "✅ Special characters: SUCCESS"
else
    echo "❌ Special characters failed: HTTP $SPECIAL_CODE"
    echo "🔍 Character escaping could be our next improvement!"
fi

# Test 4: API Edge Cases
echo ""
echo "🧪 Test 4: API Edge Cases"
echo "========================="

# Empty document
echo "📝 Testing empty document..."
EMPTY_RESPONSE=$(curl -s -k -X POST "$BASE_URL/api/documents" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d '{}' \
    -w "HTTP:%{http_code}")

EMPTY_CODE=$(echo "$EMPTY_RESPONSE" | grep -o "HTTP:[0-9]*" | cut -d':' -f2)
if [ "$EMPTY_CODE" = "201" ]; then
    echo "✅ Empty document: SUCCESS"
else
    echo "❌ Empty document failed: HTTP $EMPTY_CODE"
    echo "🔍 Empty document handling could be our next improvement!"
fi

# Invalid JSON
echo "📝 Testing malformed JSON handling..."
INVALID_RESPONSE=$(curl -s -k -X POST "$BASE_URL/api/documents" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d '{"invalid": json}' \
    -w "HTTP:%{http_code}")

INVALID_CODE=$(echo "$INVALID_RESPONSE" | grep -o "HTTP:[0-9]*" | cut -d':' -f2)
if [ "$INVALID_CODE" = "400" ]; then
    echo "✅ Invalid JSON properly rejected: HTTP 400"
else
    echo "⚠️ Invalid JSON unexpected response: HTTP $INVALID_CODE"
    echo "🔍 JSON validation could be our next improvement!"
fi

# Test 5: Rapid Sequential Operations
echo ""
echo "🧪 Test 5: Rapid Sequential Operations (100 ops)"
echo "==============================================="

success_count=0
start_time=$(date +%s.%N)

for i in {1..100}; do
    RESPONSE=$(curl -s -k -X POST "$BASE_URL/api/documents" \
        -H "Authorization: Bearer $TOKEN" \
        -H "Content-Type: application/json" \
        -d "{\"id\":$i,\"data\":\"rapid test $i\"}" \
        -w "HTTP:%{http_code}" \
        --max-time 10 --connect-timeout 5)
    
    CODE=$(echo "$RESPONSE" | grep -o "HTTP:[0-9]*" | cut -d':' -f2)
    if [ "$CODE" = "201" ]; then
        success_count=$((success_count + 1))
        echo -n "."
    else
        echo -n "x"
    fi
    
    if [ $((i % 25)) -eq 0 ]; then echo " ($i/100)"; fi
done

end_time=$(date +%s.%N)
duration=$(echo "$end_time - $start_time" | bc)

echo ""
echo "📊 Rapid Sequential: $success_count/100 successful in ${duration}s"

if [ $success_count -eq 100 ]; then
    echo "✅ EXCELLENT: 100% success rate"
    ops_per_sec=$(echo "scale=2; 100 / $duration" | bc)
    echo "⚡ Performance: $ops_per_sec operations/second"
else
    echo "⚠️ Some operations failed - potential improvement opportunity!"
fi

# Summary
echo ""
echo "🎯 Next Improvement Discovery Summary:"
echo "====================================="
echo "🔍 Scanning for bottlenecks and edge cases..."

failure_found=false

if [ "$LARGE_CODE" != "201" ]; then
    echo "🎯 OPPORTUNITY: Large documents (>50KB) need optimization"
    failure_found=true
fi

if [ "$DEEP_CODE" != "201" ]; then
    echo "🎯 OPPORTUNITY: Deep JSON nesting limits need expansion"
    failure_found=true
fi

if [ "$ARRAY_CODE" != "201" ]; then
    echo "🎯 OPPORTUNITY: Large array processing needs optimization"
    failure_found=true
fi

if [ "$UNICODE_CODE" != "201" ]; then
    echo "🎯 OPPORTUNITY: Unicode handling needs improvement"
    failure_found=true
fi

if [ $success_count -lt 95 ]; then
    echo "🎯 OPPORTUNITY: Rapid operation reliability needs enhancement"
    failure_found=true
fi

if [ "$failure_found" = false ]; then
    echo "🎉 AMAZING: No obvious bottlenecks found!"
    echo "🔍 System is performing exceptionally well"
    echo "💡 Consider stress testing with even higher loads or more complex scenarios"
else
    echo "🚀 READY: Next improvement opportunity identified!"
    echo "📋 Focus areas for next 'test → fix → repeat' cycle discovered"
fi

echo ""
echo "🔍 Recent Server Activity:"
echo "=========================="
tail -15 /opt/jdbx/build/var/jdbxd.log | grep -E "(ERROR|WARNING|INFO.*document|INFO.*SSL)"

echo ""
echo "✅ Next discovery test completed!"