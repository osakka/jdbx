#!/bin/bash
# Test N-1 byte fix with curl over HTTPS

echo "Testing N-1 Byte Fix with curl over HTTPS"
echo

# Login to get token
TOKEN=$(curl -k -s -X POST https://localhost:5000/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"secure123456789"}' | \
  grep -o '"token":"[^"]*' | cut -d'"' -f4)

if [ -z "$TOKEN" ]; then
  echo "Login failed"
  exit 1
fi

echo "Login successful"
echo

# Test various sizes
for SIZE in 100 1000 5000 10000 50000; do
  # Generate data
  DATA=$(python3 -c "print('x' * $SIZE)")
  
  # Send request
  RESPONSE=$(curl -k -s -w "\n%{http_code}" -X POST https://localhost:5000/api/documents \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d "{\"title\":\"Test $SIZE bytes\",\"type\":\"curl-test\",\"data\":\"$DATA\"}")
  
  HTTP_CODE=$(echo "$RESPONSE" | tail -n1)
  
  if [ "$HTTP_CODE" = "201" ]; then
    echo "✅ $SIZE bytes: Success"
  else
    echo "❌ $SIZE bytes: HTTP $HTTP_CODE"
  fi
done

echo
echo "curl HTTPS test complete!"