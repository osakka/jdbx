#\!/bin/bash

BASE_URL="https://localhost:5000"
CURL_OPTS="-s -k"

echo "=== JDBX Logout Security Test ==="
echo

# 1. Admin login
echo "1. Admin login..."
ADMIN_RESPONSE=$(curl $CURL_OPTS -X POST "$BASE_URL/api/auth/login" \
    -H "Content-Type: application/json" \
    -d '{"username":"admin","password":"secure123456789"}')
ADMIN_TOKEN=$(echo "$ADMIN_RESPONSE"  < /dev/null |  jq -r '.token' 2>/dev/null)
echo "Admin token: ${ADMIN_TOKEN:0:50}..."

# 2. Test access with token
echo -e "\n2. Test access BEFORE logout..."
BEFORE=$(curl $CURL_OPTS -X GET "$BASE_URL/api/users" \
    -H "Authorization: Bearer $ADMIN_TOKEN" | jq -r '.users | length' 2>/dev/null)
if [[ -n "$BEFORE" ]]; then
    echo "✅ Access allowed - found $BEFORE users"
else
    echo "❌ Access denied before logout"
fi

# 3. Logout
echo -e "\n3. Logging out..."
LOGOUT=$(curl $CURL_OPTS -X POST "$BASE_URL/api/auth/logout" \
    -H "Authorization: Bearer $ADMIN_TOKEN")
echo "Logout response: $LOGOUT"

# 4. Test access after logout
echo -e "\n4. Test access AFTER logout..."
AFTER=$(curl $CURL_OPTS -X GET "$BASE_URL/api/users" \
    -H "Authorization: Bearer $ADMIN_TOKEN")
ERROR=$(echo "$AFTER" | jq -r '.error' 2>/dev/null)

if [[ "$ERROR" == "Unauthorized" ]]; then
    echo "✅ SECURITY PASS: Access denied after logout"
else
    echo "❌ SECURITY FAIL: Access still allowed after logout"
    echo "Response: $AFTER"
fi
