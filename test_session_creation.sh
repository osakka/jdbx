#!/bin/bash

echo "=== Testing Session Creation ==="

# Login and get full response
echo "1. Logging in..."
RESPONSE=$(curl -s -X POST http://localhost:5000/api/login \
  -H "Content-Type: application/json" \
  -H "User-Agent: Mozilla/5.0 Test" \
  -H "X-Real-IP: 192.168.1.100" \
  -d '{"username":"admin","password":"admin"}')

echo "Login response:"
echo "$RESPONSE" | jq .

# Extract token
TOKEN=$(echo "$RESPONSE" | jq -r .token)
USER_ID=$(echo "$RESPONSE" | jq -r .user_id)

echo -e "\n2. Token obtained: ${TOKEN:0:50}..."
echo "User ID: $USER_ID"

# Wait a moment
sleep 1

# Check sessions collection
echo -e "\n3. Checking _sessions collection..."
curl -s -X GET http://localhost:5000/api/collections/_sessions \
  -H "Authorization: Bearer $TOKEN" | jq .

# Check logs
echo -e "\n4. Recent log entries about sessions:"
tail -50 /opt/jsondb/build/var/jsondb.log | grep -i "session" | tail -10

echo -e "\nDone."