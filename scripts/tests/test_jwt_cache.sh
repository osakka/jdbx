#!/bin/bash

# Test JWT cache performance

echo "Testing JWT cache performance..."

# First, login to get a token
echo "1. Logging in to get JWT token..."
LOGIN_RESPONSE=$(curl -s -X POST http://localhost:5000/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username": "admin", "password": "admin"}')

TOKEN=$(echo "$LOGIN_RESPONSE" | grep -o '"token":"[^"]*' | cut -d'"' -f4)

if [ -z "$TOKEN" ]; then
  echo "Failed to get token. Response: $LOGIN_RESPONSE"
  exit 1
fi

echo "Got token: ${TOKEN:0:20}..."

# Make multiple authenticated requests rapidly
echo -e "\n2. Making 10 rapid authenticated requests..."
for i in {1..10}; do
  START_TIME=$(date +%s%N)
  
  RESPONSE=$(curl -s -X GET http://localhost:5000/api/collections \
    -H "Authorization: Bearer $TOKEN" \
    -w "\n%{time_total}")
  
  END_TIME=$(date +%s%N)
  ELAPSED=$((($END_TIME - $START_TIME) / 1000000))
  
  # Extract time from curl
  CURL_TIME=$(echo "$RESPONSE" | tail -n1)
  
  echo "Request $i: ${ELAPSED}ms (curl: ${CURL_TIME}s)"
done

# Check logs for cache hits
echo -e "\n3. Checking logs for JWT cache activity..."
tail -n 50 /opt/jsondb/build/var/jsondb.log | grep -E "(JWT cache|Token authentication)" | tail -10

echo -e "\nDone!"