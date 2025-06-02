#!/bin/bash

echo "=== Connection Pattern Testing ==="
echo

# Test 1: Keep-alive connections
echo "1. Testing persistent connections (keep-alive):"
for i in {1..5}; do
    (
        curl -s -H "Connection: keep-alive" \
            http://localhost:5000/api/health \
            http://localhost:5000/api/collections \
            http://localhost:5000/api/metrics > /dev/null && echo "Keep-alive $i: OK"
    ) &
done
wait

echo
echo "2. Testing rapid connection churn (open/close):"
for i in {1..20}; do
    curl -s -H "Connection: close" http://localhost:5000/api/health > /dev/null && echo -n "." || echo -n "X"
done
echo

echo
echo "3. Testing mixed static and API requests:"
for i in {1..10}; do
    (
        # Mix of static files and API calls
        curl -s http://localhost:5000/ > /dev/null && echo -n "S"
        curl -s http://localhost:5000/api/collections > /dev/null && echo -n "A"
        curl -s http://localhost:5000/js/app.js > /dev/null && echo -n "J"
    ) &
done
wait
echo

echo
echo "4. Testing large response handling:"
# Create a collection with many documents
curl -s -X POST http://localhost:5000/api/collections \
    -H "Content-Type: application/json" \
    -d '{"name": "test_large"}' > /dev/null

# Add 50 documents
for i in {1..50}; do
    curl -s -X POST http://localhost:5000/api/collections/test_large/documents \
        -H "Content-Type: application/json" \
        -d "{\"index\": $i, \"data\": \"Test document $i\"}" > /dev/null
done

# Now fetch all documents (large response)
echo -n "Fetching 50 documents: "
time curl -s http://localhost:5000/api/collections/test_large/documents > /dev/null && echo "OK" || echo "FAILED"

# Cleanup
curl -s -X DELETE http://localhost:5000/api/collections/test_large > /dev/null

echo
echo "5. Testing error conditions:"
echo -n "Invalid JSON: "
curl -s -X POST http://localhost:5000/api/collections \
    -H "Content-Type: application/json" \
    -d '{invalid}' -w "%{http_code}\n" -o /dev/null

echo -n "Large header (8KB): "
large_header=$(python3 -c "print('X' * 8192)")
curl -s -H "X-Large-Header: $large_header" \
    http://localhost:5000/api/health -w "%{http_code}\n" -o /dev/null

echo -n "Non-existent endpoint: "
curl -s http://localhost:5000/api/nonexistent -w "%{http_code}\n" -o /dev/null

echo
echo "Final server check:"
curl -s http://localhost:5000/api/health | jq -c '{status, uptime, metrics}'

echo
echo "=== Test Complete ==="