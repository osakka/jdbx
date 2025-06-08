#!/bin/bash

# Test script to verify connection leak fix

echo "Testing connection leak fix..."
echo "Initial state:"
curl -s http://localhost:5000/api/health 2>/dev/null | jq -r '.metrics.performance.active_connections' | xargs echo "Active connections:"

echo -e "\nStarting 10 parallel connections with keep-alive..."
for i in {1..10}; do
    (
        # Make 5 requests per connection with keep-alive
        for j in {1..5}; do
            curl -s -H "Connection: keep-alive" http://localhost:5000/api/health > /dev/null
            sleep 0.1
        done
    ) &
done

# Monitor active connections
echo -e "\nMonitoring active connections during test:"
for i in {1..20}; do
    active=$(curl -s http://localhost:5000/api/health 2>/dev/null | jq -r '.metrics.performance.active_connections')
    echo "[$i] Active connections: $active"
    sleep 0.5
done

# Wait for all background jobs
wait

echo -e "\nWaiting for connections to close..."
sleep 2

echo -e "\nFinal state:"
curl -s http://localhost:5000/api/health 2>/dev/null | jq -r '.metrics.performance.active_connections' | xargs echo "Active connections:"

echo -e "\nTest complete."