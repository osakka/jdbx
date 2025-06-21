#!/bin/bash

BASE_URL="https://localhost:5000"
TOKEN=$(curl -s -k -X POST "$BASE_URL/api/auth/login" -H "Content-Type: application/json" -d '{"username":"admin","password":"secure123456789"}' | jq -r '.token')

echo "=== Testing simple GET /api/documents ==="
timeout 10 curl -s -k -X GET "$BASE_URL/api/documents" -H "Authorization: Bearer $TOKEN" | head -200

echo -e "\n=== Testing simple POST query ==="
timeout 10 curl -s -k -X POST "$BASE_URL/api/documents/query" -H "Authorization: Bearer $TOKEN" -H "Content-Type: application/json" -d '{}' | head -200