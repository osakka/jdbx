#!/bin/bash
# Simple checkpoint stress test

BASE_URL="https://localhost:5000"
ADMIN_USER="admin"
ADMIN_PASS="secure123456789"

echo "🚀 JDBX Checkpoint Memory Management Test"
echo "========================================="

# Get token
echo "🔐 Getting authentication token..."
TOKEN=$(curl -s -k -X POST "$BASE_URL/api/auth/login" \
    -H "Content-Type: application/json" \
    -d "{\"username\":\"$ADMIN_USER\",\"password\":\"$ADMIN_PASS\"}" | \
    grep -o '"token":"[^"]*' | cut -d'"' -f4)

if [ -z "$TOKEN" ]; then
    echo "❌ Authentication failed"
    exit 1
fi
echo "✅ Authentication successful"

# Function to create document
create_doc() {
    local id=$1
    local size=$2
    
    # Generate content based on size
    case $size in
        small) content="Small content $id" ;;
        medium) content=$(printf "Medium content %s: %s" "$id" "$(head -c 500 < /dev/zero | tr '\0' 'x')") ;;
        large) content=$(printf "Large content %s: %s" "$id" "$(head -c 2000 < /dev/zero | tr '\0' 'x')") ;;
    esac
    
    curl -s -k -X POST "$BASE_URL/api/documents" \
        -H "Authorization: Bearer $TOKEN" \
        -H "Content-Type: application/json" \
        -d "{\"title\":\"Test $id\",\"content\":\"$content\",\"size\":\"$size\"}" \
        -w "HTTP:%{http_code}" | grep -o "HTTP:[0-9]*"
}

# Function to query documents
query_docs() {
    curl -s -k -X GET "$BASE_URL/api/documents" \
        -H "Authorization: Bearer $TOKEN" \
        -w "HTTP:%{http_code}" | grep -o "HTTP:[0-9]*"
}

# Test 1: Sequential operations
echo ""
echo "🧪 Test 1: Sequential Operations (20 docs)"
success_count=0
total_count=0

for i in {1..20}; do
    size="small"
    if [ $((i % 3)) -eq 0 ]; then size="medium"; fi
    if [ $((i % 5)) -eq 0 ]; then size="large"; fi
    
    result=$(create_doc $i $size)
    total_count=$((total_count + 1))
    
    if [[ "$result" == "HTTP:201" ]]; then
        success_count=$((success_count + 1))
        echo -n "✅"
    else
        echo -n "❌($result)"
    fi
    
    if [ $((i % 10)) -eq 0 ]; then echo ""; fi
done

echo ""
echo "📊 Sequential Test: $success_count/$total_count successful"

# Test 2: Rapid fire operations
echo ""
echo "🧪 Test 2: Rapid Fire Operations (50 docs)"
success_count=0
total_count=0

for i in {1..50}; do
    result=$(create_doc "rapid-$i" "small")
    total_count=$((total_count + 1))
    
    if [[ "$result" == "HTTP:201" ]]; then
        success_count=$((success_count + 1))
        echo -n "."
    else
        echo -n "x"
    fi
    
    if [ $((i % 25)) -eq 0 ]; then echo ""; fi
done

echo ""
echo "📊 Rapid Fire Test: $success_count/$total_count successful"

# Test 3: Query operations
echo ""
echo "🧪 Test 3: Query Operations (20 queries)"
success_count=0
total_count=0

for i in {1..20}; do
    result=$(query_docs)
    total_count=$((total_count + 1))
    
    if [[ "$result" == "HTTP:200" ]]; then
        success_count=$((success_count + 1))
        echo -n "📖"
    else
        echo -n "❌"
    fi
    
    if [ $((i % 10)) -eq 0 ]; then echo ""; fi
done

echo ""
echo "📊 Query Test: $success_count/$total_count successful"

# Test 4: Concurrent operations (background jobs)
echo ""
echo "🧪 Test 4: Concurrent Operations (10 parallel sessions)"

# Function for background session
run_session() {
    local session_id=$1
    local success=0
    local total=0
    
    for i in {1..10}; do
        result=$(create_doc "concurrent-$session_id-$i" "small")
        total=$((total + 1))
        if [[ "$result" == "HTTP:201" ]]; then
            success=$((success + 1))
        fi
    done
    
    echo "$success $total" > "/tmp/session_$session_id.result"
}

# Start background sessions
for session in {1..10}; do
    run_session $session &
done

# Wait for all to complete
wait

# Collect results
total_success=0
total_ops=0

for session in {1..10}; do
    if [ -f "/tmp/session_$session.result" ]; then
        result=$(cat "/tmp/session_$session.result")
        success=$(echo "$result" | cut -d' ' -f1)
        ops=$(echo "$result" | cut -d' ' -f2)
        total_success=$((total_success + success))
        total_ops=$((total_ops + ops))
        rm -f "/tmp/session_$session.result"
        echo -n "[$success/$ops]"
    fi
done

echo ""
echo "📊 Concurrent Test: $total_success/$total_ops successful"

# Final summary
echo ""
echo "🎯 Final Summary:"
echo "=================="

# Calculate overall success rate
overall_success=$((success_count + total_success))
overall_total=$((total_count + total_ops))

if [ $overall_total -gt 0 ]; then
    success_rate=$((overall_success * 100 / overall_total))
    echo "📊 Overall Success Rate: $overall_success/$overall_total ($success_rate%)"
    
    if [ $success_rate -ge 95 ]; then
        echo "🎉 EXCELLENT: Checkpoint system performing exceptionally!"
    elif [ $success_rate -ge 85 ]; then
        echo "✅ GOOD: Checkpoint system stable and reliable"
    elif [ $success_rate -ge 70 ]; then
        echo "⚠️  MODERATE: Some issues detected"
    else
        echo "❌ POOR: Significant problems with checkpoint system"
    fi
else
    echo "❌ No operations completed - critical failure"
fi

# Check server logs for checkpoint activity
echo ""
echo "🔍 Recent Checkpoint Activity:"
echo "=============================="
tail -10 /opt/jdbx/build/var/jdbxd.log | grep -i "checkpoint\|memory" || echo "No checkpoint logs found"

echo ""
echo "✅ Checkpoint stress test completed!"