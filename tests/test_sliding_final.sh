#!/bin/bash

echo "Testing Sliding Session Timeouts - Final Test"
echo "============================================="

# Login to get a fresh token
echo -e "\n1. Logging in..."
LOGIN_RESPONSE=$(curl -s -X POST http://localhost:5000/api/login \
  -H "Content-Type: application/json" \
  -d '{"username": "admin", "password": "admin"}')

TOKEN=$(echo "$LOGIN_RESPONSE" | jq -r '.token')
echo "Got token: ${TOKEN:0:30}..."

# Make initial request and note time
echo -e "\n2. Making initial authenticated request..."
INITIAL_TIME=$(date +%s)
curl -s -X GET http://localhost:5000/api/health \
  -H "Authorization: Bearer $TOKEN" > /dev/null
echo "Initial request at: $(date '+%Y-%m-%d %H:%M:%S')"

# Wait a bit
echo -e "\n3. Waiting 3 seconds..."
sleep 3

# Make another request
echo -e "\n4. Making second authenticated request..."
SECOND_TIME=$(date +%s)
curl -s -X GET http://localhost:5000/api/collections \
  -H "Authorization: Bearer $TOKEN" > /dev/null
echo "Second request at: $(date '+%Y-%m-%d %H:%M:%S')"

# Check the logs for our authentication events
echo -e "\n5. Checking authentication logs..."
tail -50 /opt/jsondb/build/var/jsondb.log | grep -E "(Authenticating token: ${TOKEN:0:20}|Token authentication successful|Extended session)" | tail -10

# Decode the JWT to see expiration
echo -e "\n6. Decoding JWT to check expiration..."
# Extract payload
PAYLOAD=$(echo $TOKEN | cut -d. -f2)
# Add padding if needed
PAYLOAD_LEN=$((${#PAYLOAD} % 4))
if [ $PAYLOAD_LEN -eq 2 ]; then
    PAYLOAD="${PAYLOAD}=="
elif [ $PAYLOAD_LEN -eq 3 ]; then
    PAYLOAD="${PAYLOAD}="
fi
# Decode
DECODED=$(echo "$PAYLOAD" | base64 -d 2>/dev/null)
echo "Token payload: $DECODED"

echo -e "\nDone!"