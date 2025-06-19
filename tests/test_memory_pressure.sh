#!/bin/bash

# Test memory pressure scenario - rapid create/delete
API="https://localhost:5000/api"

# Login first
echo "Logging in..."
TOKEN=$(curl -sk "$API/auth/login" -H "Content-Type: application/json" \
  -d '{"username": "admin", "password": "secure123456789"}' | \
  grep -o '"token":"[^"]*' | cut -d'"' -f4)

if [ -z "$TOKEN" ]; then
    echo "Failed to login!"
    exit 1
fi

echo "Token: $TOKEN"

# Test rapid create/delete cycle
echo "Testing rapid create/delete (memory pressure test)..."

for i in {1..50}; do
    echo -n "Create/Delete cycle $i: "
    
    # Create document
    DOC_ID=$(curl -sk "$API/documents" \
        -H "Authorization: Bearer $TOKEN" \
        -H "Content-Type: application/json" \
        -d "{\"title\": \"Test Document $i\", \"content\": \"Memory pressure test content that should stress the skiplist and checkpoint memory system\", \"iteration\": $i}" | \
        grep -o '"uuid":"[^"]*' | cut -d'"' -f4)
    
    if [ -z "$DOC_ID" ]; then
        echo "FAILED to create document"
        exit 1
    fi
    
    echo -n "Created $DOC_ID ... "
    
    # Immediately delete it
    DELETE_RESP=$(curl -sk -X DELETE "$API/documents/$DOC_ID" \
        -H "Authorization: Bearer $TOKEN")
    
    if echo "$DELETE_RESP" | grep -q "error"; then
        echo "FAILED to delete"
        echo "$DELETE_RESP"
        exit 1
    fi
    
    echo "Deleted OK"
done

echo "Test completed successfully - no crashes!"

# Check server status
if curl -sk "$API/health" | grep -q "ok"; then
    echo "Server is still healthy!"
else
    echo "Server health check failed!"
    exit 1
fi