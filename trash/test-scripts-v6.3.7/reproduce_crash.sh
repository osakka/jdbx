#!/bin/bash

# Reproduce the crash at operation 5 by doing exactly 4 cycles first

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

# Perform exactly 4 create-delete cycles to reach the crash state
for i in {1..4}; do
    echo
    echo "=== Cycle $i (Pre-crash setup) ==="
    
    # Create document
    create_response=$(curl -k -s -X POST \
        -H "Content-Type: application/json" \
        -H "Authorization: Bearer $token" \
        -d '{"title":"Setup Doc '$i'","content":"Setup content"}' \
        "$HOST/api/documents")
    
    doc_id=$(echo "$create_response" | grep -o '"uuid":"[^"]*' | cut -d'"' -f4)
    echo "Created: $doc_id"
    
    # Delete document
    if [ -n "$doc_id" ]; then
        delete_response=$(curl -k -s -X DELETE \
            -H "Authorization: Bearer $token" \
            "$HOST/api/documents/$doc_id")
        echo "Deleted: $(echo "$delete_response" | grep -o '"success":[^,]*')"
    fi
    
    sleep 0.2
done

echo
echo "=== Now attempting the problematic 5th operation ==="

# Try operation 5 with detailed output
echo "Creating document 5 (the problematic one)..."
create_response=$(curl -k -s -w "\nHTTP_CODE:%{http_code}\nTIME_TOTAL:%{time_total}\n" -X POST \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer $token" \
    -d '{"title":"Crash Test Doc 5","content":"This should cause a crash"}' \
    "$HOST/api/documents")

echo "Response from operation 5:"
echo "$create_response"

# Check server status
sleep 1
if kill -0 $(cat /opt/jdbx/build/var/jdbxd.pid) 2>/dev/null; then
    echo "✅ Server survived operation 5!"
else
    echo "❌ Server crashed on operation 5!"
fi