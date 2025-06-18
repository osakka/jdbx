#!/bin/bash
set -x  # Debug mode

BASE_URL="https://localhost:5000"

# Get token
TOKEN=$(curl -s -k "$BASE_URL/api/auth/login" \
    -H "Content-Type: application/json" \
    -d '{"username":"admin","password":"secure123456789"}' | jq -r '.token')

echo "Token: ${TOKEN:0:20}..."

# Test exactly as discovery script does
echo "Testing 5KB document..."
MEDIUM_CONTENT=$(head -c 5000 < /dev/zero | tr '\0' 'A')
echo "Content length: ${#MEDIUM_CONTENT}"

# Build JSON exactly as discovery script
JSON="{\"title\":\"Medium Document\",\"content\":\"$MEDIUM_CONTENT\"}"
echo "JSON length: ${#JSON}"

# Send request
curl -v -k -X POST "$BASE_URL/api/documents" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d "$JSON" 2>&1 | tail -20

# Check server
sleep 1
if ps -p $(cat /opt/jdbx/build/var/jdbxd.pid) > /dev/null; then
    echo "✅ Server still running"
else
    echo "❌ Server CRASHED!"
fi