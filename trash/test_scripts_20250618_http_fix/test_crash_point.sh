#!/bin/bash

BASE_URL="https://localhost:5000"

# Get auth token
echo "Getting auth token..."
TOKEN=$(curl -s -k "$BASE_URL/api/auth/login" \
    -H "Content-Type: application/json" \
    -d '{"username":"admin","password":"secure123456789"}' | jq -r '.token')

echo "Starting rapid operation test..."
echo "Will test until server crashes..."

for i in {1..100}; do
    echo -n "Request $i: "
    
    # Check if server is running before request
    if ! ps -p $(cat /opt/jdbx/build/var/jdbxd.pid 2>/dev/null) > /dev/null 2>&1; then
        echo "❌ Server already crashed!"
        exit 1
    fi
    
    # Make request
    RESPONSE=$(curl -s -k -X POST "$BASE_URL/api/documents" \
        -H "Authorization: Bearer $TOKEN" \
        -H "Content-Type: application/json" \
        -d "{\"id\":$i,\"data\":\"rapid test $i\"}" \
        -w "HTTP:%{http_code}" \
        --max-time 2 --connect-timeout 1 2>&1)
    
    CODE=$(echo "$RESPONSE" | grep -o "HTTP:[0-9]*" | cut -d':' -f2)
    
    if [ -z "$CODE" ]; then
        echo "NO RESPONSE"
        # Check if server crashed
        if ! ps -p $(cat /opt/jdbx/build/var/jdbxd.pid 2>/dev/null) > /dev/null 2>&1; then
            echo "❌ Server CRASHED after request $i!"
            echo "Last log entries:"
            tail -10 /opt/jdbx/build/var/jdbxd.log
            exit 1
        fi
    else
        echo "HTTP $CODE"
    fi
    
    # Small delay to let server process
    sleep 0.1
done

echo "✅ Server survived all 100 requests!"