#!/bin/bash

# Login and get token
TOKEN=$(curl -k -s -X POST https://localhost:5000/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username": "admin", "password": "admin123"}' | \
  python3 -c "import json, sys; data = json.load(sys.stdin); print(data.get('token', ''))")

if [ -z "$TOKEN" ]; then
  echo "Failed to get auth token"
  exit 1
fi

echo "Token obtained successfully"

# Create a test collection if it doesn't exist
echo "Creating test collection..."
curl -k -s -X POST https://localhost:5000/api/collections \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"name": "test_collection"}'

# Insert a test document
echo -e "\nInserting test document..."
curl -k -s -X POST https://localhost:5000/api/collections/test_collection/documents \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"name": "Test Document", "value": 123}'

# Get collections with counts
echo -e "\nCollections response:"
curl -k -s -H "Authorization: Bearer $TOKEN" https://localhost:5000/api/collections | python3 -m json.tool

rm -f /opt/jsondb/test_create_doc.sh