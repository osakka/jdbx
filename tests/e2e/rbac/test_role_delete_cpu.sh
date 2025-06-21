#\!/bin/bash

source "$(dirname "$0")/../../e2e/test_common.sh"

echo "Testing role deletion CPU usage..."

# Login as admin
TOKEN=$(login_admin  < /dev/null |  jq -r '.token')

# Create a user
echo "1. Creating test user..."
USER_RESPONSE=$(curl -s -X POST https://localhost:5000/api/rbac/users \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d '{
        "username": "testuser_cpu",
        "password": "test123",
        "email": "test@example.com"
    }')
USER_ID=$(echo "$USER_RESPONSE" | jq -r '.uuid')
echo "User ID: $USER_ID"

# Create a role
echo "2. Creating test role..."
ROLE_RESPONSE=$(curl -s -X POST https://localhost:5000/api/rbac/roles \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d '{
        "name": "test_role_cpu",
        "permissions": {
            "users": ["read", "create"],
            "documents": ["read", "create", "update"]
        }
    }')
ROLE_ID=$(echo "$ROLE_RESPONSE" | jq -r '.uuid')
echo "Role ID: $ROLE_ID"

# Assign role to user
echo "3. Assigning role to user..."
curl -s -X POST "https://localhost:5000/api/rbac/roles/$ROLE_ID/users/$USER_ID" \
    -H "Authorization: Bearer $TOKEN"

# Monitor CPU before deletion
echo "4. Monitoring CPU before deletion..."
PID=$(pgrep jdbxd)
echo "Server PID: $PID"
ps -p $PID -o pid,pcpu,pmem,cmd --no-headers

# Delete the role (not the user)
echo "5. Deleting role (this might cause high CPU)..."
time curl -s -X DELETE "https://localhost:5000/api/rbac/roles/$ROLE_ID" \
    -H "Authorization: Bearer $TOKEN" \
    -w "\nHTTP Status: %{http_code}\n"

# Give it a moment to process
sleep 2

# Check CPU after deletion
echo "6. Checking CPU after deletion..."
if ps -p $PID > /dev/null 2>&1; then
    ps -p $PID -o pid,pcpu,pmem,cmd --no-headers
else
    echo "Server crashed\!"
fi

# Try to query something to see if server is responsive
echo "7. Testing server responsiveness..."
curl -s -X GET https://localhost:5000/api/health \
    -w "\nHTTP Status: %{http_code}\n" || echo "Server not responding"

