#!/bin/bash

echo "Testing basic login..."

# Test with verbose output
curl -v -k https://localhost:5000/api/auth/login \
    -H "Content-Type: application/json" \
    -d '{"username":"admin","password":"secure123456789"}' 2>&1 | grep -E "(Content-Length|HTTP/|400|connected|SSL)"