#!/bin/bash
# Detailed test to understand crash pattern

BASE_URL="https://localhost:5000"

echo "Detailed Crash Analysis Test"
echo "============================"

# Get auth token
TOKEN=$(curl -k -s -X POST "$BASE_URL/api/auth/login" \
    -H "Content-Type: application/json" \
    -d '{"username":"admin","password":"secure123456789"}' | jq -r .token)

if [ -z "$TOKEN" ] || [ "$TOKEN" = "null" ]; then
    echo "❌ Failed to get auth token"
    exit 1
fi

echo "✅ Got auth token"
echo ""

# Test 1: Sequential create+delete (should work)
echo "Test 1: Sequential create+delete..."
for i in 1 2 3 4 5; do
    # Create
    response=$(curl -k -s -X POST "$BASE_URL/api/documents" \
        -H "Authorization: Bearer $TOKEN" \
        -H "Content-Type: application/json" \
        -d "{\"name\":\"Sequential $i\",\"content\":\"Content $i\"}")
    
    uuid=$(echo "$response" | jq -r .uuid)
    if [ -n "$uuid" ] && [ "$uuid" != "null" ]; then
        echo -n "C"
        
        # Delete immediately
        del_status=$(curl -k -s -o /dev/null -w "%{http_code}" -X DELETE "$BASE_URL/api/documents/$uuid" \
            -H "Authorization: Bearer $TOKEN")
        
        if [ "$del_status" = "200" ]; then
            echo -n "D"
        else
            echo -n "✗"
        fi
    else
        echo -n "✗"
    fi
done
echo " ✅ Sequential test complete"
echo ""

# Test 2: Create all, then delete all (should work)
echo "Test 2: Create all, then delete all..."
declare -a UUIDS=()

# Create phase
for i in 1 2 3 4 5; do
    response=$(curl -k -s -X POST "$BASE_URL/api/documents" \
        -H "Authorization: Bearer $TOKEN" \
        -H "Content-Type: application/json" \
        -d "{\"name\":\"Batch $i\",\"content\":\"Content $i\"}")
    
    uuid=$(echo "$response" | jq -r .uuid)
    if [ -n "$uuid" ] && [ "$uuid" != "null" ]; then
        UUIDS+=("$uuid")
        echo -n "C"
    else
        echo -n "✗"
    fi
done

# Delete phase
for uuid in "${UUIDS[@]}"; do
    del_status=$(curl -k -s -o /dev/null -w "%{http_code}" -X DELETE "$BASE_URL/api/documents/$uuid" \
        -H "Authorization: Bearer $TOKEN")
    
    if [ "$del_status" = "200" ]; then
        echo -n "D"
    else
        echo -n "✗"
    fi
done
echo " ✅ Batch test complete"
echo ""

# Test 3: Exactly 3 concurrent create+delete (the breaking point)
echo "Test 3: Exactly 3 concurrent create+delete..."

# Function to create and delete
create_and_delete() {
    local id=$1
    local response=$(curl -k -s -X POST "$BASE_URL/api/documents" \
        -H "Authorization: Bearer $TOKEN" \
        -H "Content-Type: application/json" \
        -d "{\"name\":\"Concurrent $id\",\"content\":\"Content $id\"}")
    
    local uuid=$(echo "$response" | jq -r .uuid)
    if [ -n "$uuid" ] && [ "$uuid" != "null" ]; then
        echo "Thread $id: Created $uuid"
        
        # Small delay to increase chance of race condition
        sleep 0.01
        
        local del_status=$(curl -k -s -o /dev/null -w "%{http_code}" -X DELETE "$BASE_URL/api/documents/$uuid" \
            -H "Authorization: Bearer $TOKEN")
        
        if [ "$del_status" = "200" ]; then
            echo "Thread $id: Deleted $uuid ✅"
        else
            echo "Thread $id: Delete failed ✗"
        fi
    else
        echo "Thread $id: Create failed ✗"
    fi
}

# Launch exactly 3 concurrent operations
for i in 1 2 3; do
    create_and_delete $i &
done

# Wait for all
wait

# Check server health
sleep 1
if curl -k -s "$BASE_URL/api/health" > /dev/null 2>&1; then
    echo "✅ Server survived 3 concurrent operations!"
else
    echo "💥 Server crashed at 3 concurrent operations!"
fi