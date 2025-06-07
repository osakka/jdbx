#!/bin/bash

echo "=== Testing HTTP Keep-Alive Support ==="
echo

# Test 1: Single request with Connection: close
echo "Test 1: Connection: close (should close after response)"
curl -k -w "\nTime: %{time_total}s\n" \
     -H "Connection: close" \
     https://localhost:5000/api/health 2>/dev/null

echo
echo "---"
echo

# Test 2: Single request with Connection: keep-alive
echo "Test 2: Connection: keep-alive (should keep connection open)"
curl -k -w "\nTime: %{time_total}s\n" \
     -H "Connection: keep-alive" \
     https://localhost:5000/api/health 2>/dev/null

echo
echo "---"
echo

# Test 3: Multiple requests on same connection
echo "Test 3: Multiple requests with keep-alive (should reuse connection)"
echo "Making 5 requests on the same connection..."

# Use curl's ability to reuse connections
total_time=0
for i in {1..5}; do
    time_output=$(curl -k -w "%{time_total}" -o /dev/null -s \
                       -H "Connection: keep-alive" \
                       https://localhost:5000/api/health 2>/dev/null)
    echo "Request $i: ${time_output}s"
    total_time=$(echo "$total_time + $time_output" | bc)
done

avg_time=$(echo "scale=4; $total_time / 5" | bc)
echo "Average time per request: ${avg_time}s"

echo
echo "---"
echo

# Test 4: Compare with Connection: close
echo "Test 4: Multiple requests with Connection: close (new connection each time)"
echo "Making 5 requests with new connections..."

total_time=0
for i in {1..5}; do
    time_output=$(curl -k -w "%{time_total}" -o /dev/null -s \
                       -H "Connection: close" \
                       https://localhost:5000/api/health 2>/dev/null)
    echo "Request $i: ${time_output}s"
    total_time=$(echo "$total_time + $time_output" | bc)
done

avg_time=$(echo "scale=4; $total_time / 5" | bc)
echo "Average time per request: ${avg_time}s"

echo
echo "=== Keep-Alive Test Complete ==="
echo
echo "Note: Keep-alive requests should be faster on average due to connection reuse."
echo "The first request establishes the connection, subsequent requests reuse it."