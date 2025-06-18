#!/bin/bash
# Test concurrent requests with curl

BASE_URL="https://localhost:5000"

echo "Testing Concurrent Requests with curl"
echo "====================================="

# Get auth token
TOKEN=$(curl -k -s -X POST "$BASE_URL/api/auth/login" \
    -H "Content-Type: application/json" \
    -d '{"username":"admin","password":"secure123456789"}' | jq -r .token)

if [ -z "$TOKEN" ] || [ "$TOKEN" = "null" ]; then
    echo "❌ Failed to get auth token"
    exit 1
fi

echo "✅ Got auth token"

# Function to create a document
create_document() {
    local id=$1
    local response=$(curl -k -s -w "\n%{http_code}" -X POST "$BASE_URL/api/documents" \
        -H "Authorization: Bearer $TOKEN" \
        -H "Content-Type: application/json" \
        -d "{\"name\":\"Concurrent Test $id\",\"content\":\"Content $id\"}" 2>/dev/null)
    
    local status=$(echo "$response" | tail -1)
    local body=$(echo "$response" | head -n -1)
    
    if [ "$status" = "201" ]; then
        local doc_id=$(echo "$body" | jq -r .uuid)
        # Clean up
        curl -k -s -X DELETE "$BASE_URL/api/documents/$doc_id" \
            -H "Authorization: Bearer $TOKEN" > /dev/null 2>&1
        echo "✅ Request $id: SUCCESS"
    else
        echo "❌ Request $id: Failed with status $status"
    fi
}

# Test sequential requests first
echo -e "\nTesting 5 sequential requests..."
for i in {1..5}; do
    create_document $i
done

# Test concurrent requests
echo -e "\nTesting 10 concurrent requests..."
for i in {1..10}; do
    create_document $i &
done
wait

echo -e "\nAll tests completed"

# Check if server is still running
if curl -k -s "$BASE_URL/api/health" > /dev/null 2>&1; then
    echo "✅ Server is still healthy"
else
    echo "❌ Server is not responding"
fi