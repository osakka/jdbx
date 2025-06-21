#!/bin/bash
# Precise logout test

BASE_URL="https://localhost:5000"

echo "=== Step 1: Login ==="
LOGIN_RESP=$(curl -k -s -X POST "$BASE_URL/api/auth/login" \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"secure123456789"}')

TOKEN=$(echo "$LOGIN_RESP" | jq -r '.token')
echo "Token: ${TOKEN:0:50}..."

echo -e "\n=== Step 2: Check session exists ==="
SESSIONS=$(curl -k -s "$BASE_URL/api/sessions" \
  -H "Authorization: Bearer $TOKEN")
  
ACTIVE_COUNT=$(echo "$SESSIONS" | jq -r '.sessions[] | select(.token == "'"$TOKEN"'" and .active == true)' | wc -l)
echo "Active sessions with this token: $ACTIVE_COUNT"

echo -e "\n=== Step 3: Access with token (should work) ==="
DOC_COUNT=$(curl -k -s "$BASE_URL/api/documents" \
  -H "Authorization: Bearer $TOKEN" | jq -r '.count')
echo "Document count: $DOC_COUNT"

echo -e "\n=== Step 4: Logout ==="
LOGOUT_RESP=$(curl -k -s -X POST "$BASE_URL/api/auth/logout" \
  -H "Authorization: Bearer $TOKEN")
echo "$LOGOUT_RESP" | jq .

echo -e "\n=== Step 5: Check session after logout ==="
SESSIONS_AFTER=$(curl -k -s "$BASE_URL/api/sessions" \
  -H "Authorization: Bearer $TOKEN")
  
ACTIVE_AFTER=$(echo "$SESSIONS_AFTER" | jq -r '.sessions[] | select(.token == "'"$TOKEN"'" and .active == true)' | wc -l)
echo "Active sessions with this token after logout: $ACTIVE_AFTER"

echo -e "\n=== Step 6: Access with token (should fail) ==="
ACCESS_RESP=$(curl -k -s "$BASE_URL/api/documents" \
  -H "Authorization: Bearer $TOKEN")

ERROR=$(echo "$ACCESS_RESP" | jq -r '.error // "none"')
if [[ "$ERROR" == "Unauthorized" ]]; then
    echo "✅ PASS: Access correctly denied"
else
    DOC_COUNT=$(echo "$ACCESS_RESP" | jq -r '.count // 0')
    echo "❌ FAIL: Access allowed! Document count: $DOC_COUNT"
fi