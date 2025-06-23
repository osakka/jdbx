#!/bin/bash

echo "Testing JSONdb API endpoints..."
echo

# Test health endpoint
echo "1. Testing health endpoint:"
curl -s http://localhost:5000/api/health | jq . || echo "FAILED"
echo

# Test collections endpoint
echo "2. Testing collections endpoint:"
curl -s http://localhost:5000/api/collections | jq . || echo "FAILED"
echo

# Test authentication
echo "3. Testing authentication:"
curl -s -X POST http://localhost:5000/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"admin"}' | jq . || echo "FAILED"
echo

# Test admin API
echo "4. Testing admin API:"
curl -s http://localhost:5000/api/admin/status | jq . || echo "FAILED"
echo

# List all endpoints
echo "5. Available endpoints (checking common paths):"
for path in / /api /api/health /api/collections /api/auth/login /api/admin/status /api/system/info; do
    echo -n "  $path: "
    status=$(curl -s -o /dev/null -w "%{http_code}" http://localhost:5000$path)
    echo "HTTP $status"
done