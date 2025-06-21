
#\!/bin/bash
# Simple logout test

BASE_URL="https://localhost:5000"

# Login
echo "=== LOGIN ==="
LOGIN_RESP=$(curl -k -s -X POST "$BASE_URL/api/auth/login" \
  -H "Content-Type: application/json" \
  -d "{\"username\":\"admin\",\"password\":\"secure123456789\"}")

TOKEN=$(echo "$LOGIN_RESP"  < /dev/null |  jq -r ".token")
echo "Token: ${TOKEN:0:50}..."

# Test access before logout
echo -e "\n=== ACCESS BEFORE LOGOUT ==="
curl -k -s -X GET "$BASE_URL/api/documents?limit=1" \
  -H "Authorization: Bearer $TOKEN" | jq -r ".documents | length"

# Logout
echo -e "\n=== LOGOUT ==="
curl -k -s -X POST "$BASE_URL/api/auth/logout" \
  -H "Authorization: Bearer $TOKEN" | jq .

# Test access after logout
echo -e "\n=== ACCESS AFTER LOGOUT ==="
curl -k -s -X GET "$BASE_URL/api/documents?limit=1" \
  -H "Authorization: Bearer $TOKEN" | jq .

