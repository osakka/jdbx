#!/bin/bash

# Test deleting the documents we just created

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

# Document IDs from the previous test
doc_ids=(
    "doc-1750364827-1038423214"
    "doc-1750364827-1211590188"
    "doc-1750364827-618435100"
    "doc-1750364827-1972700639"
    "doc-1750364827-1318014680"
    "doc-1750364828-1884510620"
    "doc-1750364828-1607320012"
    "doc-1750364828-1244481586"
    "doc-1750364828-2044346232"
    "doc-1750364829-360053991"
)

# Delete each document
for i in "${!doc_ids[@]}"; do
    doc_id="${doc_ids[$i]}"
    echo
    echo "=== Deleting document $((i+1)): $doc_id ==="
    
    delete_response=$(curl -k -s -X DELETE \
        -H "Authorization: Bearer $token" \
        "$HOST/api/documents/$doc_id")
    
    echo "Delete response: $delete_response"
    
    # Check if server is still alive
    if [ -z "$delete_response" ]; then
        echo "ERROR: Empty response - server may have crashed!"
        break
    fi
    
    sleep 0.2
done

echo
echo "=== Delete test complete ==="