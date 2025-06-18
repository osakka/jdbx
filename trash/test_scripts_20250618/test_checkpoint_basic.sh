#!/bin/bash
# Basic test of checkpoint system

BASE_URL="https://localhost:5000"

echo "Basic Checkpoint Test"
echo "===================="

# Get auth token
TOKEN=$(curl -k -s -X POST "$BASE_URL/api/auth/login" \
    -H "Content-Type: application/json" \
    -d '{"username":"admin","password":"secure123456789"}' | jq -r .token)

if [ -z "$TOKEN" ] || [ "$TOKEN" = "null" ]; then
    echo "❌ Failed to get auth token"
    exit 1
fi

echo "✅ Got auth token"

# Test 1: Just create documents (no delete)
echo -e "\nTest 1: Create 20 documents..."
for i in {1..20}; do
    response=$(curl -k -s -X POST "$BASE_URL/api/documents" \
        -H "Authorization: Bearer $TOKEN" \
        -H "Content-Type: application/json" \
        -d "{\"name\":\"Test $i\",\"content\":\"Content $i\"}")
    
    uuid=$(echo "$response" | jq -r .uuid 2>/dev/null)
    if [ -n "$uuid" ] && [ "$uuid" != "null" ]; then
        echo -n "."
    else
        echo -e "\n❌ Create failed at iteration $i"
        echo "Response: $response"
        exit 1
    fi
done

echo -e "\n✅ Created 20 documents successfully!"

# Check server health
if curl -k -s "$BASE_URL/api/health" > /dev/null 2>&1; then
    echo "✅ Server still healthy"
else
    echo "💥 Server crashed!"
    exit 1
fi