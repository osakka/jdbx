#!/bin/bash

echo "=== Load Limit Testing ==="
echo

test_concurrent_load() {
    local count=$1
    echo -n "Testing $count concurrent requests: "
    
    success=0
    fail=0
    
    # Create temp file for results
    temp_file=$(mktemp)
    
    # Launch concurrent requests
    for i in $(seq 1 $count); do
        (
            if curl -s --max-time 10 http://localhost:5000/api/health > /dev/null 2>&1; then
                echo "OK" >> "$temp_file"
            else
                echo "FAIL" >> "$temp_file"
            fi
        ) &
    done
    
    # Wait for all requests to complete
    wait
    
    # Count results
    if [ -f "$temp_file" ]; then
        success=$(grep -c "OK" "$temp_file" 2>/dev/null || echo 0)
        fail=$(grep -c "FAIL" "$temp_file" 2>/dev/null || echo 0)
        rm -f "$temp_file"
    fi
    
    echo "Success: $success, Failed: $fail"
    
    # Check if server is still responsive
    if ! curl -s --max-time 5 http://localhost:5000/api/health > /dev/null 2>&1; then
        echo "  WARNING: Server not responding after test!"
        return 1
    fi
    
    return 0
}

# Test increasing loads
for load in 10 20 30 40 50 75 100; do
    if ! test_concurrent_load $load; then
        echo "Server failed at $load concurrent connections"
        break
    fi
    sleep 2  # Give server time to recover between tests
done

echo
echo "Checking final server state:"
curl -s --max-time 5 http://localhost:5000/api/health | jq -c 'select(. != null) // "Server not responding"'

echo
echo "=== Test Complete ==="