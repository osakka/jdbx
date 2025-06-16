#!/bin/bash

# Debug document storage issue

TOKEN=$(cat /tmp/token.txt)
HOST="https://localhost:5000"

echo "=== Testing Document Storage ==="

# 1. Get current library context
echo -e "\n1. Checking session library:"
CURRENT_LIB=$(curl -sk "$HOST/api/libraries" -H "Authorization: Bearer $TOKEN" | jq -r '.libraries[0].name')
echo "Current library: $CURRENT_LIB"

# 2. Create document with explicit library
echo -e "\n2. Creating document in default library:"
CREATE_RESULT=$(curl -sk -X POST "$HOST/api/collections/test_products/documents" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d '{"name": "Debug Widget", "price": 99.99, "library": "default"}')
echo "$CREATE_RESULT" | jq .

DOC_ID=$(echo "$CREATE_RESULT" | jq -r '.uuid')
echo "Created document ID: $DOC_ID"

# 3. Try different query methods
echo -e "\n3. Query methods:"

echo -e "\n3a. Query collection directly:"
curl -sk "$HOST/api/collections/test_products/documents" \
    -H "Authorization: Bearer $TOKEN" | jq .

echo -e "\n3b. Query with library prefix:"
curl -sk "$HOST/api/collections/default/test_products/documents" \
    -H "Authorization: Bearer $TOKEN" | jq .

echo -e "\n3c. Get document by ID:"
curl -sk "$HOST/api/collections/test_products/documents/$DOC_ID" \
    -H "Authorization: Bearer $TOKEN" | jq .

# 4. Check unified documents
echo -e "\n4. Check raw documents collection:"
curl -sk "$HOST/api/collections/documents/documents?query={\"type\":\"data\"}" \
    -H "Authorization: Bearer $TOKEN" | jq .

echo -e "\n5. List all collections in all libraries:"
curl -sk "$HOST/api/collections" \
    -H "Authorization: Bearer $TOKEN" | jq '.collections[] | {name, library}' 2>/dev/null | head -20