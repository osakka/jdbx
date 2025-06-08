#!/bin/bash

# Login and get token
TOKEN=$(curl -s -X POST http://localhost:5000/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username": "admin", "password": "admin123"}' | \
  python3 -c "import json, sys; print(json.load(sys.stdin).get('token', ''))")

if [ -z "$TOKEN" ]; then
  echo "Failed to get auth token"
  exit 1
fi

echo "Token obtained successfully"

# Get collections
echo "Collections response:"
curl -s -H "Authorization: Bearer $TOKEN" http://localhost:5000/api/collections | python3 -m json.tool

# Clean up
rm -f /opt/jsondb/test_collections_api.sh