#!/bin/bash

BASE_URL="https://localhost:5000"

# Get auth token
TOKEN=$(curl -s -k "$BASE_URL/api/auth/login" \
    -H "Content-Type: application/json" \
    -d '{"username":"admin","password":"secure123456789"}' | jq -r '.token')

echo "🔍 Testing Discovery Sequence"
echo "============================="

# First, trigger the failed requests like discovery test does
echo "1. Testing 5KB document (will fail with 400)..."
MEDIUM_CONTENT=$(head -c 5000 < /dev/zero | tr '\0' 'A')
curl -s -k -X POST "$BASE_URL/api/documents" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d "{\"title\":\"Medium Document\",\"content\":\"$MEDIUM_CONTENT\"}" \
    -w "HTTP:%{http_code}\n" -o /dev/null

echo "2. Testing 50KB document (will fail with 400)..."
LARGE_CONTENT=$(head -c 50000 < /dev/zero | tr '\0' 'B')
curl -s -k -X POST "$BASE_URL/api/documents" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d "{\"title\":\"Large Document\",\"content\":\"$LARGE_CONTENT\"}" \
    -w "HTTP:%{http_code}\n" -o /dev/null

echo "3. Testing large array (will fail with 400)..."
ARRAY_JSON='{"items":['
for i in {1..1000}; do
    ARRAY_JSON+="{\"id\":$i,\"value\":\"item$i\"}"
    [ $i -lt 1000 ] && ARRAY_JSON+=","
done
ARRAY_JSON+=']}'

curl -s -k -X POST "$BASE_URL/api/documents" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d "$ARRAY_JSON" \
    -w "HTTP:%{http_code}\n" -o /dev/null

# Check if server survived the 400 errors
if ! ps -p $(cat /opt/jdbx/build/var/jdbxd.pid 2>/dev/null) > /dev/null 2>&1; then
    echo "❌ Server crashed after 400 errors!"
    exit 1
fi

echo -e "\n✅ Server survived the 400 errors"

# Now do rapid requests
echo -e "\n4. Starting rapid sequential requests..."
for i in {1..100}; do
    if ! ps -p $(cat /opt/jdbx/build/var/jdbxd.pid 2>/dev/null) > /dev/null 2>&1; then
        echo -e "\n❌ Server crashed at rapid request $i!"
        exit 1
    fi
    
    curl -s -k -X POST "$BASE_URL/api/documents" \
        -H "Authorization: Bearer $TOKEN" \
        -H "Content-Type: application/json" \
        -d "{\"id\":$i,\"data\":\"rapid test $i\"}" \
        -w "" -o /dev/null
    
    echo -n "."
    [ $((i % 25)) -eq 0 ] && echo -n " ($i)"
done

echo -e "\n✅ All tests completed!"