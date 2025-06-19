#!/bin/bash

# Test deleting the documents we just created with the new UUIDs

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

# Document IDs from the most recent test
doc_ids=(
    "doc-1750365036-615165085"
    "doc-1750365036-1640856375"
    "doc-1750365036-1797249388"
    "doc-1750365037-1950031968"
    "doc-1750365037-1215025273"
    "doc-1750365037-506879382"
    "doc-1750365037-879670551"
    "doc-1750365037-1076718402"
    "doc-1750365038-121189830"
    "doc-1750365038-1404657248"
)

# Delete each document with extra focus on the 6th one where crashes occurred
for i in "${!doc_ids[@]}"; do
    doc_id="${doc_ids[$i]}"
    echo
    echo "=== Deleting document $((i+1)): $doc_id ==="
    
    if [ $((i+1)) -eq 6 ]; then
        echo "🚨 CRITICAL: This is the 6th deletion where crashes occurred before!"
    fi
    
    delete_response=$(curl -k -s -X DELETE \
        -H "Authorization: Bearer $token" \
        "$HOST/api/documents/$doc_id")
    
    echo "Delete response: $delete_response"
    
    # Check if server is still alive
    if [ -z "$delete_response" ]; then
        echo "❌ ERROR: Empty response - server may have crashed!"
        break
    else
        echo "✅ SUCCESS: Server responded correctly"
    fi
    
    sleep 0.2
done

echo
echo "=== Delete test complete - checking server status ==="

# Final server status check
if kill -0 $(cat /opt/jdbx/build/var/jdbxd.pid) 2>/dev/null; then
    echo "🎉 SUCCESS: Server is still running after all deletions!"
else
    echo "❌ FAILURE: Server crashed during deletions"
fi