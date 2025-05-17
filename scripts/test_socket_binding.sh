#!/bin/bash

# Script to test socket binding capabilities in the current environment

echo "JSONdb Socket Binding Test Utility"
echo "=================================="
echo

# Check if the socket_test program exists, if not build it
if [ ! -f "../src/socket_test" ]; then
    echo "Building socket test program..."
    cd ../src
    gcc -o socket_test socket_test.c
    cd - > /dev/null
fi

# Test a range of ports
echo "Testing socket binding on various ports..."
echo

# Array of ports to test
PORTS=(5000 8080 3000 9000 1234)

for PORT in "${PORTS[@]}"; do
    echo "Testing port $PORT..."
    
    # Try to bind to the port
    ../src/socket_test $PORT &
    TEST_PID=$!
    
    # Wait briefly for the program to start
    sleep 1
    
    # Check if the process is still running
    if ps -p $TEST_PID > /dev/null; then
        echo "  ✓ Successfully bound to port $PORT"
        
        # Check if the port is visible in netstat/ss
        if netstat -tuln | grep -q ":$PORT " || ss -tuln | grep -q ":$PORT "; then
            echo "  ✓ Port $PORT is visible in netstat/ss"
        else
            echo "  ✗ Port $PORT is NOT visible in netstat/ss"
        fi
        
        # Try to connect to the port with curl
        if curl -s -m 1 localhost:$PORT > /dev/null; then
            echo "  ✓ Successfully connected to port $PORT with curl"
        else
            echo "  ✗ Failed to connect to port $PORT with curl"
        fi
        
        # Kill the test process
        kill $TEST_PID 2>/dev/null
    else
        echo "  ✗ Failed to bind to port $PORT"
    fi
    
    echo
done

echo "Test complete. See above for results."
echo 
echo "If binding succeeds but connections fail, this indicates"
echo "network restrictions in your container/VM environment."