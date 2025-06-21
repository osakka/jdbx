#!/bin/bash
# Minimal test to isolate crash

echo "=== Test 1: Simple health check ==="
curl -k -s https://localhost:5000/api/health | jq -r '.status'

echo -e "\n=== Test 2: Login ==="
TOKEN=$(curl -k -s -X POST https://localhost:5000/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"secure123456789"}' | jq -r '.token')
echo "Token received: ${TOKEN:0:20}..."

echo -e "\n=== Test 3: Access with token ==="
curl -k -s https://localhost:5000/api/documents?limit=1 \
  -H "Authorization: Bearer $TOKEN" | jq -r '.count'

echo -e "\n=== Test 4: Logout ==="
curl -k -s -X POST https://localhost:5000/api/auth/logout \
  -H "Authorization: Bearer $TOKEN" | jq .

echo -e "\n=== Test 5: Access after logout ==="
curl -k -s https://localhost:5000/api/documents \
  -H "Authorization: Bearer $TOKEN" | jq .