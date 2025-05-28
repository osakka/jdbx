#!/bin/bash

echo "Testing Sliding Session Timeouts"
echo "================================"

# Login to get a token
echo -e "\n1. Logging in..."
LOGIN_RESPONSE=$(curl -s -X POST http://localhost:5000/api/v1/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username": "admin", "password": "admin"}')

TOKEN=$(echo "$LOGIN_RESPONSE" | grep -o '"token":"[^"]*' | cut -d'"' -f4)
echo "Got token: ${TOKEN:0:30}..."

# Check current session details
echo -e "\n2. Checking initial session state..."
SESSIONS=$(curl -s -X GET http://localhost:5000/api/v1/sessions \
  -H "Authorization: Bearer $TOKEN")
echo "Sessions response: $SESSIONS" | head -50

# Make a request to trigger session extension
echo -e "\n3. Making authenticated request (should extend session)..."
sleep 2
HEALTH=$(curl -s -X GET http://localhost:5000/api/v1/health \
  -H "Authorization: Bearer $TOKEN")
echo "Health check: $HEALTH"

# Check session details again
echo -e "\n4. Checking session state after activity..."
sleep 1
SESSIONS2=$(curl -s -X GET http://localhost:5000/api/v1/sessions \
  -H "Authorization: Bearer $TOKEN")
echo "Updated sessions: $SESSIONS2" | head -50

# Check logs for session extension
echo -e "\n5. Checking logs for session extension..."
tail -20 /opt/jsondb/build/var/jsondb.log | grep -E "(Extended session|Authenticating|Token authentication)" || echo "No session extension logs found"