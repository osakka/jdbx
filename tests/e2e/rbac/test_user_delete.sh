#!/bin/bash

# Test user delete operation specifically

BASE_URL="https://localhost:5000"
CURL_OPTS="-s -k"

# Login as admin
echo "1. Login as admin..."
TOKEN=$(curl $CURL_OPTS -X POST "$BASE_URL/api/auth/login" \
    -H "Content-Type: application/json" \
    -d '{"username":"admin","password":"secure123456789"}' | jq -r '.token')

if [[ -z "$TOKEN" || "$TOKEN" == "null" ]]; then
    echo "Failed to login"
    exit 1
fi

echo "Token: ${TOKEN:0:20}..."

# Create a test user
echo -e "\n2. Creating test user..."
CREATE_RESPONSE=$(curl $CURL_OPTS -X POST "$BASE_URL/api/users" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d '{"username":"testdelete","password":"password123"}')

echo "Create response: $CREATE_RESPONSE" | jq

# Extract user ID from create response
USER_ID_FROM_CREATE=$(echo "$CREATE_RESPONSE" | jq -r '.user.id // .id // .uuid')
echo "User ID from create: $USER_ID_FROM_CREATE"

# Get the actual document ID from the user list
echo -e "\n2.5. Getting actual document ID from user list..."
USER_LIST=$(curl $CURL_OPTS -X GET "$BASE_URL/api/users" \
    -H "Authorization: Bearer $TOKEN")
USER_DOC_ID=$(echo "$USER_LIST" | jq -r '.users[] | select(.username == "testdelete") | .id')
echo "User document ID: $USER_DOC_ID"

# Use the document ID for deletion
USER_ID="$USER_DOC_ID"

if [[ -z "$USER_ID" || "$USER_ID" == "null" ]]; then
    echo "Failed to create user"
    exit 1
fi

# Verify user exists
echo -e "\n3. Verifying user exists..."
curl $CURL_OPTS -X GET "$BASE_URL/api/users" \
    -H "Authorization: Bearer $TOKEN" | jq '.users[] | select(.username == "testdelete")'

# Try to delete the user
echo -e "\n4. Attempting to delete user..."
DELETE_RESPONSE=$(curl -w "\nHTTP_STATUS:%{http_code}" $CURL_OPTS -X DELETE "$BASE_URL/api/users/$USER_ID" \
    -H "Authorization: Bearer $TOKEN")

HTTP_STATUS=$(echo "$DELETE_RESPONSE" | grep "HTTP_STATUS:" | cut -d: -f2)
BODY=$(echo "$DELETE_RESPONSE" | sed '/HTTP_STATUS:/d')

echo "HTTP Status: $HTTP_STATUS"
echo "Response body: $BODY"

# Check if delete was successful (204 No Content or 200 with success)
if [[ "$HTTP_STATUS" == "204" ]]; then
    echo -e "\n✅ Delete successful (204 No Content)"
elif [[ "$HTTP_STATUS" == "200" && "$BODY" =~ "success" ]]; then
    echo -e "\n✅ Delete successful (200 with success response)"
else
    echo -e "\n❌ Delete failed"
    echo "Expected: 204 No Content or 200 with success"
    echo "Got: $HTTP_STATUS with body: $BODY"
fi

# Verify user is deleted
echo -e "\n5. Verifying user is deleted..."
VERIFY_RESPONSE=$(curl $CURL_OPTS -X GET "$BASE_URL/api/users" \
    -H "Authorization: Bearer $TOKEN")

DELETED_USER=$(echo "$VERIFY_RESPONSE" | jq '.users[] | select(.username == "testdelete")' 2>/dev/null)

if [[ -z "$DELETED_USER" ]]; then
    echo "✅ User successfully deleted from database"
else
    echo "❌ User still exists in database!"
    echo "$DELETED_USER" | jq
fi