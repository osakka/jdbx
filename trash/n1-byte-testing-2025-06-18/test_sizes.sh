#!/bin/bash
# Test different document sizes to find the limit

TOKEN=$(curl -k -s https://localhost:5000/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"secure123456789"}' | jq -r .token)

echo "Testing document size limits..."

for size in 1000 2000 3000 4000 5000 6000; do
  DATA=$(python3 -c "print('x' * $size)")
  
  RESPONSE=$(curl -k -s -w "\nHTTP_CODE:%{http_code}" \
    -X POST https://localhost:5000/api/documents \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d "{\"title\":\"Size test $size\",\"type\":\"size-test\",\"data\":\"$DATA\"}")
  
  HTTP_CODE=$(echo "$RESPONSE" | grep HTTP_CODE | cut -d: -f2)
  BODY=$(echo "$RESPONSE" | grep -v HTTP_CODE)
  
  if [ "$HTTP_CODE" = "201" ]; then
    echo "✅ $size bytes: Success"
  else
    echo "❌ $size bytes: HTTP $HTTP_CODE"
    echo "   Response: $BODY" | head -c 100
  fi
done