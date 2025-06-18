#!/bin/bash

# Simple test for medium document
TOKEN=$(curl -s -k https://localhost:5000/api/auth/login \
    -H "Content-Type: application/json" \
    -d '{"username":"admin","password":"secure123456789"}' | jq -r '.token')

echo "Token: $TOKEN"

# Create a 5KB JSON document
DOC='{"data":"'
for i in {1..5000}; do
    DOC="${DOC}x"
done
DOC="${DOC}\"}"

echo "Document size: ${#DOC} bytes"

# Send it
curl -v -k -X POST https://localhost:5000/api/documents \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d "$DOC" 2>&1 | grep -E "(HTTP|Content-Length:|< HTTP|400|Error)"