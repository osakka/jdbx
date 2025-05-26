#\!/bin/bash

echo "Testing RBAC UI issue..."

# Get auth token
echo "1. Getting auth token..."
AUTH_RESPONSE=$(curl -s -X POST http://localhost:5000/api/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"admin"}')

TOKEN=$(echo $AUTH_RESPONSE  < /dev/null |  grep -o '"token":"[^"]*' | cut -d'"' -f4)
echo "Token obtained: ${TOKEN:0:20}..."

# Test RBAC roles API
echo -e "\n2. Testing RBAC roles API..."
ROLES_RESPONSE=$(curl -s -X GET http://localhost:5000/api/rbac/roles \
  -H "Authorization: Bearer $TOKEN")

echo "Roles API Response:"
echo "$ROLES_RESPONSE" | jq . 2>/dev/null || echo "$ROLES_RESPONSE"

# Check if response is just "admin"
if [ "$ROLES_RESPONSE" = "admin" ]; then
    echo "ERROR: API returned just 'admin' text\!"
else
    echo "API returned proper JSON"
fi

# Get the main page and check content
echo -e "\n3. Checking main page structure..."
curl -s http://localhost:5000/ | grep -o "rbac-view.*admin" | head -5

echo -e "\nDone."
