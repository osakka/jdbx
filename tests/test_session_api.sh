#!/bin/bash

echo "=== Testing Session Persistence ==="

# Get auth token
echo "1. Logging in..."
TOKEN=$(curl -s -X POST http://localhost:5000/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"admin"}' | jq -r '.token')

echo "Token: ${TOKEN:0:20}..."

# Insert a document directly into _sessions to test
echo -e "\n2. Inserting test session document..."
SESSION_ID=$(curl -s -X POST http://localhost:5000/api/collections/_sessions \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "user_id": "test-user-123",
    "username": "testuser",
    "token": "test-token-abc",
    "created_at": "2025-01-26T19:30:00Z",
    "expires_at": "2025-01-26T20:00:00Z",
    "active": true,
    "ip_address": "127.0.0.1",
    "user_agent": "Test/1.0"
  }' | jq -r '._id')

echo "Created session ID: $SESSION_ID"

# Query sessions
echo -e "\n3. Querying all sessions..."
curl -s -H "Authorization: Bearer $TOKEN" http://localhost:5000/api/collections/_sessions | jq '.documents | length as $count | {count: $count, sessions: .}'

# Wait for persistence
echo -e "\n4. Waiting 35 seconds for persistence..."
sleep 35

# Query again
echo -e "\n5. Querying sessions after persistence wait..."
curl -s -H "Authorization: Bearer $TOKEN" http://localhost:5000/api/collections/_sessions | jq '.documents | length as $count | {count: $count, sessions: .}'

# Restart server
echo -e "\n6. Restarting server..."
cd /opt/jsondb && ./build/jsondb_runtime.sh stop
sleep 2
cd /opt/jsondb && ./build/jsondb_runtime.sh start
sleep 3

# Login again
echo -e "\n7. Logging in after restart..."
NEW_TOKEN=$(curl -s -X POST http://localhost:5000/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"admin"}' | jq -r '.token')

# Query sessions after restart
echo -e "\n8. Querying sessions after restart..."
curl -s -H "Authorization: Bearer $NEW_TOKEN" http://localhost:5000/api/collections/_sessions | jq '.documents | length as $count | {count: $count, sessions: .}'

echo -e "\nDone."