#!/bin/bash

echo "Testing simple document insertion..."

# Insert a simple test document
RESPONSE=$(curl -s -X POST "http://localhost:5000/api/collections/large_test_data/documents" \
  -H "Content-Type: application/json" \
  -d '{
    "name": "Test Document",
    "type": "test",
    "value": 123,
    "description": "Simple test document to verify insertion works"
  }')

echo "Response: $RESPONSE"

# Check if it was inserted
echo -e "\nChecking collection documents..."
curl -s "http://localhost:5000/api/collections/large_test_data/documents?limit=5" | jq '.'