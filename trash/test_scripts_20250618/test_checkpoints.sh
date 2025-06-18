#!/bin/bash
# Test checkpoint system with multiple operations

TOKEN=$(curl -s -k -X POST https://localhost:5000/api/auth/login -H "Content-Type: application/json" -d '{"username":"admin","password":"secure123456789"}' | grep -o '"token":"[^"]*' | cut -d'"' -f4)

echo "Token: $TOKEN"
echo "Running multiple operations..."

# Create 10 documents
for i in {1..10}; do
    echo -n "Creating document $i... "
    RESPONSE=$(curl -s -k -X POST https://localhost:5000/api/documents \
        -H "Authorization: Bearer $TOKEN" \
        -H "Content-Type: application/json" \
        -d "{\"title\":\"Test $i\",\"content\":\"Content $i\"}" \
        -w "\nHTTP_CODE:%{http_code}")
    
    HTTP_CODE=$(echo "$RESPONSE" | grep -o "HTTP_CODE:[0-9]*" | cut -d: -f2)
    if [ "$HTTP_CODE" = "201" ]; then
        echo "✅ Success"
    else
        echo "❌ Failed (HTTP $HTTP_CODE)"
        echo "$RESPONSE" | head -n -1
    fi
done

# List documents
echo -n "Listing documents... "
RESPONSE=$(curl -s -k -X GET https://localhost:5000/api/documents \
    -H "Authorization: Bearer $TOKEN" \
    -w "\nHTTP_CODE:%{http_code}")

HTTP_CODE=$(echo "$RESPONSE" | grep -o "HTTP_CODE:[0-9]*" | cut -d: -f2)
if [ "$HTTP_CODE" = "200" ]; then
    COUNT=$(echo "$RESPONSE" | head -n -1 | grep -o '"count":[0-9]*' | cut -d: -f2)
    echo "✅ Success (found $COUNT documents)"
else
    echo "❌ Failed (HTTP $HTTP_CODE)"
fi

echo "Test completed!"