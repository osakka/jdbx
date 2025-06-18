#!/bin/bash
# Test concurrent create and delete operations

BASE_URL="https://localhost:5000"

echo "Concurrent Create+Delete Test"
echo "============================="

# Get auth token
TOKEN=$(curl -k -s -X POST "$BASE_URL/api/auth/login" \
    -H "Content-Type: application/json" \
    -d '{"username":"admin","password":"secure123456789"}' | jq -r .token)

if [ -z "$TOKEN" ] || [ "$TOKEN" = "null" ]; then
    echo "❌ Failed to get auth token"
    exit 1
fi

echo "✅ Got auth token"

# Function to create and delete a document
create_and_delete() {
    local id=$1
    
    # Create
    local response=$(curl -k -s -w "\n%{http_code}" -X POST "$BASE_URL/api/documents" \
        -H "Authorization: Bearer $TOKEN" \
        -H "Content-Type: application/json" \
        -d "{\"name\":\"Test $id\",\"content\":\"Content $id\"}" 2>/dev/null)
    
    local status=$(echo "$response" | tail -1)
    
    if [ "$status" = "201" ]; then
        local doc_id=$(echo "$response" | head -n -1 | jq -r .uuid)
        echo -n "C"
        
        # Delete
        local del_status=$(curl -k -s -o /dev/null -w "%{http_code}" -X DELETE "$BASE_URL/api/documents/$doc_id" \
            -H "Authorization: Bearer $TOKEN")
        
        if [ "$del_status" = "200" ]; then
            echo -n "D"
        else
            echo -n "✗"
        fi
    else
        echo -n "✗"
    fi
}

# Test with increasing concurrency
for num in 2 3 4 5 6 7 8 9 10; do
    echo -e "\nTesting $num concurrent create+delete..."
    
    # Launch requests
    for i in $(seq 1 $num); do
        create_and_delete $i &
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

echo -e "\n✅ Server handled concurrent create+delete operations!"