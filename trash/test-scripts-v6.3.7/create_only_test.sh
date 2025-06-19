#!/bin/bash

# Test only creating documents without deletion to isolate memory issues

HOST="https://localhost:5000"
USERNAME="admin"
PASSWORD="secure123456789"

# Login
echo "=== Logging in ==="
auth_response=$(curl -k -s -X POST \
    -H "Content-Type: application/json" \
    -d '{"username":"'$USERNAME'","password":"'$PASSWORD'"}' \
    "$HOST/api/auth/login")

token=$(echo "$auth_response" | grep -o '"token":"[^"]*' | cut -d'"' -f4)
echo "Token: ${token:0:20}..."

# Create 10 documents without deleting
for i in {1..10}; do
    echo
    echo "=== Creating document $i ==="
    
    create_response=$(curl -k -s -X POST \
        -H "Content-Type: application/json" \
        -H "Authorization: Bearer $token" \
        -d '{"title":"Test Doc '$i'","content":"Test content '$i'"}' \
        "$HOST/api/documents")
    
    echo "Create response: $create_response"
    
    # Check if server is still alive
    if [ -z "$create_response" ]; then
        echo "ERROR: Empty response - server may have crashed!"
        break
    fi
    
    sleep 0.2
done

echo
echo "=== Test complete ==="