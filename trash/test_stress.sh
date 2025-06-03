#!/bin/bash

echo "=== Stress Test to Reproduce Hanging ==="
echo

# Function to send continuous requests
stress_test() {
    local duration=$1
    local connections=$2
    echo "Running stress test for ${duration}s with ${connections} concurrent connections..."
    
    end_time=$(($(date +%s) + duration))
    request_count=0
    
    while [ $(date +%s) -lt $end_time ]; do
        for i in $(seq 1 $connections); do
            (
                # Mix of requests
                case $((i % 4)) in
                    0) curl -s http://localhost:5000/api/health > /dev/null ;;
                    1) curl -s http://localhost:5000/api/collections > /dev/null ;;
                    2) curl -s http://localhost:5000/ > /dev/null ;;
                    3) curl -s http://localhost:5000/js/app.js > /dev/null ;;
                esac
            ) &
        done
        
        wait
        request_count=$((request_count + connections))
        echo -n "."
        
        # Check if server is still responding
        if ! curl -s --max-time 2 http://localhost:5000/api/health > /dev/null 2>&1; then
            echo
            echo "Server stopped responding after ~$request_count requests!"
            return 1
        fi
    done
    
    echo
    echo "Completed ~$request_count requests successfully"
    return 0
}

# Monitor server in background
monitor_server() {
    while true; do
        if ! curl -s --max-time 5 http://localhost:5000/api/health > /dev/null 2>&1; then
            echo "[MONITOR] Server not responding at $(date)"
            break
        fi
        sleep 5
    done
}

# Start monitoring
monitor_server &
MONITOR_PID=$!

# Run stress test
stress_test 30 25

# Stop monitor
kill $MONITOR_PID 2>/dev/null

echo
echo "Checking server state after stress test:"
if curl -s --max-time 5 http://localhost:5000/api/health > /dev/null 2>&1; then
    echo "Server is still responsive!"
    curl -s http://localhost:5000/api/health | jq '.metrics'
else
    echo "Server is NOT responding!"
    
    # Check process
    if ps -p $(cat /opt/jsondb/build/var/jsondb.pid 2>/dev/null) > /dev/null 2>&1; then
        echo "Process is still running"
    else
        echo "Process has crashed"
    fi
    
    # Check recent logs
    echo
    echo "Last 20 log lines:"
    tail -20 /opt/jsondb/build/var/jsondb.log
fi

echo
echo "=== Test Complete ==="