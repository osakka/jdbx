#!/bin/bash
# Test JDBX without SSL to isolate N-1 byte issue

echo "🧪 Testing JDBX over plain HTTP (no SSL)"
echo "======================================="

# First, check if server supports HTTP
echo -e "\n1. Testing HTTP endpoint..."
curl -s http://localhost:5000/api/health -w "\nHTTP Status: %{http_code}\n" || echo "HTTP not available"

# If HTTP works, run tests
echo -e "\n2. Testing with SSL disabled in config..."
# This would require server restart with SSL disabled

echo -e "\nConclusion: To properly test JDBX functionality, we need to either:"
echo "1. Fix the N-1 byte SSL issue"
echo "2. Temporarily disable SSL for testing"
echo "3. Use a different HTTP client that works with OpenSSL 3.x"