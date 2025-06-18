#!/bin/bash
# Gradually increase concurrent requests to find breaking point

BASE_URL="https://localhost:5000"

echo "Gradual Concurrent Test"
echo "======================="

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
        -d "{\"name\":\"Test $id\",\"content\":\"Content $id\"}" 2>/dev/null)
    
    local status=$(echo "$response" | tail -1)
    
    if [ "$status" = "201" ]; then
        echo -n "✓"
    else
        echo -n "✗"
    fi
}

# Test with increasing concurrency
for num in 2 3 4 5 6 7 8 9 10; do
    echo -e "\nTesting $num concurrent requests..."
    
    # Launch requests
    for i in $(seq 1 $num); do
        create_document $i &
    done
    
    # Wait for all
    wait
    
    # Check server health
    if curl -k -s "$BASE_URL/api/health" > /dev/null 2>&1; then
        echo " ✅ Server survived $num concurrent requests"
    else
        echo " 💥 Server crashed at $num concurrent requests!"
        exit 1
    fi
    
    # Small delay between tests
    sleep 1
done

echo -e "\n✅ Server handled up to 10 concurrent requests!"