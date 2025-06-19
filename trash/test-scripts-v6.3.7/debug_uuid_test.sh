#!/bin/bash

# Simple test to debug UUID corruption

HOST="https://localhost:5000"
USERNAME="admin"
PASSWORD="secure123456789"

# Login
echo "=== Logging in ==="
auth_response=$(curl -k -s -X POST \
    -H "Content-Type: application/json" \
    -d '{"username":"'$USERNAME'","password":"'$PASSWORD'"}' \
    "$HOST/api/auth/login")

echo "Auth response: $auth_response"

token=$(echo "$auth_response" | grep -o '"token":"[^"]*' | cut -d'"' -f4)
echo "Token: ${token:0:20}..."

# Perform 5 create-delete cycles with detailed output
for i in {1..5}; do
    echo
    echo "=== Cycle $i ==="
    
    # Create document
    echo "Creating document $i..."
    create_response=$(curl -k -s -X POST \
        -H "Content-Type: application/json" \
        -H "Authorization: Bearer $token" \
        -d '{"title":"Test Doc '$i'","content":"Test content"}' \
        "$HOST/api/documents")
    
    echo "Create response: $create_response"
    
    # Extract UUID 
    doc_id=$(echo "$create_response" | grep -o '"uuid":"[^"]*' | cut -d'"' -f4)
    echo "Extracted UUID: '$doc_id'"
    
    if [ -n "$doc_id" ]; then
        # Delete document
        echo "Deleting document $doc_id..."
        delete_response=$(curl -k -s -X DELETE \
            -H "Authorization: Bearer $token" \
            "$HOST/api/documents/$doc_id")
        
        echo "Delete response: $delete_response"
    else
        echo "ERROR: No UUID extracted from create response!"
        break
    fi
    
    # Small delay
    sleep 0.5
done