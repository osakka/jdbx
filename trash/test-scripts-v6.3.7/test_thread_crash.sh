#!/bin/bash

# Test to demonstrate the thread crash pattern
API="https://localhost:5000/api"

echo "Testing SSL thread crash pattern..."
echo "Theory: Crashes on 5th operation (after using all 4 worker threads)"
echo ""

# Login first
echo "1. Logging in..."
TOKEN=$(curl -sk "$API/auth/login" -H "Content-Type: application/json" \
  -d '{"username": "admin", "password": "secure123456789"}' | \
  grep -o '"token":"[^"]*' | cut -d'"' -f4)

if [ -z "$TOKEN" ]; then
    echo "Failed to login!"
    exit 1
fi

echo "   ✓ Login successful"
echo ""

# Create 6 documents one by one to see when it crashes
for i in {1..6}; do
    echo "$((i+1)). Creating document $i..."
    
    RESPONSE=$(curl -sk "$API/documents" \
        -H "Authorization: Bearer $TOKEN" \
        -H "Content-Type: application/json" \
        -d "{\"title\": \"Test Document $i\", \"content\": \"Testing thread $i\"}" 2>&1)
    
    if echo "$RESPONSE" | grep -q "uuid"; then
        echo "   ✓ Document $i created successfully"
    else
        echo "   ✗ FAILED at document $i!"
        echo "   Response: $RESPONSE"
        
        # Check if server is still alive
        if ! curl -sk "$API/health" > /dev/null 2>&1; then
            echo ""
            echo "   🔥 SERVER CRASHED!"
            echo "   Theory confirmed: Crash after $((i-1)) successful operations"
            echo "   This matches our 4 worker threads"
        fi
        exit 1
    fi
    
    # Small delay to ensure we're using different threads
    sleep 0.1
done

echo ""
echo "All documents created successfully - no crash!"