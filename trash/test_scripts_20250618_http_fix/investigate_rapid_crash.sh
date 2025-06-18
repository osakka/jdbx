#!/bin/bash

BASE_URL="https://localhost:5000"

# Get auth token
TOKEN=$(curl -s -k "$BASE_URL/api/auth/login" \
    -H "Content-Type: application/json" \
    -d '{"username":"admin","password":"secure123456789"}' | jq -r '.token')

echo "🔍 Investigating Rapid Operations Crash"
echo "======================================"

# Test 1: Gradual increase in speed
echo -e "\nTest 1: Gradual speed increase"
for delay in 0.5 0.2 0.1 0.05 0.01 0; do
    echo -e "\nTesting with ${delay}s delay between requests..."
    
    for i in {1..10}; do
        if ! ps -p $(cat /opt/jdbx/build/var/jdbxd.pid 2>/dev/null) > /dev/null 2>&1; then
            echo "❌ Server crashed!"
            exit 1
        fi
        
        curl -s -k -X POST "$BASE_URL/api/documents" \
            -H "Authorization: Bearer $TOKEN" \
            -H "Content-Type: application/json" \
            -d "{\"test\":\"speed\",\"delay\":\"$delay\",\"seq\":$i}" \
            -w "." -o /dev/null
        
        [ "$delay" != "0" ] && sleep $delay
    done
    echo " ✅"
done

# Test 2: Concurrent requests
echo -e "\nTest 2: Concurrent requests"
for concurrent in 2 5 10 20; do
    echo -e "\nTesting $concurrent concurrent requests..."
    
    if ! ps -p $(cat /opt/jdbx/build/var/jdbxd.pid 2>/dev/null) > /dev/null 2>&1; then
        echo "❌ Server already crashed!"
        exit 1
    fi
    
    # Launch concurrent requests
    for i in $(seq 1 $concurrent); do
        curl -s -k -X POST "$BASE_URL/api/documents" \
            -H "Authorization: Bearer $TOKEN" \
            -H "Content-Type: application/json" \
            -d "{\"test\":\"concurrent\",\"batch\":$concurrent,\"req\":$i}" \
            -o /dev/null &
    done
    
    # Wait for all to complete
    wait
    
    if ps -p $(cat /opt/jdbx/build/var/jdbxd.pid 2>/dev/null) > /dev/null 2>&1; then
        echo "✅ Server survived $concurrent concurrent requests"
    else
        echo "❌ Server crashed with $concurrent concurrent requests!"
        exit 1
    fi
    
    sleep 1  # Brief pause between batches
done

# Test 3: Specific pattern from discovery test
echo -e "\nTest 3: Discovery test pattern (rapid sequential)"
for i in {1..50}; do
    if ! ps -p $(cat /opt/jdbx/build/var/jdbxd.pid 2>/dev/null) > /dev/null 2>&1; then
        echo -e "\n❌ Server crashed at request $i!"
        exit 1
    fi
    
    # Exactly as discovery test does it
    curl -s -k -X POST "$BASE_URL/api/documents" \
        -H "Authorization: Bearer $TOKEN" \
        -H "Content-Type: application/json" \
        -d "{\"id\":$i,\"data\":\"rapid test $i\"}" \
        -w "" -o /dev/null
    
    echo -n "."
    
    [ $((i % 10)) -eq 0 ] && echo -n " ($i)"
done

echo -e "\n✅ All tests completed successfully!"