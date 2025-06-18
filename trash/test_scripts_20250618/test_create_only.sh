#!/bin/bash
# Test concurrent create operations only (no delete)

BASE_URL="https://localhost:5000"

echo "Concurrent Create-Only Test"
echo "==========================="

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
create_doc() {
    local id=$1
    
    # Create
    local response=$(curl -k -s -w "\n%{http_code}" -X POST "$BASE_URL/api/documents" \
        -H "Authorization: Bearer $TOKEN" \
        -H "Content-Type: application/json" \
        -d "{\"name\":\"Test $id\",\"content\":\"Content $id\"}" 2>/dev/null)
    
    local status=$(echo "$response" | tail -1)
    
    if [ "$status" = "201" ]; then
        echo -n "C"
    else
        echo -n "✗"
    fi
}

# Test with increasing concurrency
for num in 2 3 4 5 10 20 30; do
    echo -e "\nTesting $num concurrent creates..."
    
    # Launch requests
    for i in $(seq 1 $num); do
        create_doc $i &
    done
    
    # Wait for all
    wait
    
    # Check server health
    if curl -k -s "$BASE_URL/api/health" > /dev/null 2>&1; then
        echo " ✅ Server survived"
    else
        echo " 💥 Server crashed!"
        exit 1
    fi
    
    # Small delay between tests
    sleep 0.5
done

echo -e "\n✅ Server handled concurrent create operations!"