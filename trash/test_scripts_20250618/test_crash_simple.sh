#!/bin/bash
# Simple test to identify crash point

BASE_URL="https://localhost:5000"

echo "Simple Crash Test"
echo "================="

# Get auth token
TOKEN=$(curl -k -s -X POST "$BASE_URL/api/auth/login" \
    -H "Content-Type: application/json" \
    -d '{"username":"admin","password":"secure123456789"}' | jq -r .token)

if [ -z "$TOKEN" ] || [ "$TOKEN" = "null" ]; then
    echo "❌ Failed to get auth token"
    exit 1
fi

echo "✅ Got auth token"

# Test creating documents one by one
for i in {1..10}; do
    echo -n "Creating document $i... "
    
    RESPONSE=$(curl -k -s -w "\n%{http_code}" -X POST "$BASE_URL/api/documents" \
        -H "Authorization: Bearer $TOKEN" \
        -H "Content-Type: application/json" \
        -d "{\"name\":\"Test $i\",\"content\":\"Content $i\"}" 2>/dev/null)
    
    STATUS=$(echo "$RESPONSE" | tail -1)
    
    if [ "$STATUS" = "201" ]; then
        DOC_ID=$(echo "$RESPONSE" | head -n -1 | jq -r .uuid)
        echo "✅ Created: $DOC_ID"
        
        # Try to delete it
        echo -n "  Deleting... "
        DEL_STATUS=$(curl -k -s -o /dev/null -w "%{http_code}" -X DELETE "$BASE_URL/api/documents/$DOC_ID" \
            -H "Authorization: Bearer $TOKEN")
        
        if [ "$DEL_STATUS" = "200" ]; then
            echo "✅ Deleted"
        else
            echo "❌ Delete failed: $DEL_STATUS"
        fi
        
        # Small delay
        sleep 0.2
    else
        echo "❌ Failed with status: $STATUS"
        
        # Check if server is alive
        if ! curl -k -s "$BASE_URL/api/health" > /dev/null 2>&1; then
            echo "💥 Server crashed!"
            exit 1
        fi
    fi
done

echo -e "\n✅ All sequential tests passed!"