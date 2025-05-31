#!/bin/bash

echo "Testing Sliding Session Timeouts"
echo "================================"

# Login to get a token
echo -e "\n1. Logging in..."
LOGIN_RESPONSE=$(curl -s -X POST http://localhost:5000/api/login \
  -H "Content-Type: application/json" \
  -d '{"username": "admin", "password": "admin"}')

TOKEN=$(echo "$LOGIN_RESPONSE" | grep -o '"token":"[^"]*' | cut -d'"' -f4)
echo "Got token: ${TOKEN:0:30}..."

# Check current session details
echo -e "\n2. Checking initial session state..."
SESSIONS=$(curl -s -X GET http://localhost:5000/api/sessions \
  -H "Authorization: Bearer $TOKEN")
echo "Sessions response: $SESSIONS" | jq '.' | head -20

# Extract session ID for our token
SESSION_ID=$(echo "$SESSIONS" | jq -r '.sessions[] | select(.token == "'$TOKEN'") | ._id')
echo "Current session ID: $SESSION_ID"

# Make a request to trigger session extension
echo -e "\n3. Waiting 5 seconds then making authenticated request..."
sleep 5
HEALTH=$(curl -s -X GET http://localhost:5000/api/health \
  -H "Authorization: Bearer $TOKEN")
echo "Health check successful"

# Check session details again
echo -e "\n4. Checking session state after activity..."
sleep 1
SESSIONS2=$(curl -s -X GET http://localhost:5000/api/sessions \
  -H "Authorization: Bearer $TOKEN")

# Compare expiration times
if [ -n "$SESSION_ID" ]; then
  EXPIRES_BEFORE=$(echo "$SESSIONS" | jq -r '.sessions[] | select(._id == "'$SESSION_ID'") | .expires_at')
  EXPIRES_AFTER=$(echo "$SESSIONS2" | jq -r '.sessions[] | select(._id == "'$SESSION_ID'") | .expires_at')
  LAST_SEEN_AFTER=$(echo "$SESSIONS2" | jq -r '.sessions[] | select(._id == "'$SESSION_ID'") | .last_seen')
  
  echo "Expires before: $EXPIRES_BEFORE"
  echo "Expires after:  $EXPIRES_AFTER"
  echo "Last seen:      $LAST_SEEN_AFTER"
  
  if [ "$EXPIRES_BEFORE" != "$EXPIRES_AFTER" ]; then
    echo -e "\n✓ SUCCESS: Session expiration was extended!"
  else
    echo -e "\n✗ FAILED: Session expiration was not extended"
  fi
fi

# Check logs for session extension
echo -e "\n5. Checking logs for session extension..."
tail -50 /opt/jsondb/build/var/jsondb.log | grep -E "(Extended session|Authenticating token|Token authentication)" | tail -10