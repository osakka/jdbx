#!/bin/bash

echo "=== Simple Server Test ==="
echo

# First restart the server
echo "Restarting server..."
/opt/jsondb/build/jsondb_runtime.sh stop > /dev/null 2>&1
sleep 2
/opt/jsondb/build/jsondb_runtime.sh start
sleep 3

echo
echo "1. Testing single request:"
time curl -s http://localhost:5000/api/health | jq -c '{status, uptime}'

echo
echo "2. Testing 5 sequential requests:"
for i in {1..5}; do
    echo -n "Request $i: "
    time curl -s -o /dev/null -w "%{http_code}\n" http://localhost:5000/api/collections
done

echo
echo "3. Testing 5 concurrent requests:"
for i in {1..5}; do
    (curl -s http://localhost:5000/api/collections > /dev/null && echo "Request $i: OK") &
done
wait

echo
echo "4. Testing 20 concurrent requests:"
success=0
for i in {1..20}; do
    (curl -s --max-time 5 http://localhost:5000/api/health > /dev/null 2>&1 && echo "OK" || echo "FAIL") &
done | grep -c "OK"

echo
echo "5. Checking server status after tests:"
curl -s http://localhost:5000/api/health | jq -c '{status, metrics}'

echo
echo "=== Test Complete ==="