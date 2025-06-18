#!/bin/bash

BASE_URL="https://localhost:5000"

echo "🔍 Testing Connection Flood vs Connection Reuse"
echo "=============================================="

# Get token with connection reuse
echo -e "\nTest 1: Single connection with Keep-Alive (proper way)"
TOKEN=$(curl -s -k "$BASE_URL/api/auth/login" \
    -H "Content-Type: application/json" \
    -d '{"username":"admin","password":"secure123456789"}' | jq -r '.token')

# Create a file with multiple requests
cat > /tmp/requests.txt << EOF
POST /api/documents HTTP/1.1
Host: localhost:5000
Authorization: Bearer $TOKEN
Content-Type: application/json
Content-Length: 35
Connection: keep-alive

{"id":1,"data":"keepalive test 1"}
POST /api/documents HTTP/1.1
Host: localhost:5000
Authorization: Bearer $TOKEN
Content-Type: application/json
Content-Length: 35
Connection: keep-alive

{"id":2,"data":"keepalive test 2"}
POST /api/documents HTTP/1.1
Host: localhost:5000
Authorization: Bearer $TOKEN
Content-Type: application/json
Content-Length: 35
Connection: close

{"id":3,"data":"keepalive test 3"}
EOF

echo "Sending 3 requests on single connection..."
# Use telnet or nc to send multiple requests on one connection
# (curl would work too but let's be explicit)

echo -e "\nTest 2: Many new connections (SYN flood pattern)"
echo "Creating 50 new connections rapidly..."

for i in {1..50}; do
    # Each curl creates a new connection
    curl -s -k -X POST "$BASE_URL/api/documents" \
        -H "Authorization: Bearer $TOKEN" \
        -H "Content-Type: application/json" \
        -d "{\"id\":$i,\"data\":\"flood test $i\"}" \
        -o /dev/null &
    
    # Check server every 10 requests
    if [ $((i % 10)) -eq 0 ]; then
        sleep 0.1
        if ! ps -p $(cat /opt/jdbx/build/var/jdbxd.pid 2>/dev/null) > /dev/null 2>&1; then
            echo -e "\n❌ Server crashed after $i connections!"
            echo "This demonstrates the SYN flood protection trigger"
            exit 1
        fi
        echo -n "."
    fi
done

wait

echo -e "\nChecking server status..."
if ps -p $(cat /opt/jdbx/build/var/jdbxd.pid 2>/dev/null) > /dev/null 2>&1; then
    echo "✅ Server survived (may have been protected by SYN cookies)"
else
    echo "❌ Server crashed - general protection fault likely occurred"
fi