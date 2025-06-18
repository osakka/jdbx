#!/bin/bash

BASE_URL="https://localhost:5000"

# Get token
TOKEN=$(curl -s -k "$BASE_URL/api/auth/login" \
    -H "Content-Type: application/json" \
    -d '{"username":"admin","password":"secure123456789"}' | jq -r '.token')

echo "Token: ${TOKEN:0:20}..."

# Create 5KB document in a file
MEDIUM_CONTENT=$(head -c 5000 < /dev/zero | tr '\0' 'A')
cat > /tmp/5kb_test.json <<EOF
{"title":"Medium Document","content":"$MEDIUM_CONTENT"}
EOF

echo "File size: $(stat -c%s /tmp/5kb_test.json) bytes"
echo "File content sample: $(head -c 100 /tmp/5kb_test.json)..."

# Method 1: Using -d @file
echo -e "\nMethod 1: curl -d @file"
curl -v -k -X POST "$BASE_URL/api/documents" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d @/tmp/5kb_test.json 2>&1 | grep -E "(Content-Length:|< HTTP|400 Bad)"

# Method 2: Using --data-binary
echo -e "\nMethod 2: curl --data-binary @file"
curl -v -k -X POST "$BASE_URL/api/documents" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    --data-binary @/tmp/5kb_test.json 2>&1 | grep -E "(Content-Length:|< HTTP|201 Created)"

# Check server
if ps -p $(cat /opt/jdbx/build/var/jdbxd.pid) > /dev/null; then
    echo -e "\n✅ Server still running"
else
    echo -e "\n❌ Server crashed!"
fi