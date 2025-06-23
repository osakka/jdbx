#!/bin/bash

# Comprehensive JWT cache performance test

echo "JWT Cache Performance Test"
echo "========================"

# First, login to get a token
echo -e "\n1. Getting authentication token..."
LOGIN_RESPONSE=$(curl -s -X POST http://localhost:5000/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username": "admin", "password": "admin"}')

TOKEN=$(echo "$LOGIN_RESPONSE" | grep -o '"token":"[^"]*' | cut -d'"' -f4)

if [ -z "$TOKEN" ]; then
  echo "Failed to get token. Response: $LOGIN_RESPONSE"
  exit 1
fi

echo "Got token successfully"

# Clear logs to get fresh results
echo -e "\n2. Clearing logs..."
echo "" > /opt/jsondb/build/var/test_performance.log

# Make 100 requests to test cache performance
echo -e "\n3. Making 100 authenticated requests..."
TOTAL_TIME=0
MIN_TIME=999999
MAX_TIME=0
CACHE_HITS=0

for i in {1..100}; do
  START_TIME=$(date +%s%N)
  
  # Make request with authentication
  RESPONSE=$(curl -s -X GET http://localhost:5000/api/collections \
    -H "Authorization: Bearer $TOKEN" \
    -o /dev/null \
    -w "%{http_code}")
  
  END_TIME=$(date +%s%N)
  ELAPSED=$((($END_TIME - $START_TIME) / 1000000))
  
  # Update statistics
  TOTAL_TIME=$((TOTAL_TIME + ELAPSED))
  
  if [ $ELAPSED -lt $MIN_TIME ]; then
    MIN_TIME=$ELAPSED
  fi
  
  if [ $ELAPSED -gt $MAX_TIME ]; then
    MAX_TIME=$ELAPSED
  fi
  
  # Print progress every 10 requests
  if [ $((i % 10)) -eq 0 ]; then
    echo -n "."
  fi
done

echo -e "\n\n4. Results:"
echo "===================="
AVERAGE_TIME=$((TOTAL_TIME / 100))

echo "Total requests: 100"
echo "Average response time: ${AVERAGE_TIME}ms"
echo "Minimum response time: ${MIN_TIME}ms"
echo "Maximum response time: ${MAX_TIME}ms"

# Count cache hits
echo -e "\n5. Cache Statistics:"
CACHE_HITS=$(grep -c "JWT cache hit" /opt/jsondb/build/var/jsondb.log | tail -1)
echo "JWT cache hits: ${CACHE_HITS}"

# Show cache initialization
echo -e "\n6. Cache Info:"
grep "JWT cache initialized" /opt/jsondb/build/var/jsondb.log | tail -1

# Show sample of cache activity
echo -e "\n7. Recent cache activity:"
grep -E "(JWT cache hit|JWT cached|jwt_cache)" /opt/jsondb/build/var/jsondb.log | tail -5

echo -e "\nDone!"