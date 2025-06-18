#!/bin/bash
# Simple stress test - just keep creating and deleting until crash

BASE_URL="https://localhost:5000"

echo "Simple Stress Test - Create/Delete Loop"
echo "======================================="

# Get auth token
TOKEN=$(curl -k -s -X POST "$BASE_URL/api/auth/login" \
    -H "Content-Type: application/json" \
    -d '{"username":"admin","password":"secure123456789"}' | jq -r .token)

if [ -z "$TOKEN" ] || [ "$TOKEN" = "null" ]; then
    echo "❌ Failed to get auth token"
    exit 1
fi

echo "✅ Got auth token"
echo "Starting loop..."

count=0
while true; do
    # Create
    response=$(curl -k -s -X POST "$BASE_URL/api/documents" \
        -H "Authorization: Bearer $TOKEN" \
        -H "Content-Type: application/json" \
        -d "{\"name\":\"Test $count\",\"content\":\"Content $count\"}")
    
    uuid=$(echo "$response" | jq -r .uuid 2>/dev/null)
    
    if [ -z "$uuid" ] || [ "$uuid" = "null" ]; then
        echo -e "\n💥 Server crashed after $count iterations!"
        exit 1
    fi
    
    # Delete
    del_status=$(curl -k -s -o /dev/null -w "%{http_code}" -X DELETE "$BASE_URL/api/documents/$uuid" \
        -H "Authorization: Bearer $TOKEN")
    
    if [ "$del_status" != "200" ]; then
        echo -e "\n💥 Delete failed after $count iterations!"
        exit 1
    fi
    
    count=$((count + 1))
    echo -n "."
    
    # Every 10 iterations, show count
    if [ $((count % 10)) -eq 0 ]; then
        echo -n " $count"
    fi
done