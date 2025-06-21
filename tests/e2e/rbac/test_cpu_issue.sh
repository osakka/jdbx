#!/bin/bash

# Test to reproduce high CPU issue during role deletion

BASE_URL="https://localhost:5000"
CURL_OPTS="-s -k"

echo "Starting JDBX server..."
JDBX_BOOTSTRAP_ADMIN_USER=admin JDBX_BOOTSTRAP_ADMIN_PASS=secure123456789 /opt/jdbx/build/jdbx_runtime.sh start
sleep 3

echo "1. Login as admin..."
TOKEN=$(curl $CURL_OPTS -X POST "$BASE_URL/api/auth/login" \
    -H "Content-Type: application/json" \
    -d '{"username":"admin","password":"secure123456789"}' | jq -r '.token')

echo "Token: ${TOKEN:0:20}..."

echo -e "\n2. Creating test user..."
USER_RESPONSE=$(curl $CURL_OPTS -X POST "$BASE_URL/api/users" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d '{"username":"cpuuser","password":"password123"}')

USER_ID=$(echo "$USER_RESPONSE" | jq -r '.user.id')
echo "User ID: $USER_ID"

echo -e "\n3. Creating test role..."
ROLE_RESPONSE=$(curl $CURL_OPTS -X POST "$BASE_URL/api/roles" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d '{"name":"cpurole","description":"Test role"}')

ROLE_ID=$(echo "$ROLE_RESPONSE" | jq -r '.role.id')
echo "Role ID: $ROLE_ID"

echo -e "\n4. Assigning role to user..."
curl $CURL_OPTS -X POST "$BASE_URL/api/rbac/roles/$ROLE_ID/users/$USER_ID" \
    -H "Authorization: Bearer $TOKEN"

echo -e "\n5. Monitor CPU before deletion..."
echo "CPU usage before:"
ps aux | grep jdbxd | grep -v grep | awk '{print "PID:", $2, "CPU:", $3"%"}'

echo -e "\n6. Deleting user first (this should remove from roles)..."
curl $CURL_OPTS -X DELETE "$BASE_URL/api/users/$USER_ID" \
    -H "Authorization: Bearer $TOKEN"

echo -e "\n7. Monitor CPU after user deletion..."
sleep 2
echo "CPU usage after user deletion:"
ps aux | grep jdbxd | grep -v grep | awk '{print "PID:", $2, "CPU:", $3"%"}'

echo -e "\n8. Now deleting role..."
curl $CURL_OPTS -X DELETE "$BASE_URL/api/roles/$ROLE_ID" \
    -H "Authorization: Bearer $TOKEN"

echo -e "\n9. Monitor CPU after role deletion..."
sleep 2
echo "CPU usage after role deletion:"
ps aux | grep jdbxd | grep -v grep | awk '{print "PID:", $2, "CPU:", $3"%"}'

echo -e "\n10. Check if server is still responsive..."
curl $CURL_OPTS -X GET "$BASE_URL/api/health" | jq -r '.status'

echo -e "\nTest complete. Check if CPU is still high."