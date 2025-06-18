#!/bin/bash

# Test with exact sizes to understand the issue

TOKEN=$(curl -s -k https://localhost:5000/api/auth/login \
    -H "Content-Type: application/json" \
    -d '{"username":"admin","password":"secure123456789"}' | jq -r '.token')

echo "Testing different document sizes..."

# Test 1: 4000 bytes (should work)
echo -n "Test 1 (4000 bytes): "
DOC='{"data":"'
for i in {1..3990}; do DOC="${DOC}x"; done
DOC="${DOC}\"}"
RESPONSE=$(curl -s -k -w " HTTP %{http_code}" \
    -X POST https://localhost:5000/api/documents \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d "$DOC" 2>&1 | grep "HTTP")
echo "$RESPONSE"

# Test 2: 4095 bytes 
echo -n "Test 2 (4095 bytes): "
DOC='{"data":"'
for i in {1..4085}; do DOC="${DOC}x"; done
DOC="${DOC}\"}"
RESPONSE=$(curl -s -k -w " HTTP %{http_code}" \
    -X POST https://localhost:5000/api/documents \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d "$DOC" 2>&1 | grep "HTTP")
echo "$RESPONSE"

# Test 3: 4096 bytes 
echo -n "Test 3 (4096 bytes): "
DOC='{"data":"'
for i in {1..4086}; do DOC="${DOC}x"; done
DOC="${DOC}\"}"
RESPONSE=$(curl -s -k -w " HTTP %{http_code}" \
    -X POST https://localhost:5000/api/documents \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d "$DOC" 2>&1 | grep "HTTP")
echo "$RESPONSE"

# Test 4: 4097 bytes (should trigger reallocation)
echo -n "Test 4 (4097 bytes): "
DOC='{"data":"'
for i in {1..4087}; do DOC="${DOC}x"; done
DOC="${DOC}\"}"
RESPONSE=$(curl -s -k -w " HTTP %{http_code}" \
    -X POST https://localhost:5000/api/documents \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d "$DOC" 2>&1 | grep "HTTP")
echo "$RESPONSE"