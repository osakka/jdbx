#!/bin/bash
# Minimal test to trigger concurrent crash

BASE_URL="https://localhost:5000"

echo "Minimal Concurrent Crash Test"
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

# Function to create a document
create_document() {
    local id=$1
    echo "[$$] Creating document $id..." >&2
    
    local response=$(curl -k -s -w "\n%{http_code}" -X POST "$BASE_URL/api/documents" \
        -H "Authorization: Bearer $TOKEN" \
        -H "Content-Type: application/json" \
        -d "{\"name\":\"Test $id\",\"content\":\"Content $id\"}" 2>/dev/null)
    
    local status=$(echo "$response" | tail -1)
    
    if [ "$status" = "201" ]; then
        echo "[$$] ✅ Request $id: SUCCESS" >&2
    else
        echo "[$$] ❌ Request $id: Failed with status $status" >&2
    fi
}

# Test just 2 concurrent requests
echo -e "\nTesting just 2 concurrent requests..."
create_document 1 &
PID1=$!
create_document 2 &
PID2=$!

# Wait for both
wait $PID1
wait $PID2

# Check if server is still alive
echo -e "\nChecking server health..."
if curl -k -s "$BASE_URL/api/health" > /dev/null 2>&1; then
    echo "✅ Server is still healthy"
else
    echo "💥 Server crashed with just 2 concurrent requests!"
fi