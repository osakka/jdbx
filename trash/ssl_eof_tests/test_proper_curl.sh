#!/bin/bash

TOKEN=$(curl -s -k https://localhost:5000/api/auth/login -H "Content-Type: application/json" -d '{"username":"admin","password":"secure123456789"}' | jq -r '.token')

echo "Testing with proper curl method..."

# Create a 5KB document
python3 -c 'import json; print(json.dumps({"data": "x" * 5000}))' > /tmp/test.json

SIZE=$(wc -c < /tmp/test.json)
echo "File size: $SIZE bytes"

# Test with file
echo -e "\nMethod 1: Using file"
curl -s -k -w "\nHTTP %{http_code}\n" \
    -X POST https://localhost:5000/api/documents \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d @/tmp/test.json

# Test with --data-binary
echo -e "\nMethod 2: Using --data-binary with file"
curl -s -k -w "\nHTTP %{http_code}\n" \
    -X POST https://localhost:5000/api/documents \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    --data-binary @/tmp/test.json