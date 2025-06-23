#!/bin/bash

# Test unique constraints on users collection

echo "Testing unique constraints on users collection..."

# First, create metadata for users collection with unique constraints
echo "1. Creating users collection metadata with unique constraints..."
curl -k -s -X POST https://localhost:5000/api/collections/users/documents \
  -H "Content-Type: application/json" \
  -d '{
    "_id": "_meta",
    "permissions": {
      "owner_perms": "rwxda",
      "world_perms": "r",
      "owners": ["system"]
    },
    "schema": {
      "type": "object",
      "required": ["username", "email"],
      "properties": {
        "username": {"type": "string", "minLength": 3},
        "email": {"type": "string", "format": "email"}
      }
    },
    "indexes": [
      {"field": "username", "type": "hash", "unique": true},
      {"field": "email", "type": "hash", "unique": true}
    ]
  }' | jq

# Create first user - should succeed
echo -e "\n2. Creating first user (should succeed)..."
curl -k -s -X POST https://localhost:5000/api/collections/users/documents \
  -H "Content-Type: application/json" \
  -d '{
    "username": "testuser1",
    "email": "test1@example.com",
    "name": "Test User 1"
  }' | jq

# Try to create duplicate username - should fail
echo -e "\n3. Creating user with duplicate username (should fail)..."
curl -k -s -X POST https://localhost:5000/api/collections/users/documents \
  -H "Content-Type: application/json" \
  -d '{
    "username": "testuser1",
    "email": "different@example.com",
    "name": "Different User"
  }' | jq

# Try to create duplicate email - should fail
echo -e "\n4. Creating user with duplicate email (should fail)..."
curl -k -s -X POST https://localhost:5000/api/collections/users/documents \
  -H "Content-Type: application/json" \
  -d '{
    "username": "testuser2",
    "email": "test1@example.com",
    "name": "Test User 2"
  }' | jq

# Create second user with unique values - should succeed
echo -e "\n5. Creating second user with unique values (should succeed)..."
curl -k -s -X POST https://localhost:5000/api/collections/users/documents \
  -H "Content-Type: application/json" \
  -d '{
    "username": "testuser2",
    "email": "test2@example.com",
    "name": "Test User 2"
  }' | jq

# List all users to verify
echo -e "\n6. Listing all users..."
curl -k -s https://localhost:5000/api/collections/users/documents | jq '.documents | map({username, email})'