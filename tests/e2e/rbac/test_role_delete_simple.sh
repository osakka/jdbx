#\!/bin/bash

echo "Testing role deletion CPU usage (no SSL)..."

# Login as admin
echo "1. Login as admin..."
LOGIN_RESPONSE=$(curl -s -X POST http://localhost:5000/api/auth/login \
    -H "Content-Type: application/json" \
    -d '{
        "username": "admin",
        "password": "secure123456789"
    }')
TOKEN=$(echo "$LOGIN_RESPONSE"  < /dev/null |  jq -r '.token')
echo "Token: ${TOKEN:0:20}..."

# Create a user
echo "2. Creating test user..."
USER_RESPONSE=$(curl -s -X POST http://localhost:5000/api/rbac/users \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d '{
        "username": "testuser_cpu",
        "password": "test123",
        "email": "test@example.com"
    }')
USER_ID=$(echo "$USER_RESPONSE" | jq -r '.id')
echo "User ID: $USER_ID"

# Create a role
echo "3. Creating test role..."
ROLE_RESPONSE=$(curl -s -X POST http://localhost:5000/api/rbac/roles \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d '{
        "name": "test_role_cpu",
        "permissions": {
            "users": ["read", "create"],
            "documents": ["read", "create", "update"]
        }
    }')
ROLE_ID=$(echo "$ROLE_RESPONSE" | jq -r '.id')
echo "Role ID: $ROLE_ID"

# Assign role to user
echo "4. Assigning role to user..."
ASSIGN_RESPONSE=$(curl -s -X POST "http://localhost:5000/api/rbac/roles/$ROLE_ID/users/$USER_ID" \
    -H "Authorization: Bearer $TOKEN")
echo "Assign response: $ASSIGN_RESPONSE"

# Monitor CPU before deletion
echo "5. Monitoring CPU before deletion..."
PID=$(pgrep jdbxd)
echo "Server PID: $PID"
ps -p $PID -o pid,pcpu,pmem,cmd --no-headers

# Monitor CPU continuously in background
(while true; do 
    if ps -p $PID > /dev/null 2>&1; then
        CPU=$(ps -p $PID -o pcpu --no-headers)
        if (( $(echo "$CPU > 50" | bc -l) )); then
            echo "HIGH CPU DETECTED: $CPU%"
        fi
    else
        echo "SERVER CRASHED\!"
        break
    fi
    sleep 0.1
done) &
MONITOR_PID=$\!

# Delete the user first (this should clean up role assignments)
echo "6. Deleting user..."
curl -s -X DELETE "http://localhost:5000/api/rbac/users/$USER_ID" \
    -H "Authorization: Bearer $TOKEN" \
    -w "\nHTTP Status: %{http_code}\n"

sleep 1

# Now delete the role
echo "7. Deleting role..."
curl -s -X DELETE "http://localhost:5000/api/rbac/roles/$ROLE_ID" \
    -H "Authorization: Bearer $TOKEN" \
    -w "\nHTTP Status: %{http_code}\n"

sleep 1

# Stop monitoring
kill $MONITOR_PID 2>/dev/null

# Check final CPU
echo "8. Final CPU check..."
ps -p $PID -o pid,pcpu,pmem,cmd --no-headers || echo "Server crashed\!"

# Test server responsiveness
echo "9. Testing server responsiveness..."
curl -s -X GET http://localhost:5000/api/health \
    -w "\nHTTP Status: %{http_code}\n" || echo "Server not responding"

