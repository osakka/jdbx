#!/bin/bash

# Enable debug logging
export JDBX_LOG_LEVEL=debug

# Restart server with debug logging
cd /opt/jdbx && build/jdbx_runtime.sh stop
JDBX_BOOTSTRAP_ADMIN_USER=admin JDBX_BOOTSTRAP_ADMIN_PASS=secure123456789 JDBX_LOG_LEVEL=debug build/jdbx_runtime.sh start

sleep 2

# Get token
TOKEN=$(curl -s -k https://localhost:5000/api/auth/login \
    -H "Content-Type: application/json" \
    -d '{"username":"admin","password":"secure123456789"}' | jq -r '.token')

# Send a 5KB document and check logs
DOC='{"data":"'
for i in {1..5000}; do
    DOC="${DOC}x"
done
DOC="${DOC}\"}"

echo "Sending document of size: ${#DOC} bytes"
curl -s -k -X POST https://localhost:5000/api/documents \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d "$DOC" > /dev/null

# Check debug logs
echo -e "\nRelevant debug logs:"
tail -30 /opt/jdbx/build/var/jdbxd.log | grep -E "(Reallocating|Need to read|Body read attempt|buffer_size|required_size|INCOMPLETE)" | tail -20