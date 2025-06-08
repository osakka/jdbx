#!/bin/bash

echo "=== Connection Leak Stress Test ==="
echo

initial=$(curl -s http://localhost:5000/api/health | jq -r '.metrics.performance.active_connections')
echo "Initial active connections: $initial"

echo -e "\nRunning 3 rounds of 20 parallel connections..."
for round in 1 2 3; do
    echo -e "\nRound $round:"
    
    # Launch 20 parallel connections
    for i in {1..20}; do
        (
            # Each connection makes 3 requests with keep-alive
            for j in {1..3}; do
                curl -s -H "Connection: keep-alive" http://localhost:5000/api/health > /dev/null
                sleep 0.05
            done
            # Last request closes connection
            curl -s -H "Connection: close" http://localhost:5000/api/health > /dev/null
        ) &
    done
    
    # Monitor during round
    sleep 1
    during=$(curl -s http://localhost:5000/api/health | jq -r '.metrics.performance.active_connections')
    echo "  During round: $during connections"
    
    # Wait for all to complete
    wait
    
    # Check after round
    sleep 2
    after=$(curl -s http://localhost:5000/api/health | jq -r '.metrics.performance.active_connections')
    echo "  After round: $after connections"
done

echo -e "\nWaiting 10 seconds for all timeouts..."
sleep 10

final=$(curl -s http://localhost:5000/api/health | jq -r '.metrics.performance.active_connections')
echo -e "\nFinal active connections: $final"

if [ "$final" -eq "$initial" ]; then
    echo "✓ SUCCESS: No connection leak detected!"
else
    echo "✗ LEAK DETECTED: Started with $initial, ended with $final (difference: $((final - initial)))"
fi

echo -e "\n=== Metric Summary ==="
echo "Increments: $(grep -c 'Incremented active connections' /opt/jsondb/build/var/jsondb.log)"
echo "Decrements: $(grep -c 'Decremented active connections' /opt/jsondb/build/var/jsondb.log)"