#!/bin/bash

# Simple test - just create one document
API="https://localhost:5000/api"

# Login first
echo "Logging in..."
TOKEN=$(curl -sk "$API/auth/login" -H "Content-Type: application/json" \
  -d '{"username": "admin", "password": "secure123456789"}' | \
  grep -o '"token":"[^"]*' | cut -d'"' -f4)

if [ -z "$TOKEN" ]; then
    echo "Failed to login!"
    exit 1
fi

echo "Token: $TOKEN"

# Create a single document
echo "Creating document..."
RESPONSE=$(curl -sk "$API/documents" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d '{"title": "Test Document", "content": "Simple test"}')

echo "Response: $RESPONSE"

if echo "$RESPONSE" | grep -q "uuid"; then
    echo "Success!"
else
    echo "Failed!"
    exit 1
fi